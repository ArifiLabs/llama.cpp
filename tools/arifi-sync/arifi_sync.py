#!/usr/bin/env python3
"""arifi-sync - currency, patch-series and upstream-bump driver for the ArifiLabs llama.cpp fork.

Design rule for every subcommand in this file: FAIL LOUDLY AND STOP. Never auto-resolve a
conflict, never skip a hunk, never continue past a failure, never report a number that was
not produced by the run you are looking at.

    python tools/arifi-sync/arifi_sync.py <subcommand> [options]

Subcommands
    status        one-shot overview: base, series, provenance, currency, recipe
    provenance    audit Taken-from:/Origin:/Measured-effect: over the whole series
    series regen  regenerate patches/series from git (the series is NEVER hand-edited)
    series check  integrity: regenerate to a temp dir, byte-compare, and prove every patch is COMMITTED
    series replay replay the series onto a ref with git am; on failure name patch + file + hunk
    currency      fetch every source and report drift against the reviewed pins
    recipe diff   recover the build recipe by DIFFING CMakeCache against the committed snapshot
    recipe export write a CMake -C initial-cache file that replays the recorded configuration
    set-base-pin  move base.upstream_sha + base.upstream_tag in sources.json as one checked,
                  atomic write; refuses unless the pin on disk is the one --expect-sha names
    bump          replay + build + judge onto a new upstream ref; refuses unless ALL THREE pass
    protected-win validate   check the protected-win manifest is complete and still true of the tree
    protected-win check      FAIL-CLOSED ingest preflight: refuse an incoming change that touches a
                             protected win without a recorded, evidence-backed comparison verdict
    protected-win discover   list commits with a real Measured-effect that no manifest entry covers
    protected-win replay-map map REBASED commits back to their registered originals, one-to-one,
                             on retained replay evidence and an equivalent patch. Carries an
                             existing registration across a rewrite; never creates one.

Exit code is 0 only when the subcommand fully passed.
"""
from __future__ import annotations

import argparse
import datetime as _dt
import filecmp
import getpass
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CONFIG = os.path.join(HERE, "sources.json")


# ---------------------------------------------------------------- infrastructure

class Loud(Exception):
    """A loud, stopping failure. Never caught to continue; only to print and exit."""


def say(msg: str) -> None:
    print(msg, flush=True)


def step(msg: str) -> None:
    print("\n==> %s" % msg, flush=True)


def git(repo: str, *args: str, check: bool = True, binary: bool = False, env=None):
    p = subprocess.run(["git", "-C", repo, *args],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    if check and p.returncode != 0:
        raise Loud("git %s failed in %s\n%s" %
                   (" ".join(args), repo, p.stderr.decode("utf-8", "replace")))
    out = p.stdout if binary else p.stdout.decode("utf-8", "replace")
    return p.returncode, out, p.stderr.decode("utf-8", "replace")


def gout(repo: str, *args: str) -> str:
    return git(repo, *args)[1].strip()


def load_config(path: str) -> dict:
    # A broken config used to reach the operator as a raw JSONDecodeError traceback from inside
    # `main`, before any subcommand ran. It is a Loud stop now: same information, in this tool's
    # own voice, and every subcommand - not just the one that writes this file - stops rather than
    # crashes on it.
    with open(path, encoding="utf-8") as fh:
        try:
            return json.load(fh)
        except ValueError as e:
            raise Loud("%s is not parseable JSON (%s). Repair it by hand; nothing else here can\n"
                       "act on a config it cannot read." % (path, e))


def repo_root(start: str) -> str:
    rc, out, _ = git(start, "rev-parse", "--show-toplevel", check=False)
    if rc != 0:
        raise Loud("not inside a git repository: %s" % start)
    return out.strip()


def read_state(repo: str, cfg: dict) -> dict:
    p = os.path.join(repo, cfg["state_file"])
    if not os.path.exists(p):
        return {}
    with open(p, encoding="utf-8") as fh:
        return json.load(fh)


def write_state(repo: str, cfg: dict, state: dict) -> None:
    p = os.path.join(repo, cfg["state_file"])
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as fh:
        json.dump(state, fh, indent=2, sort_keys=True)
        fh.write("\n")


def now_iso() -> str:
    return _dt.datetime.now(_dt.timezone.utc).replace(microsecond=0).isoformat()


# ---------------------------------------------------------------- provenance

TRAILERS = ("Taken-from", "Origin", "Measured-effect")


def commit_trailers(repo: str, sha: str) -> dict:
    out = {}
    for key in TRAILERS:
        out[key] = gout(repo, "log", "-1",
                        "--format=%%(trailers:key=%s,valueonly,separator=%%x2C)" % key, sha)
    return out


def load_grandfather(repo: str) -> set:
    """Sha-pinned exemption ledger for fork-NATIVE commits that predate the
    'Origin: ArifiLabs (native)' convention (2026-08-20). These commits have no
    upstream origin - requiring Taken-from on them was a category error, and
    rewriting 289 commits of history to add trailers would break every series
    pin and evidence citation. The ledger is CLOSED: new native commits carry
    the native Origin trailer instead and must never be added here."""
    p = os.path.join(repo, "tools", "arifi-sync", "native-grandfather.json")
    if not os.path.exists(p):
        return set()
    with open(p, "r", encoding="utf-8-sig") as fh:
        data = json.load(fh)
    return {row["sha"] for row in data.get("commits", [])}


def load_trailer_exemptions(repo: str) -> dict:
    """Sha-pinned exemptions for commits whose provenance is PRESENT and human-readable but
    does not satisfy --strict, and whose only repair would be amending published, tagged,
    series-pinned history. Distinct from `native-grandfather.json` (CLOSED, pre-convention
    native commits) and from `pending-trailers.json` (debt owed by UNMERGED branches).

    A row is NOT a repair. `cmd_provenance` counts these separately and says so on the exit
    line, so an exempted commit can never be read as a fixed one."""
    p = os.path.join(repo, "tools", "arifi-sync", "trailer-exemptions.json")
    if not os.path.exists(p):
        return {}
    with open(p, "r", encoding="utf-8-sig") as fh:
        data = json.load(fh)
    return {row["sha"]: row for row in data.get("commits", [])}


def has_loose_provenance(repo: str, sha: str) -> bool:
    """The LOOSE test: the token appears anywhere in the message. Deliberately the loose one -
    it is what `cmd_provenance` uses to decide `NO PROVENANCE AT ALL`, the only provenance
    condition that is a hard failure. Shared with `regen_preflight` so the pre-flight and the
    report can never disagree about which commits are offenders."""
    body = gout(repo, "log", "-1", "--format=%B", sha)
    return ("Taken-from:" in body) or ("Origin:" in body)


def cmd_provenance(repo: str, cfg: dict, args) -> int:
    base = cfg["base"]["upstream_sha"]
    rng = "%s..%s" % (base, args.ref)
    shas = gout(repo, "rev-list", "--reverse", rng).split()
    if not shas:
        raise Loud("empty range %s - is the base pin correct?" % rng)

    grandfathered = load_grandfather(repo)
    exempt = load_trailer_exemptions(repo)
    strict, loose, meas, native, gf = 0, 0, 0, 0, 0
    loose_only, missing, no_meas = [], [], []
    ex_loose, ex_meas = [], []
    for sha in shas:
        body = gout(repo, "log", "-1", "--format=%B", sha)
        subject = gout(repo, "log", "-1", "--format=%s", sha)
        tr = commit_trailers(repo, sha)
        is_native = "arifilabs (native)" in tr["Origin"].lower()
        is_strict = bool(tr["Taken-from"] or tr["Origin"])
        is_loose = ("Taken-from:" in body) or ("Origin:" in body)
        strict += is_strict
        loose += is_loose
        native += is_native
        if tr["Measured-effect"]:
            meas += 1
        elif not (is_native or sha in grandfathered):
            if sha in exempt:
                ex_meas.append((sha[:9], subject))
            else:
                no_meas.append((sha[:9], subject))
        if is_loose and not is_strict:
            if sha in exempt:
                ex_loose.append((sha[:9], subject))
            else:
                loose_only.append((sha[:9], subject))
        if not is_loose:
            if sha in grandfathered:
                gf += 1
            else:
                missing.append((sha[:9], subject))

    n = len(shas)
    say("range                     : %s" % rng)
    say("commits                   : %d" % n)
    say("Taken-from/Origin STRICT  : %d / %d   (git's own trailer-block parser; this is what CI sees)" % (strict, n))
    say("Taken-from/Origin LOOSE   : %d / %d   (token present anywhere in the message)" % (loose, n))
    say("Measured-effect           : %d / %d" % (meas, n))
    say("native (Origin trailer)   : %d       (fork-authored, no upstream origin - convention 2026-08-20)" % native)
    say("grandfathered NATIVE      : %d / %d  (sha-pinned in tools/arifi-sync/native-grandfather.json, ledger CLOSED)" % (gf, n))
    say("EXEMPT, NOT REPAIRED      : %d / %d  (%d loose-only + %d without Measured-effect; sha-pinned with a reason each in tools/arifi-sync/trailer-exemptions.json)"
        % (len(ex_loose) + len(ex_meas), n, len(ex_loose), len(ex_meas)))

    if ex_loose or ex_meas:
        say("\nEXEMPT - the provenance is present and human-readable but does NOT parse, and the")
        say("only repair would be amending published, tagged, series-pinned history. These are")
        say("NOT fixed commits. Each row's reason is in trailer-exemptions.json.")
        for sha, s in ex_loose:
            say("  %s  [loose-only]          %s" % (sha, s))
        for sha, s in ex_meas:
            say("  %s  [no Measured-effect]  %s" % (sha, s))

    if loose_only:
        say("\nLOOSE-ONLY - provenance is human-readable but NOT machine-readable.")
        say("A trailer only parses when it sits in a trailing paragraph of trailer-shaped lines.")
        say("")
        say("The usual cause is a WRAPPED VALUE. git ends the trailer block at the first line")
        say("that is not trailer-shaped, so a continuation line starting in column 0 silently")
        say("invalidates every trailer in the block - including the ones above it:")
        say("")
        say("    WRONG                              RIGHT")
        say("    Measured-effect: 12.5 t/s on the   Measured-effect: 12.5 t/s on the")
        say("    reference machine, 3 rolls.          reference machine, 3 rolls.")
        say("    ^ column 0 - block ends here       ^ indented - still the same trailer")
        say("")
        say("Indent continuation lines by at least one space, and keep the trailer block as")
        say("the LAST paragraph of the message. Prose after it has the same effect.")
        for sha, s in loose_only:
            say("  %s  %s" % (sha, s))
    if no_meas:
        say("\nNO Measured-effect: trailer (patches/series/MANIFEST.md requires one):")
        for sha, s in no_meas:
            say("  %s  %s" % (sha, s))
    if missing:
        say("\nNO PROVENANCE AT ALL - this is a hard failure:")
        for sha, s in missing:
            say("  %s  %s" % (sha, s))

    if missing:
        say("\nFAIL: %d commit(s) carry no provenance." % len(missing))
        return 1
    if args.strict and (loose_only or no_meas):
        say("\nFAIL (--strict): %d loose-only, %d without Measured-effect."
            % (len(loose_only), len(no_meas)))
        return 1
    if ex_loose or ex_meas:
        say("\nPASS (%d EXEMPT, not repaired - %d loose-only, %d without Measured-effect):"
            % (len(ex_loose) + len(ex_meas), len(ex_loose), len(ex_meas)))
        say("every commit carries provenance; %d of them carry it in a form git cannot parse,"
            % (len(ex_loose) + len(ex_meas)))
        say("and are sha-pinned with a reason in tools/arifi-sync/trailer-exemptions.json.")
        return 0
    say("\nPASS: every commit carries provenance.")
    return 0


# ---------------------------------------------------------------- series

GROUP_RULES = [
    (re.compile(r"IOCP", re.I), "iocp-async"),
    (re.compile(r"^docs:"), "arifi-fork-base"),
    (re.compile(r"g128|Q2_0_G128|retagger|gguf\): retag", re.I), "ternary-g128"),
    (re.compile(r"^(vulkan|loader|ggml-cpu):"), "ternary-g128"),
    (re.compile(r"^community:"), "community-prefetch"),
    (re.compile(r"^powerinfer:|^build: wire powerinfer"), "powerinfer-streaming"),
    (re.compile(r"M-RoPE"), "rocmfpx-mrope"),
    (re.compile(r"recurrent", re.I), "prismml-recurrent"),
]
DEFAULT_GROUP = "arifi-fork-base"

GATE_TOKENS = [
    "GGML_ARIFI_VNNI_REPACK", "GGML_ARIFI_TURBO_KV", "GGML_RECURRENT_STATE_F16",
    "POWERINFER_IOCP", "EXPERT_BUNDLE_PATH", "GENERATE_EXPERT_BUNDLE",
    "LANE110_PREFETCH_CAP", "LANE110_PROF", "MAX_N_CACHED",
    "LLAMA_USE_PREBUILT_UI", "ARIFI_TOOL_NVFP4_REMAP",
]


def classify(subject: str) -> str:
    for rx, group in GROUP_RULES:
        if rx.search(subject):
            return group
    return DEFAULT_GROUP


def gates_in_commit(repo: str, sha: str) -> list:
    _, diff, _ = git(repo, "show", "--format=", "--unified=0", sha)
    found = []
    for line in diff.splitlines():
        if not line.startswith("+") or line.startswith("+++"):
            continue
        for tok in GATE_TOKENS:
            if tok in line and tok not in found:
                found.append(tok)
    return found


def format_patch(repo: str, cfg: dict, base: str, ref: str, outdir: str) -> list:
    """Generate the series. Three generation rules are load-bearing:

    --binary is MANDATORY: commit d09d083aa vendors 19 binary files (PowerInfer's cli11/fmt test
    fixtures and images) and without it git am cannot apply them.

    --no-signature is MANDATORY: otherwise every patch ends with the local git version and the
    integrity check breaks the day git is upgraded.

    ':(exclude)<series_dir>' is MANDATORY, and is what makes the artifact CONVERGE. The series is
    committed inside the tree it describes. Without the exclusion, regenerating emits a patch
    whose content is the patch files, committing that changes the series, regenerating emits a
    patch for THAT commit, and so on: `series check --ref master` can never pass, because the
    commit that publishes the series is by construction never inside the series it published. A
    self-containing patch is impossible - format-patch writes the commit's own sha into its
    `From` line. Excluding the generated directory makes a commit that touches only the series a
    no-op for generation, so the second regeneration produces zero diff and the fixpoint is real.

    This rule was stated in commit 5d1486012 ("The generated series excludes patches/series/ so
    that the artifact converges") but was never implemented here; the committed series was
    hand-filtered to match. It is now enforced in code.

    A LINEAR range is equally mandatory, and is checked here rather than assumed. `format-patch`
    silently omits merge commits, so a merge in the range takes its hand-made conflict resolutions
    out of the series while keeping every commit that builds on them - the series then generates
    and byte-compares fine and only fails much later, at `git am`, on a patch that looks innocent.
    `status` has always printed the merge count with "must be 0", but nothing enforced it."""
    merges = gout(repo, "rev-list", "--count", "--merges", "%s..%s" % (base, ref)).strip()
    if merges not in ("0", ""):
        raise Loud(
            "range %s..%s contains %s merge commit(s); the series must be linear.\n"
            "  format-patch omits merges, so their conflict resolutions never reach the series and\n"
            "  the commits that depend on them cannot apply. Linearize the branch (rebase, not\n"
            "  merge) onto the base, or point base.ref at a linear branch.\n"
            "  offenders:\n%s"
            % (base[:9], ref, merges,
               "\n".join("    " + l for l in gout(
                   repo, "log", "--oneline", "--merges", "%s..%s" % (base, ref)).splitlines()[:10])))
    os.makedirs(outdir, exist_ok=True)
    _, out, _ = git(repo, "format-patch", "--binary", "--no-signature", "-N",
                    "--output-directory", outdir, "%s..%s" % (base, ref),
                    "--", ":(exclude)%s" % cfg["series_dir"])
    return [l.strip() for l in out.splitlines() if l.strip()]


def patch_sha(path: str) -> str:
    """The commit a generated patch came from, read from its own `From <sha>` header.

    Attribution is read back out of the artifact rather than re-derived by zipping the file list
    against `git rev-list`. The zip was silently order-dependent and could not survive a generator
    that legitimately drops commits (see format_patch): it would have mis-attributed every row
    after the first dropped commit while still passing its own length check."""
    with open(path, encoding="utf-8", errors="replace") as fh:
        first = fh.readline().strip()
    m = re.match(r"^From ([0-9a-f]{40}) ", first)
    if not m:
        raise Loud("patch has no 'From <sha>' header, cannot attribute it: %s" % path)
    return m.group(1)


def series_files(directory: str) -> list:
    if not os.path.isdir(directory):
        return []
    return sorted(f for f in os.listdir(directory)
                  if re.match(r"^\d{4}-.*\.patch$", f))


def build_manifest(repo: str, cfg: dict, ref: str, files: list, outdir: str) -> str:
    base = cfg["base"]["upstream_sha"]
    tag = cfg["base"]["upstream_tag"]
    shas = [patch_sha(os.path.join(outdir, fn)) for fn in files]
    allshas = gout(repo, "rev-list", "--reverse", "%s..%s" % (base, ref)).split()
    for sha in shas:
        if sha not in allshas:
            raise Loud("patch attributes itself to %s, which is not in %s..%s"
                       % (sha[:9], base[:9], ref))
    # NOTE: do NOT derive anything in this manifest from the commits the generator dropped. Doing
    # so makes the manifest a function of excluded commits, and every series-only commit then
    # changes it - which is the same non-convergence the exclusion exists to remove, one level up.
    # An earlier draft of this function listed them and diverged by exactly one line per regen.

    # Which patch carries the binary payload, read off the artifact rather than remembered. The
    # previous manifest hardcoded `0008`; the real one is whichever file contains a GIT binary
    # patch, and it moves whenever a commit is added ahead of it.
    binpatch = "-"
    for fn in files:
        with open(os.path.join(outdir, fn), encoding="utf-8", errors="replace") as fh:
            if "GIT binary patch" in fh.read():
                binpatch = fn.split("-", 1)[0]
                break

    rows, unclassified = [], []
    for fn, sha in zip(files, shas):
        subject = gout(repo, "log", "-1", "--format=%s", sha)
        tr = commit_trailers(repo, sha)
        group = classify(subject)
        if group == DEFAULT_GROUP and not subject.startswith(("docs:", "chore:", "ci:", "lane-110")):
            unclassified.append((sha[:9], subject))
        src = tr["Taken-from"] or "-"
        src = ", ".join(s.strip() for s in src.split(",")) if src != "-" else "-"
        gates = ", ".join("`%s`" % g for g in gates_in_commit(repo, sha)) or "-"
        meas = tr["Measured-effect"] or "*(no Measured-effect trailer)*"
        rows.append((fn, group, sha[:9], src, gates, subject, meas))

    L = []
    L.append("# ArifiLabs patch-series MANIFEST")
    L.append("")
    L.append("<!-- GENERATED FILE - DO NOT EDIT BY HAND.")
    L.append("     Regenerate:  python tools/arifi-sync/arifi_sync.py series regen")
    L.append("     Verify:      python tools/arifi-sync/arifi_sync.py series check  -->")
    L.append("")
    L.append("## What this is")
    L.append("")
    L.append("`patches/series/` is the complete, ordered, replayable definition of everything ArifiLabs")
    L.append("adds to upstream llama.cpp. It is generated from git, never hand-maintained, and an")
    L.append("integrity check refuses to pass if it has drifted from git by so much as a byte.")
    L.append("")
    L.append("- Base: `ggml-org/llama.cpp` tag `%s`, `%s`" % (tag, base))
    L.append("- Patches: **%d**, all non-merge, applied in filename order." % len(files))
    L.append("")
    L.append("## Applying the series")
    L.append("")
    L.append("```bash")
    L.append("git checkout -b my-rebuild %s" % base[:9])
    L.append("git am patches/series/*.patch")
    L.append("```")
    L.append("")
    L.append("The result is byte-identical to `master` everywhere outside `%s` itself:"
             % cfg["series_dir"])
    L.append("same file contents, same %d commit messages, same provenance trailers. Verified, not"
             % len(files))
    L.append("asserted - `series check` replays the series with `git am` and diffs the result against")
    L.append("`master` on every run.")
    L.append("")
    L.append("## Why the series excludes itself")
    L.append("")
    L.append("`%s` is generated INTO the tree it describes, so it is excluded from its own"
             % cfg["series_dir"])
    L.append("generation with `':(exclude)%s'`. This is not tidiness, it is what makes the"
             % cfg["series_dir"])
    L.append("artifact converge. Include it, and regenerating emits a patch whose content is the patch")
    L.append("files; committing that changes the series; regenerating emits a patch for THAT commit,")
    L.append("forever. The commit that publishes the series can never be inside the series it")
    L.append("published - a self-containing patch is impossible, since `format-patch` writes the")
    L.append("commit's own sha into its `From` line. With the exclusion, a commit touching only the")
    L.append("series generates nothing, the next regeneration produces zero diff, and `series check`")
    L.append("can actually pass.")
    L.append("")
    L.append("The manifest deliberately does NOT enumerate the commits that rule drops, and must never")
    L.append("start to. Anything generated from an excluded commit puts the loop straight back: every")
    L.append("regeneration commit would add its own line, requiring another regeneration, forever. This")
    L.append("file is a function of the commits it INCLUDES, and of nothing else. List the dropped ones")
    L.append("on demand instead:")
    L.append("")
    L.append("```bash")
    L.append("git log --oneline %s..master -- %s" % (base[:9], cfg["series_dir"]))
    L.append("```")
    L.append("")
    L.append("## Load-bearing generation flags")
    L.append("")
    L.append("Never drop any of these:")
    L.append("")
    L.append("- `--binary` - patch `%s` vendors 19 binary files (PowerInfer's bundled cli11/fmt"
             % binpatch)
    L.append("  fixtures and images). Without `--binary` `git format-patch` emits *\"Binary files")
    L.append("  differ\"* and `git am` cannot apply the series at all.")
    L.append("- `--no-signature` - otherwise every patch is terminated with the local git version")
    L.append("  string, and the integrity check starts failing the day git is upgraded.")
    L.append("- `':(exclude)%s'` - see above; without it there is no fixpoint."
             % cfg["series_dir"])
    L.append("")
    L.append("`.gitattributes` pins `patches/**` to `-text`, so these files are byte-identical in")
    L.append("every clone. Without it, `core.autocrlf=true` (the Git-for-Windows default) checks them")
    L.append("out as CRLF while `format-patch` writes LF, and the byte-compare cannot pass at all.")
    L.append("")
    L.append("## Flat numbering, grouped in the table")
    L.append("")
    L.append("Patches are flat and globally numbered because **apply order is the authoritative**")
    L.append("**property** - the groups interleave (an ArifiLabs docs commit sits between the")
    L.append("PowerInfer series and the thecodacus series), so per-group subdirectories would put")
    L.append("the order at the mercy of shell glob expansion. The grouping is metadata, and lives")
    L.append("in the table below and in `SERIES`.")
    L.append("")
    L.append("`patches/banked-source/` is **not part of this series.** Those are other projects' own")
    L.append("format-patches, banked as *source material* for a port. Do not `git am` them. They were")
    L.append("moved out of `%s` by `5d1486012` precisely because everything inside the"
             % cfg["series_dir"])
    L.append("generated directory is destroyed and rewritten on the next `series regen`.")
    L.append("")
    L.append("## The series")
    L.append("")
    L.append("| # | Patch | Group | Commit | Taken-from | Gate / toggle | Effect |")
    L.append("|---|---|---|---|---|---|---|")
    for i, (fn, group, sha, src, gates, subject, meas) in enumerate(rows, 1):
        L.append("| %d | `%s` | %s | `%s` | %s | %s | %s |"
                 % (i, fn, group, sha, src, gates, subject.replace("|", "\\|")))
    L.append("")
    L.append("## Measured effect, per patch")
    L.append("")
    L.append("Reproduced verbatim from each commit's `Measured-effect:` trailer. `UNMEASURED` is a")
    L.append("legal and honest value; an absent trailer is a gap and is named as one.")
    L.append("")
    L.append("| Patch | Measured-effect |")
    L.append("|---|---|")
    for fn, group, sha, src, gates, subject, meas in rows:
        L.append("| `%s` | %s |" % (fn, meas.replace("|", "\\|")))
    L.append("")
    if unclassified:
        L.append("## Unclassified")
        L.append("")
        L.append("These commits fell through every grouping rule in `arifi_sync.py`. That is surfaced")
        L.append("rather than silently bucketed - add a rule when a new source appears.")
        L.append("")
        for sha, s in unclassified:
            L.append("- `%s` %s" % (sha, s))
        L.append("")
    return "\n".join(L) + "\n"


def regen_preflight(repo: str, cfg: dict, ref: str) -> None:
    """Every condition that can abort a regen, evaluated BEFORE anything is deleted.

    Linearity is not checked here because `format_patch` already raises on it, and this
    function's callers generate into a temp directory first - so generation IS a pre-flight.
    What is checked here is the condition generation cannot see: a commit in the range with no
    provenance at all produces a MANIFEST row that asserts an attribution the repository does
    not have. `provenance` reports that as a hard failure; regen must not publish it."""
    base = cfg["base"]["upstream_sha"]
    shas = gout(repo, "rev-list", "--reverse", "%s..%s" % (base, ref)).split()
    if not shas:
        raise Loud("empty range %s..%s - is the base pin correct?" % (base[:9], ref))
    grandfathered = load_grandfather(repo)
    missing = [s for s in shas
               if not has_loose_provenance(repo, s) and s not in grandfathered]
    if missing:
        raise Loud(
            "%d commit(s) in %s..%s carry no provenance; the series would publish a MANIFEST\n"
            "  that attributes them to nothing. Add a Taken-from:/Origin: trailer (or a\n"
            "  grandfather row for a pre-2026-08-20 native commit), then regenerate.\n"
            "  Full report: python tools/arifi-sync/arifi_sync.py provenance --ref %s\n"
            "  offenders:\n%s"
            % (len(missing), base[:9], ref, ref,
               "\n".join("    %s  %s" % (s[:9], gout(repo, "log", "-1", "--format=%s", s))
                         for s in missing[:10])))


def cmd_series_regen(repo: str, cfg: dict, args) -> int:
    """Generate to a temp directory, verify, and only then swap. NEVER delete first.

    Until 2026-09-04 this function ran its `os.remove` loop over every file in
    `patches/series/` and only THEN called `format_patch()`, which is where the linearity
    pre-flight STOP lives. On a branch that fails that pre-flight - which `arifi/main` did,
    with 10 merge commits - it therefore exited rc=2 having left the repository with NO SERIES
    AT ALL. Measured (lane-209 rank 3): 415 tracked `.patch` files deleted by what its operator
    ran as a read-only probe. The files were tracked so `git restore` recovered them, but a
    destructive-then-abort chokepoint that depends on the operator noticing is not a safe
    chokepoint, and UPDATE-RUNBOOK 4.1 could only warn about it.

    The order is now: pre-flight -> generate to temp -> build every artifact from the temp
    directory -> swap. Any failure anywhere above the swap leaves `patches/series/`
    byte-for-byte untouched, which is what the tests in test_arifi_sync.py assert."""
    base = cfg["base"]["upstream_sha"]
    outdir = os.path.join(repo, cfg["series_dir"])
    step("Regenerating %s from %s..%s" % (cfg["series_dir"], base[:9], args.ref))
    tmp = tempfile.mkdtemp(prefix="arifi-regen-")
    try:
        regen_preflight(repo, cfg, args.ref)
        made = format_patch(repo, cfg, base, args.ref, tmp)
        say("generated %d patches (staged, nothing swapped yet)" % len(made))
        files = series_files(tmp)

        series_txt = ("# Apply order. Machine-readable; independent of glob sorting.\n"
                      "# git am $(sed '/^#/d' patches/series/SERIES | sed 's|^|patches/series/|')\n"
                      + "".join(f + "\n" for f in files))
        # build_manifest reads the patch files it is given, so it runs against the STAGED set.
        # It raises on a patch that attributes itself outside the range - one more failure that
        # now happens while the committed series is still intact.
        manifest = build_manifest(repo, cfg, args.ref, files, tmp)

        # ---- swap. This is the first line in the function that touches patches/series/. ----
        os.makedirs(outdir, exist_ok=True)
        for old in series_files(outdir):
            os.remove(os.path.join(outdir, old))
        for f in files:
            shutil.move(os.path.join(tmp, f), os.path.join(outdir, f))
        with open(os.path.join(outdir, "SERIES"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(series_txt)
        say("wrote SERIES (%d entries)" % len(files))
        with open(os.path.join(outdir, "MANIFEST.md"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(manifest)
        say("wrote MANIFEST.md")
        return 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def series_pathspec(cfg: dict) -> str:
    """One normalized spelling of the generated directory, for every `:(exclude)` pathspec.
    Interpolating the raw config string put a backslash or a trailing slash into a pathspec that
    then silently matched nothing - and an exclusion that excludes nothing is a comparison that
    quietly changed its meaning."""
    return cfg["series_dir"].replace("\\", "/").rstrip("/")


def series_integrity_problems(repo: str, cfg: dict, ref: str) -> list:
    """The committed series IS what git generates for `ref`, and every patch is committed there.

    This is what makes a series-EXCLUDED tree comparison honest (CHECK-UPDATE-ROOT finding 4). A
    candidate and its replay legitimately differ inside `series_dir`, because the generator
    excludes that directory from itself; so a comparison that did not exclude it could never match,
    and one that does exclude it would be blind to a hand-edited patch. It is not blind, because
    the excluded directory is proved separately - here - to be exactly the artifact git produces.

    No replay is run: this is the byte-and-tracking half only, so `bump` can call it without
    paying for a second `git am` of the whole series.
    """
    base = cfg["base"]["upstream_sha"]
    committed = os.path.join(repo, cfg["series_dir"])
    tmp = tempfile.mkdtemp(prefix="arifi-integrity-")
    try:
        format_patch(repo, cfg, base, ref, tmp)
        have, want = series_files(committed), series_files(tmp)
        out = []
        out += ["series: %s is committed but no longer regenerates from git (stale)" % f
                for f in have if f not in want]
        out += ["series: %s regenerates from git but is not in %s (missing)" % (f, cfg["series_dir"])
                for f in want if f not in have]
        out += ["series: %s DIFFERS from a fresh generation from git (hand-edited patch?)" % f
                for f in want if f in have
                and not filecmp.cmp(os.path.join(tmp, f), os.path.join(committed, f),
                                    shallow=False)]
        prefix = series_pathspec(cfg) + "/"
        tracked = set()
        for line in gout(repo, "ls-tree", "-r", "--name-only", ref, "--",
                         cfg["series_dir"]).splitlines():
            line = line.strip().replace("\\", "/")
            if line.startswith(prefix):
                tracked.add(line[len(prefix):])
        out += ["series: %s exists on disk but is NOT COMMITTED at %s - a fresh clone dies on it"
                % (f, ref) for f in want if f not in tracked]
        return out
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def cmd_series_check(repo: str, cfg: dict, args) -> int:
    base = cfg["base"]["upstream_sha"]
    committed = os.path.join(repo, cfg["series_dir"])
    step("Integrity check: regenerating the series and byte-comparing against %s" % cfg["series_dir"])
    # The byte compare is against the WORKING DIRECTORY, which is not the same thing as
    # "committed" no matter what the help text used to say. An untracked patch file satisfies the
    # compare and then does not exist in a fresh clone, so `git am` dies on it. That defect shipped
    # twice (lane-147, then again inside lane-151's own regen), so committed-ness is asserted
    # separately and by name, inside the shared predicate.
    problems = series_integrity_problems(repo, cfg, args.ref)
    if problems:
        say("FAIL: the committed series has DRIFTED from git (%d finding(s)):" % len(problems))
        for p in problems:
            say("  %s" % p)
        say("\nFix with:")
        say("  python tools/arifi-sync/arifi_sync.py series regen --ref %s" % args.ref)
        # `add` first, and by pathspec. regen writes NEW patch files; `git commit -- <path>` only
        # commits paths git already tracks, so without the `add` the new patch stays untracked -
        # it passes the byte compare here and is then missing from a fresh clone.
        say("  git add -- %s" % cfg["series_dir"])
        say("  git commit -- %s" % cfg["series_dir"])
        return 1
    say("PASS: every patch is byte-identical to a fresh generation from git AND a committed blob.")

    step("Replay check: git am the committed series onto %s" % base[:9])
    return _replay(repo, cfg, base, committed, expect_ref=args.ref)


REPLAY_BRANCH = "arifi-sync/replay"


def _replay(repo: str, cfg: dict, onto: str, series_path: str, expect_ref: str = "",
            record=None, keep_branch: bool = False) -> int:
    """Replay the series onto `onto` in a throwaway worktree. Loud, per-patch, never auto-resolves.

    `keep_branch` RETAINS the replayed candidate on `arifi-sync/replay` after the worktree is
    removed, and only on success. The worktree was always destroyed, which meant the candidate -
    the one tree a bump is actually about - existed for the length of one function call and then
    could not be diffed, inspected or built (CHECK-UPDATE-ROOT finding 4). This is a lifetime
    change to a scratch branch this function already created and deleted on every run; no
    canonical branch is created or moved."""
    files = [os.path.join(series_path, f) for f in series_files(series_path)]
    if not files:
        raise Loud("no patches found in %s" % series_path)
    parent = os.path.dirname(repo.rstrip("/\\"))
    wt = os.path.join(parent, ".arifi-replay")
    branch = REPLAY_BRANCH
    kept = False
    if os.path.exists(wt):
        git(repo, "worktree", "remove", "--force", wt, check=False)
        shutil.rmtree(wt, ignore_errors=True)
    git(repo, "branch", "-D", branch, check=False)
    say("worktree: %s   (short path on purpose - llama.cpp's tools/ui tree blows past" % wt)
    say("           Windows MAX_PATH from a deep temp directory)")
    # The replay worktree must be CHECKED OUT with autocrlf=false as well: with the Git-for-Windows default
    # (core.autocrlf=true) the checkout writes CRLF while the index holds LF, and the very first patch that
    # touches an existing text file dies with "does not match index" (HQ seat-59, 2026-09-15, 283/550).
    git(repo, "-c", "core.autocrlf=false", "worktree", "add", "--detach", wt, onto)
    try:
        git(wt, "-c", "core.autocrlf=false", "checkout", "-B", branch, onto)
        for i, patch in enumerate(files, 1):
            name = os.path.basename(patch)
            # core.autocrlf=false for the replay: the committed blobs are the artifact. With the
            # Git-for-Windows default (true) `git am` eol-converts any file the base's .gitattributes
            # does not yet mark -text, and the replayed tree differs from arifi/main by line endings
            # alone (HQ seat-58, 2026-09-14: eight evidence receipts, 1584 lines swapped).
            rc, out, err = git(wt, "-c", "core.autocrlf=false", "am", "--keep-cr", "--keep-non-patch", patch, check=False)
            if rc != 0:
                say("\n" + "=" * 78)
                say("REPLAY FAILED - STOPPING. Nothing was auto-resolved.")
                say("=" * 78)
                say("patch  : %d/%d  %s" % (i, len(files), name))
                say("onto   : %s" % onto)
                say("worktree preserved for inspection: %s" % wt)
                say("\n--- git am ---\n%s%s" % (out, err))
                _, st, _ = git(wt, "status", "--short", check=False)
                say("--- conflicted paths ---\n%s" % (st or "(none reported)"))
                rej = []
                for root, _dirs, fs in os.walk(wt):
                    if ".git" in root:
                        continue
                    for f in fs:
                        if f.endswith(".rej"):
                            rej.append(os.path.join(root, f))
                for r in rej[:20]:
                    say("\n--- REJECTED HUNKS: %s ---" % os.path.relpath(r, wt))
                    with open(r, encoding="utf-8", errors="replace") as fh:
                        say(fh.read()[:4000])
                say("\nResolve by hand, or drop/rework the patch. Do not force-apply.")
                return 1
            say("  [%2d/%d] ok  %s" % (i, len(files), name))
        tip = gout(wt, "rev-parse", "HEAD")
        tree = gout(wt, "rev-parse", "HEAD^{tree}")
        # The replay worktree is destroyed in `finally`. Hand the caller the identity of what was
        # replayed, because that - not the checkout the caller is standing in - is the candidate.
        if record is not None:
            record["tip"], record["tree"], record["onto"] = tip, tree, onto
            record["branch"] = branch if keep_branch else ""
        kept = keep_branch
        say("\nreplayed %d patches cleanly -> %s (tree %s)" % (len(files), tip[:9], tree[:9]))
        if expect_ref:
            # The series excludes its own directory (see format_patch), so the replayed tree
            # legitimately lacks it. Everything else must match the reference to the byte, and
            # that is asserted by a real diff rather than by trusting the patch count.
            expect = gout(repo, "rev-parse", expect_ref)
            rc, out, _ = git(wt, "diff", "--stat", expect, "HEAD",
                             "--", ".", ":(exclude)%s" % series_pathspec(cfg), check=False)
            if rc == 0 and not out.strip():
                say("PASS: replayed tree is IDENTICAL to %s outside %s"
                    % (expect_ref, cfg["series_dir"]))
                say("      (reference tree %s, replayed tree %s - they differ only by the"
                    % (gout(repo, "rev-parse", expect_ref + "^{tree}")[:9], tree[:9]))
                say("       generated series itself, which by construction cannot contain itself)")
            else:
                say("FAIL: replayed tree differs from %s outside %s:" % (expect_ref, cfg["series_dir"]))
                say(out or "(git diff failed)")
                return 1
        return 0
    finally:
        if os.path.exists(wt):
            git(repo, "worktree", "remove", "--force", wt, check=False)
            shutil.rmtree(wt, ignore_errors=True)
        if not kept:
            git(repo, "branch", "-D", branch, check=False)


def cmd_series_replay(repo: str, cfg: dict, args) -> int:
    step("Replaying the committed series onto %s" % args.onto)
    return _replay(repo, cfg, args.onto, os.path.join(repo, cfg["series_dir"]))


# ---------------------------------------------------------------- currency

def _resolve_ref(repo: str, ref: str) -> str:
    rc, out, _ = git(repo, "rev-parse", ref, check=False)
    return out.strip() if rc == 0 else ""


def cmd_currency(repo: str, cfg: dict, args) -> int:
    """Report per-source drift. This CANNOT be satisfied by doing nothing:

    - a source that has never been fetched FAILS (no remote-tracking ref exists);
    - a source whose last successful fetch is older than max_fetch_age_days FAILS;
    - a source whose live head != the reviewed pin in sources.json FAILS, and the only way to
      clear it is to review the new commits and write the new sha into sources.json.
    """
    state = read_state(repo, cfg)
    fetched = state.setdefault("last_fetch", {})
    rows, failures = [], []

    if not args.no_fetch:
        for name, spec in cfg["remotes"].items():
            step("fetch %s (%s)" % (name, spec["url"]))
            fargs = ["fetch", name, "--prune"]
            if spec.get("track_tags"):
                fargs.append("--tags")
            rc, _out, err = git(repo, *fargs, check=False)
            if rc != 0:
                say("FETCH FAILED: %s\n%s" % (name, err.strip()))
                failures.append("%s: fetch failed" % name)
            else:
                fetched[name] = now_iso()
                say("ok")

    step("drift against reviewed pins")
    for name, spec in cfg["remotes"].items():
        ref = spec["ref"]
        live = _resolve_ref(repo, ref)
        if not live:
            rows.append((name, ref, "NEVER FETCHED", spec["pin"], "FAIL"))
            failures.append("%s: ref %s does not resolve - the remote has never been fetched"
                            % (name, ref))
            continue
        pin = spec["pin"]
        pin_full = _resolve_ref(repo, pin)
        if not pin_full:
            rows.append((name, ref, live[:9], pin, "FAIL (pin unknown)"))
            failures.append("%s: pinned sha %s is not an object in this repo" % (name, pin))
            continue
        if live == pin_full:
            rows.append((name, ref, live[:9], pin[:9], "current"))
        else:
            _, cnt, _ = git(repo, "rev-list", "--count", "%s..%s" % (pin_full, live), check=False)
            behind = cnt.strip() or "?"
            rows.append((name, ref, live[:9], pin[:9], "BEHIND by %s commit(s)" % behind))
            failures.append("%s: %s commit(s) unreviewed between pin %s and %s"
                            % (name, behind, pin[:9], ref))

    for name, spec in cfg.get("local_checkouts", {}).items():
        if name.startswith("_"):
            continue
        path = spec["path"]
        if not os.path.isabs(path):
            path = os.path.normpath(os.path.join(repo, path))
        if not os.path.isdir(os.path.join(path, ".git")):
            rows.append((name, spec["ref"], "NOT ON DISK", spec["pin"], "FAIL (missing checkout)"))
            failures.append("%s: local checkout not found at %s" % (name, path))
            continue
        if not args.no_fetch:
            rc, _o, err = git(path, "fetch", "--prune", check=False)
            if rc != 0:
                say("FETCH FAILED (local %s): %s" % (name, err.strip()))
                failures.append("%s: fetch failed" % name)
            else:
                fetched[name] = now_iso()
        live = _resolve_ref(path, spec["ref"]) or _resolve_ref(path, "HEAD")
        pin = spec["pin"]
        if pin == "UNPINNED":
            rows.append((name, spec["ref"], live[:9], "UNPINNED", "FAIL (never reviewed)"))
            failures.append("%s: UNPINNED - no reviewed sha has ever been recorded" % name)
        elif _resolve_ref(path, pin) == live:
            rows.append((name, spec["ref"], live[:9], pin[:9], "current"))
        else:
            rows.append((name, spec["ref"], live[:9], pin[:9], "BEHIND"))
            failures.append("%s: local checkout has moved past the reviewed pin" % name)

    say("")
    say("%-22s %-18s %-14s %-12s %s" % ("SOURCE", "REF", "LIVE", "REVIEWED", "STATUS"))
    say("-" * 92)
    for r in rows:
        say("%-22s %-18s %-14s %-12s %s" % r)

    step("fetch freshness (max %d days)" % cfg["max_fetch_age_days"])
    limit = cfg["max_fetch_age_days"]
    for name in list(cfg["remotes"]) + [k for k in cfg.get("local_checkouts", {})
                                        if not k.startswith("_")]:
        ts = fetched.get(name)
        if not ts:
            say("  %-22s NEVER FETCHED   <- FAIL" % name)
            failures.append("%s: no successful fetch has ever been recorded" % name)
            continue
        age = (_dt.datetime.now(_dt.timezone.utc)
               - _dt.datetime.fromisoformat(ts)).days
        flag = "  <- FAIL" if age > limit else ""
        say("  %-22s %s (%d days)%s" % (name, ts, age, flag))
        if age > limit:
            failures.append("%s: last fetch was %d days ago" % (name, age))

    state["last_fetch"] = fetched
    state["last_currency_run"] = now_iso()
    write_state(repo, cfg, state)

    if failures:
        say("\n" + "=" * 78)
        say("CURRENCY FAILED - %d finding(s). This is the intended behaviour when work is owed." % len(failures))
        say("=" * 78)
        for f in failures:
            say("  - %s" % f)
        say("\nTo clear a BEHIND row you must REVIEW the new commits and write the sha you")
        say("reviewed into tools/arifi-sync/sources.json. There is no way to clear it by")
        say("re-running this command.")
        return 1
    say("\nPASS: every source is fetched, fresh, and at its reviewed pin.")
    return 0


# ---------------------------------------------------------------- build recipe

def parse_cache(path: str) -> dict:
    """Parse a CMakeCache.txt into {key: (type, value)}, dropping INTERNAL/STATIC noise."""
    out = {}
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line or line.startswith(("//", "#")):
                continue
            if ":" not in line or "=" not in line:
                continue
            key, rest = line.split(":", 1)
            if "=" not in rest:
                continue
            ctype, value = rest.split("=", 1)
            if ctype in ("INTERNAL", "STATIC"):
                continue
            out[key.strip()] = (ctype.strip(), value)
    return out


def cmd_recipe_diff(repo: str, cfg: dict, args) -> int:
    """Recover the build recipe by DIFFING, never by grepping.

    Grepping a CMakeCache for the flags you happen to remember is how -D_WIN32_WINNT=0x0A00,
    LLAMA_USE_PREBUILT_UI=OFF and -static-libgcc -static-libstdc++ were each missed, costing
    three failed builds. A diff cannot miss a flag: anything that differs is reported.
    """
    snap = args.snapshot or os.path.join(repo, cfg["build"]["cache_snapshot"])
    live = args.live or os.path.join(repo, cfg["build"]["live_cache"])
    if not os.path.exists(snap):
        raise Loud("no committed recipe snapshot at %s\n"
                   "Create one from a known-good build with:\n"
                   "  python tools/arifi-sync/arifi_sync.py recipe export --snapshot" % snap)
    if not os.path.exists(live):
        raise Loud("no live cache at %s - configure a build first, or pass --live" % live)

    a, b = parse_cache(snap), parse_cache(live)
    step("recipe diff: committed snapshot  vs  live cache")
    say("snapshot: %s  (%d entries)" % (snap, len(a)))
    say("live    : %s  (%d entries)" % (live, len(b)))

    only_a = sorted(set(a) - set(b))
    only_b = sorted(set(b) - set(a))
    changed = sorted(k for k in set(a) & set(b) if a[k][1] != b[k][1])

    if only_a:
        say("\nLOST - in the recorded recipe, ABSENT from the live build (%d):" % len(only_a))
        for k in only_a:
            say("  - %s:%s=%s" % (k, a[k][0], a[k][1]))
    if only_b:
        say("\nNEW - in the live build, not in the recorded recipe (%d):" % len(only_b))
        for k in only_b:
            say("  + %s:%s=%s" % (k, b[k][0], b[k][1]))
    if changed:
        say("\nCHANGED (%d):" % len(changed))
        for k in changed:
            say("  ~ %s" % k)
            say("      recorded: %s" % a[k][1])
            say("      live    : %s" % b[k][1])
    if not (only_a or only_b or changed):
        say("\nPASS: the live build matches the recorded recipe exactly.")
        return 0
    say("\n%d difference(s). Every one is a deliberate decision or a regression - decide which."
        % (len(only_a) + len(only_b) + len(changed)))
    return 1 if (only_a or changed) else 0


def cmd_recipe_export(repo: str, cfg: dict, args) -> int:
    live = args.live or os.path.join(repo, cfg["build"]["live_cache"])
    if not os.path.exists(live):
        raise Loud("no cache at %s" % live)
    entries = parse_cache(live)

    if args.snapshot:
        dest = os.path.join(repo, cfg["build"]["cache_snapshot"])
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "w", encoding="utf-8", newline="\n") as fh:
            fh.write("# ArifiLabs build recipe snapshot - GENERATED, do not hand-edit.\n")
            fh.write("# Source: %s\n" % os.path.relpath(live, repo).replace("\\", "/"))
            fh.write("# Captured: %s\n" % now_iso())
            fh.write("# Non-INTERNAL / non-STATIC CMake cache entries only.\n")
            fh.write("# Compare a new build against this with: arifi_sync.py recipe diff\n")
            for k in sorted(entries):
                t, v = entries[k]
                fh.write("%s:%s=%s\n" % (k, t, v))
        say("wrote snapshot: %s (%d entries)" % (dest, len(entries)))

    out = os.path.join(repo, "arifi-recipe.cmake") if args.initial_cache is None else args.initial_cache
    with open(out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("# ArifiLabs llama.cpp build recipe - GENERATED by tools/arifi-sync.\n")
        fh.write("#\n")
        fh.write("# Replay this configuration into a fresh build directory:\n")
        fh.write("#   cmake -S . -B build-vulkan -C %s\n" % os.path.basename(out))
        fh.write("#\n")
        fh.write("# This carries EVERY non-internal cache entry, so it cannot omit a flag the way\n")
        fh.write("# a hand-written -D command line can. Absolute paths below are machine-specific:\n")
        fh.write("# review the FILEPATH/PATH entries before using this on another machine.\n")
        for k in sorted(entries):
            t, v = entries[k]
            esc = v.replace("\\", "\\\\").replace('"', '\\"')
            fh.write('set(%s "%s" CACHE %s "" FORCE)\n' % (k, esc, t))
    say("wrote initial-cache file: %s (%d entries)" % (out, len(entries)))
    say("replay it with:  cmake -S . -B <newbuild> -C %s" % os.path.basename(out))
    return 0


# ---------------------------------------------------------------- build + judge + bump

BUILD_CONFIG = "Release"


def cmd_build(repo: str, cfg: dict, args) -> int:
    """Order is the whole guard, and every step below happens BEFORE the compiler is invoked:

      1. the configured CMake source root and the build directory are canonicalized and bound to
         this checkout (finding 2);
      2. any existing receipt is DESTROYED - from here the build dir is being mutated, so a
         receipt in it describes a state that is about to stop being true. A failed, refused or
         interrupted build must leave no certification behind (findings 3 and 6);
      3. the recipe diff's return code is PROPAGATED. It used to be discarded, so a lost or
         changed build flag reached the compiler and then received a fresh receipt (finding 6).

    After the compiler: the source identity is re-sampled and must match the pre-compile sample,
    the produced artifacts are hashed, and only then is the receipt published atomically.
    """
    bdir = os.path.join(repo, cfg["build"]["build_dir"])
    rpath = os.path.join(bdir, BUILD_RECEIPT)

    # ORDER, and it is load-bearing: the build-directory SHAPE is judged before anything is
    # deleted. `_drop_receipt` removes a file inside `build_dir`, so a `build_dir` misconfigured
    # onto a source path would have this function delete from the source tree on its way to
    # refusing that very configuration. Shape first, then the drop.
    step("build root: the configured CMake source root must BE this checkout")
    roots = build_root_problems(repo, cfg)
    if roots:
        if _strict_descendant(bdir, repo):
            _drop_receipt(rpath)
        say("\nBUILD REFUSED - nothing was compiled and any previous receipt was removed:")
        for r in roots:
            say("  %s" % r)
        return 1
    say("cmake source root: %s  (== checkout)" % _canon(repo))
    say("build directory  : %s  (strictly inside the checkout)" % _canon(bdir))

    # CHECK-UPDATE-ROOT-R2 finding 3: EVERY refusal drops an earlier receipt, including the missing
    # -cache refusal below, which used to raise before reaching this line. A receipt that outlives
    # the configuration it describes is a certification of nothing.
    _drop_receipt(rpath)
    if not os.path.exists(canonical_cache(repo, cfg)):
        raise Loud("no configured build at %s - any previous receipt there was removed.\n"
                   "Configure it from the recorded recipe first:\n"
                   "  python tools/arifi-sync/arifi_sync.py recipe export\n"
                   "  cmake -S . -B %s -C arifi-recipe.cmake"
                   % (bdir, cfg["build"]["build_dir"]))

    step("recipe diff before building (a silently changed flag is a failed build later)")
    # The cache is named EXPLICITLY, as the one under the build directory the compiler is handed.
    # Resolving it from `build.live_cache` let a decoy cache be compared while another was built.
    if cmd_recipe_diff(repo, cfg, argparse.Namespace(
            snapshot=None, live=canonical_cache(repo, cfg))) != 0:
        say("\nBUILD REFUSED: the live configuration has LOST or CHANGED a recorded recipe entry.")
        say("The compiler was NOT invoked. Any previous receipt in %s was removed, because it"
            % cfg["build"]["build_dir"])
        say("described a build of a recipe this directory no longer holds. Restore the flag, or")
        say("record the new recipe deliberately:")
        say("  python tools/arifi-sync/arifi_sync.py recipe export --snapshot")
        return 1

    targets = cfg["build"]["targets"]
    step("cmake --build %s --target %s" % (cfg["build"]["build_dir"], " ".join(targets)))
    cmd = ["cmake", "--build", bdir, "--config", BUILD_CONFIG]
    for t in targets:
        cmd += ["--target", t]
    before = _tree_identity(repo, bdir)
    p = subprocess.run(cmd)
    if p.returncode != 0:
        say("\nBUILD FAILED (exit %d). Stopping - a bump is never accepted on a failed build."
            % p.returncode)
        say("No receipt exists in %s: the previous one was removed before this attempt, so a"
            % cfg["build"]["build_dir"])
        say("half-updated binary cannot keep an old certification.")
        return 1

    after = _tree_identity(repo, bdir)
    if not _identity_matches(before, after):
        say("\nBUILD NOT CERTIFIED: the source tree CHANGED while the compiler was running, so")
        say("the binaries are of no single state. Nothing was written; re-run on a settled tree.")
        say("  before: tree %s content %s" % (before["tree"][:9], before["content_sha256"][:12]))
        say("  after : tree %s content %s" % (after["tree"][:9], after["content_sha256"][:12]))
        return 1
    binary = _judge_binary(bdir)
    if binary is None:
        say("\nBUILD NOT CERTIFIED: cmake reported success but no llama-server was produced in")
        say("%s. That is a partial build; it gets no receipt." % bdir)
        return 1

    ident = dict(after)
    ident["built_utc"] = now_iso()
    ident["build_dir"] = cfg["build"]["build_dir"]
    ident["build_dir_abs"] = _canon(bdir)
    ident["cmake_home"] = _canon(repo)
    ident["config"] = BUILD_CONFIG
    # WHICH candidate was selected, recorded by name. `_judge_binary` walks a fixed candidate list,
    # so a build that selected `bin/llama-server` and a later judge that resolves a newly appeared
    # `bin/llama-server.exe` would each be internally consistent and be about different files.
    ident["judge_binary"] = artifact_key(bdir, binary)
    ident["artifacts"] = artifact_digest(bdir, binary)
    _write_receipt(rpath, ident)
    say("build ok  (identity receipt: %s tree %s content %s%s, %d artifact(s) hashed)"
        % (BUILD_RECEIPT, ident["tree"][:9], ident["content_sha256"][:12],
           ", DIRTY tree" if ident["dirty"] else "", len(ident["artifacts"])))
    return 0


def cmd_judge(repo: str, cfg: dict, args) -> int:
    """F-085, BINDING: the regression judge is llama-server. Not llama-cli, not llama-bench.

    This subcommand deliberately refuses to invent a verdict. It checks that the judge binary
    exists and that the harness is configured, then requires --i-have-read-bench-purity to run,
    because a timed arm under sibling load is worse than no number at all.
    """
    j = cfg["judge"]
    if j["binary"] != "llama-server":
        raise Loud("F-085 violation: the judge must be llama-server, found %r" % j["binary"])
    bdir = os.path.join(repo, cfg["build"]["build_dir"])
    binary = _judge_binary(bdir)
    if not binary:
        raise Loud("llama-server not found. Looked in:\n  %s"
                   % "\n  ".join(os.path.join(bdir, c.replace("/", os.sep))
                                 for c in JUDGE_BINARY_CANDIDATES))
    # WHAT did this binary come from? Before the receipt existed the answer was "whatever was
    # checked out whenever somebody last built", and a judge run said PASS about it either way.
    rpath = os.path.join(bdir, BUILD_RECEIPT)
    if not os.path.exists(rpath):
        raise Loud("no build-identity receipt at %s.\n"
                   "  The binary in this build dir was produced by an unknown source tree, so a\n"
                   "  PASS here would certify nothing. Build it through this tool, which records\n"
                   "  the identity it built:\n"
                   "    python tools/arifi-sync/arifi_sync.py build" % rpath)
    with open(rpath, "r", encoding="utf-8-sig") as fh:
        receipt = json.load(fh)
    now = _tree_identity(repo, bdir)
    if not _identity_matches(receipt, now):
        raise Loud("WRONG-TREE REFUSAL: the binaries in %s were built from a different source\n"
                   "  state than the one checked out now. The judge would report a verdict about\n"
                   "  code that is not in front of you.\n\n"
                   "    built   : HEAD %s tree %s%s\n"
                   "    current : HEAD %s tree %s%s\n\n"
                   "  Rebuild the tree you actually want judged, then judge it:\n"
                   "    python tools/arifi-sync/arifi_sync.py build\n"
                   "    python tools/arifi-sync/arifi_sync.py judge --i-have-read-bench-purity"
                   % (bdir, str(receipt.get("head"))[:9], str(receipt.get("tree"))[:9],
                      " +local edits" if receipt.get("dirty") else "",
                      now["head"][:9], now["tree"][:9],
                      " +local edits" if now["dirty"] else ""))
    # The source identity above says WHICH TREE was compiled. It says nothing about the bytes in
    # the build directory, and a judge verdict is a statement about those bytes. Everything below
    # re-checks the OUTPUT: the same configured roots, and the exact artifacts the build recorded.
    binp = build_root_problems(repo, cfg)
    for key, want in (("build_dir_abs", _canon(bdir)), ("cmake_home", _canon(repo)),
                      ("config", BUILD_CONFIG),
                      ("judge_binary", artifact_key(bdir, binary))):
        if receipt.get(key) != want:
            binp.append("the receipt was written for %s=%r, this run resolves %r"
                        % (key, receipt.get(key), want))
    recorded = receipt.get("artifacts")
    if not isinstance(recorded, dict) or not recorded:
        binp.append("the receipt carries NO artifact hashes, so it certifies no executable bytes. "
                    "Rebuild through this tool: arifi_sync.py build")
    else:
        binp.extend(artifact_problems(bdir, binary, recorded))
    if binp:
        raise Loud("BINARY REFUSAL: the build directory does not hold the artifacts this receipt\n"
                   "  certifies, so a verdict would be about something else.\n\n  %s\n\n"
                   "  Rebuild the tree you want judged, then judge it:\n"
                   "    python tools/arifi-sync/arifi_sync.py build\n"
                   "    python tools/arifi-sync/arifi_sync.py judge --i-have-read-bench-purity"
                   % "\n  ".join(binp))
    say("build identity: HEAD %s tree %s content %s (receipt %s)"
        % (now["head"][:9], now["tree"][:9], now["content_sha256"][:12],
           receipt.get("built_utc", "?")))
    say("artifacts     : %d hash(es) verified, judge binary %s"
        % (len(recorded), os.path.relpath(binary, bdir).replace("\\", "/")))
    model = j["model"]
    if model.startswith("SET-ME"):
        raise Loud("judge.model is not configured in sources.json. Set it to the absolute path\n"
                   "of the regression model gguf. This tool will not pick a model for you.")
    if not os.path.exists(model):
        raise Loud("judge model not found: %s" % model)
    say("judge binary : %s" % binary)
    say("judge model  : %s" % model)
    say("judge args   : %s" % " ".join(j["args"]))
    if "--no-host" not in j["args"]:
        say("\nWARNING: --no-host is absent. If any CPU_REPACK path is under test, both arms will")
        say("silently exercise the same code and the comparison proves nothing.")
    if not args.i_have_read_bench_purity:
        say("\nSTOPPING before any timed run. Re-run with --i-have-read-bench-purity once you have")
        say("confirmed: no sibling build or inference is running, the box is otherwise idle, and")
        say("both arms will use the SAME binary. Bench purity: reuse a binary, never a number.")
        return 1
    # The harness (wired lane-110D, 2026-07-28): launch llama-server, wait for /health,
    # POST /completion, require HTTP 200 + non-empty finite output. Correctness gate,
    # not a bench — timings are reported but never compared to a historical number.
    import json as _json
    import time as _time
    import urllib.request as _rq
    port = int(j.get("port", 8199))
    prompt = j.get("prompt", "The capital of France is")
    n_predict = int(j.get("n_predict", 16))
    cmd = [binary, "-m", model, "--port", str(port), "--host", "127.0.0.1"] + list(j["args"])
    step("launching judge server (port %d)" % port)
    logpath = os.path.join(repo, "judge-server.log")
    logf = open(logpath, "w", encoding="utf-8", errors="replace")
    proc = subprocess.Popen(cmd, stdout=logf, stderr=subprocess.STDOUT)
    try:
        deadline = _time.time() + 180
        up = False
        while _time.time() < deadline:
            if proc.poll() is not None:
                raise Loud("judge server DIED during load (exit %s). Log: %s"
                           % (proc.returncode, logpath))
            try:
                with _rq.urlopen("http://127.0.0.1:%d/health" % port, timeout=2) as r:
                    if r.status == 200:
                        up = True
                        break
            except Exception:
                _time.sleep(1.0)
        if not up:
            raise Loud("judge server never became healthy within 180 s. Log: %s" % logpath)
        body = _json.dumps({"prompt": prompt, "n_predict": n_predict,
                            "temperature": 0}).encode()
        req = _rq.Request("http://127.0.0.1:%d/completion" % port, data=body,
                          headers={"Content-Type": "application/json"})
        t0 = _time.time()
        with _rq.urlopen(req, timeout=300) as r:
            if r.status != 200:
                raise Loud("judge /completion returned HTTP %d" % r.status)
            resp = _json.loads(r.read().decode("utf-8", errors="replace"))
        dt = _time.time() - t0
        content = resp.get("content", "")
        say("judge output : %r" % content[:120])
        timings = resp.get("timings", {})
        if timings:
            say("judge timings: prompt %.2f t/s, predict %.2f t/s (informational only — "
                "never compare to a historical number)"
                % (timings.get("prompt_per_second") or 0.0,
                   timings.get("predicted_per_second") or 0.0))
        if not content.strip():
            raise Loud("judge FAILED: empty completion. Log: %s" % logpath)
        say("judge PASS   : llama-server loaded the model and produced a completion "
            "(%.1f s round-trip)" % dt)
        return 0
    finally:
        proc.kill()
        try:
            proc.wait(timeout=30)
        except Exception:
            pass
        logf.close()


# ------------------------------------------------- protected wins (President mandate 2026-09-04)
#
# Authority: registers/2026-09-04-local-inference-win-preservation-mandate.md, VERBATIM:
#
#   "a win is a win even if it's small. Never throw away. ... anything from upstream of llama.cpp
#    or the forks we ingest, must not overwrite what we have, we must compare and make sure we
#    keep the winning formula, maybe something upstream from anyone might be better than ours but
#    we must confirm/test be aware of those changes so we dont overwrite the specific win"
#
# Mandate assignment 2 is this manifest; assignment 3 is `cmd_protected_win_check`, the
# fail-closed preflight; assignment 4 is UPDATE-RUNBOOK 4.1c, which drives both by name.
#
# The manifest is AUTHORED, not generated, because the four fields that make an entry useful -
# protected paths, semantic anchors, the quality condition, and the workload the number was
# measured on - are not derivable from a commit trailer. What IS derivable is the CANDIDATE SET,
# and `discover` prints it: any commit with a real Measured-effect that no entry covers. So a
# proven win cannot quietly stay unregistered, and no field is ever invented to fill a row.

PROTECTED_WINS = "protected-wins.json"
PROTECTED_RESOLUTIONS = "protected-win-resolutions.json"
PROTECTED_BASELINE = "protected-win-baseline.json"

WIN_REQUIRED = ("id", "status", "mechanism", "commit", "protected_paths", "anchors",
                "evidence", "quality_gate", "workload", "measured_effect")
WIN_STATUSES = ("active", "superseded")
RESOLUTION_REQUIRED = ("win_id", "incoming", "verdict", "arms", "quality", "evidence",
                       "decided_utc", "decided_by")
RESOLUTION_VERDICTS = ("keep-ours", "adopt-upstream", "adopt-union")
# A dash alone is the placeholder for "no effect stated". It used to live in NO_EFFECT_PREFIXES,
# where it was a PREFIX test - so `Measured-effect: -12.79% per dispatch` classified as NO EFFECT.
# Every speedup this fork writes as a negative delta was therefore invisible to discovery: the one
# shape a win is most likely to take was the one shape the classifier could not see. Caught
# 2026-09-04 by the WI-1688 fail-closed tests, on a fixture whose effect read `-9.4% per dispatch`.
# It is an EXACT match now, and `test_a_negative_delta_is_a_real_measured_effect` keeps it one.
NO_EFFECT_EXACT = ("-", "--", "n/a", "none", "tbd")
NO_EFFECT_PREFIXES = ("unmeasured", "none", "n/a", "documentation only", "docs only",
                      "no code", "no runtime", "generated artifact", "packaging only",
                      "analysis only", "comment only", "tooling and documentation",
                      "tooling only", "build fix only", "build-system only", "build variant",
                      "ci contract only", "process guardrail", "file relocation only",
                      "structure and tooling", "default-only change", "enables the",
                      "kernel dispatch only", "correctness fix", "correctness only",
                      "documents placement",
                      "build-variant documentation", "source census", "clean fixture",
                      "harness selftest", "series check pass", "provenance audit",
                      "'currency' subcommand")


def _load_json(repo: str, name: str, default=None):
    p = os.path.join(repo, "tools", "arifi-sync", name)
    if not os.path.exists(p):
        if default is None:
            raise Loud("%s is missing. The protected-win preflight is FAIL-CLOSED: without the\n"
                       "  manifest it cannot know what an incoming change would overwrite, so it\n"
                       "  refuses rather than passing vacuously." % name)
        return default
    with open(p, "r", encoding="utf-8-sig") as fh:
        return json.load(fh)


# ------------------------------------------------- candidate identity (lane-229 / HQ-57)
#
# THE WRONG-TREE TRAP, reproduced by HQ 2026-09-11: every manifest and the config itself are read
# from the WORKING TREE of whatever checkout the script file lives in - `main()` derives `repo` from
# the directory of `--config`, and `_load_json` joins `repo`. `--ref` names a git ref, whose tree is
# a different thing entirely. So `protected-win validate --ref arifi/main` run from the lane-206
# checkout validated lane-206's 21-entry manifest against arifi/main's tree, while arifi/main's own
# manifest carries 30 entries. It printed a PASS-shaped report about a ref it had never read.
#
# A stale checkout's manifest may not certify another ref. The guard below is a REFUSAL, not a
# warning row: once the identity is in doubt every count below it is about the wrong tree, so
# nothing is printed that could be mistaken for a verdict.
#
# Compared as PARSED JSON, deliberately, not as bytes. These files are read `utf-8-sig` and a
# Windows checkout may hold a BOM or CRLF that the blob does not; a byte compare would refuse a
# legitimate invocation over a line ending. Content is the thing under test.

# `replay-provenance.json` is in this set for the same reason the manifests are: it decides whether
# a commit counts as registered, so a copy that exists only on disk could carry a registration that
# the ref itself does not have. It is a TRACKED file of the tree it describes, or it is not evidence.
CANDIDATE_FILES = ("sources.json", PROTECTED_WINS, PROTECTED_BASELINE, PROTECTED_RESOLUTIONS,
                   "native-grandfather.json", "replay-provenance.json")


def _canon_json(obj) -> str:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=False)


def _worktree_json(repo: str, name: str):
    p = os.path.join(repo, "tools", "arifi-sync", name)
    if not os.path.exists(p):
        return None
    with open(p, "r", encoding="utf-8-sig") as fh:
        return json.load(fh)


def _ref_json(repo: str, ref: str, name: str):
    rc, out, _ = git(repo, "show", "%s:tools/arifi-sync/%s" % (ref, name), check=False)
    if rc != 0:
        return None
    return json.loads(out.lstrip("﻿"))


def candidate_identity_problems(repo: str, ref: str) -> list:
    """Divergences between the config/manifests ON DISK and the same files AT `ref`."""
    problems = []
    for name in CANDIDATE_FILES:
        live = _worktree_json(repo, name)
        at_ref = _ref_json(repo, ref, name)
        if live is None and at_ref is None:
            continue
        if live is None:
            problems.append("%s exists at %s but NOT in the checkout at %s" % (name, ref, repo))
            continue
        if at_ref is None:
            problems.append("%s exists in the checkout at %s but NOT at %s" % (name, repo, ref))
            continue
        if _canon_json(live) != _canon_json(at_ref):
            extra = ""
            if name == PROTECTED_WINS:
                extra = " (checkout: %d entr(ies); %s: %d)" % (
                    len(live.get("wins") or []), ref, len(at_ref.get("wins") or []))
            problems.append("%s in the checkout DIFFERS from %s:tools/arifi-sync/%s%s"
                            % (name, ref, name, extra))
    return problems


DRAFT_SNAPSHOT_MSG = "arifi-sync draft snapshot (not committed to any branch)"


DRAFT_MAX_NEW_BYTES = 1 << 20


def _untracked_nonignored(repo: str) -> list:
    """Every untracked path git would NOT ignore, `-z` parsed. Ignored files are already absent
    from this list (`--exclude-standard`), which is why a build directory or a model file does not
    have to be authorised."""
    _, raw, _ = git(repo, "ls-files", "--others", "--exclude-standard", "-z")
    return sorted(p for p in raw.split("\0") if p)


def draft_snapshot(repo: str, allow_new=()) -> str:
    """A real, nameable commit object for the CURRENT WORKING TREE.

    The registration workflow this restores (CHECK-UPDATE-ROOT finding 5): author a manifest entry,
    validate it, THEN commit. `require_candidate_identity` correctly refuses to certify a ref with
    manifests that are not that ref's - but that also refused the uncommitted draft against every
    ref including HEAD, so the only way to validate a new entry was to commit it first and find out
    afterwards. The printed `--ref HEAD` repair could not work for the same reason.

    Nothing is relaxed here and no other ref is certified. The worktree is written into a THROWAWAY
    INDEX (`GIT_INDEX_FILE`, so the real index is untouched) and committed with `commit-tree` onto
    HEAD. The result is a dangling commit - no branch, no ref, nothing published - that IS the
    draft, so root binding and content identity both hold against it exactly as they would against
    a branch, and the whole validator runs unmodified.

    WHAT IT WILL NOT DO (CHECK-UPDATE-ROOT-R2 finding 7). This used to run `git add -A -- .`, which
    swept every untracked non-ignored file in the checkout into a real Git object. A dangling
    commit is unreachable, not absent: the blobs are written into `.git/objects` and survive until
    a gc that may never come, so an unrelated private file left in the tree was silently and
    durably stored by a command whose whole promise is that it publishes nothing. The default is
    now a REFUSAL, taken BEFORE any object is written. Tracked modifications and deletions are
    staged with `git add -u`, which cannot pick up a new path at all.

    A genuinely new candidate file - a first-ever manifest, say - is supported, but only by
    EXPLICIT bounded authorisation: each path is named on the command line, must exist, must be
    untracked, and must be under `DRAFT_MAX_NEW_BYTES`. Nothing is included by pattern, by
    directory sweep, or by size alone.
    """
    allowed, missing, oversize, not_new = [], [], [], []
    untracked_now = set(_untracked_nonignored(repo))
    for raw in allow_new:
        rel = str(raw).replace("\\", "/").strip()
        while rel.startswith("./"):
            rel = rel[2:]
        if not rel:
            continue
        full = os.path.join(repo, rel.replace("/", os.sep))
        if not os.path.isfile(full):
            missing.append(rel)
            continue
        if rel not in untracked_now:
            not_new.append(rel)
            continue
        if os.path.getsize(full) > DRAFT_MAX_NEW_BYTES:
            oversize.append("%s (%d bytes > %d)" % (rel, os.path.getsize(full), DRAFT_MAX_NEW_BYTES))
            continue
        allowed.append(rel)
    if missing or oversize or not_new:
        raise Loud(
            "--allow-new was given path(s) it cannot honour, so nothing was snapshotted:\n"
            "%s%s%s"
            "  An authorisation names one existing, untracked, bounded file. It is not a pattern."
            % ("".join("    does not exist: %s\n" % p for p in missing),
               "".join("    not untracked (already tracked, or ignored): %s\n" % p for p in not_new),
               "".join("    too large: %s\n" % p for p in oversize)))

    extra = [p for p in _untracked_nonignored(repo) if p not in set(allowed)]
    if extra:
        raise Loud(
            "DRAFT REFUSED: %d untracked, non-ignored file(s) are in the checkout. Nothing was\n"
            "  written to the object store - this refusal happens before any object is created.\n\n"
            "  A draft snapshot writes a real commit, and a commit contains the bytes of every\n"
            "  file in it. Sweeping in whatever else happens to be in the tree would durably store\n"
            "  unrelated content in .git/objects, so the untracked set has to be named, not\n"
            "  guessed:\n%s%s\n"
            "  Either remove or ignore these, or authorise the ones that are genuinely part of the\n"
            "  candidate, by name (each must be under %d bytes):\n"
            "    python tools/arifi-sync/arifi_sync.py protected-win validate --draft%s"
            % (len(extra),
               "".join("    %s\n" % p for p in extra[:20]),
               "    ... and %d more\n" % (len(extra) - 20) if len(extra) > 20 else "",
               DRAFT_MAX_NEW_BYTES,
               "".join(" --allow-new %s" % p for p in extra[:3])))

    fd, idx = tempfile.mkstemp(prefix="arifi-draft-index-")
    os.close(fd)
    os.remove(idx)  # git insists on creating the index file itself
    env = dict(os.environ, GIT_INDEX_FILE=idx)
    try:
        git(repo, "read-tree", "HEAD", env=env)
        # `-u` updates TRACKED paths only, including deletions. It cannot add a new path, which is
        # what makes the authorisation below the only route by which a new file can enter.
        git(repo, "add", "-u", "--", ".", env=env)
        for rel in allowed:
            git(repo, "add", "--", rel, env=env)
        tree = git(repo, "write-tree", env=env)[1].strip()
        return git(repo, "commit-tree", tree, "-p", "HEAD", "-m", DRAFT_SNAPSHOT_MSG,
                   env=env)[1].strip()
    finally:
        # The exact paths mkstemp gave us, never a reconstructed one.
        for p in (idx, idx + ".lock"):
            if os.path.exists(p):
                os.remove(p)


def require_candidate_identity(repo: str, ref: str) -> None:
    """Refuse to certify `ref` with another checkout's config/manifests. Fail-closed."""
    if _resolve_ref(repo, ref) == "":
        raise Loud("--ref %s does not resolve in %s. The candidate has to be nameable in the\n"
                   "  repository whose manifests are being read, or the two are not the same thing."
                   % (ref, repo))
    problems = candidate_identity_problems(repo, ref)
    if not problems:
        return
    # Never print a repair that cannot work. `--ref HEAD` only helps when the manifests on disk ARE
    # HEAD's; when they are an uncommitted draft, HEAD refuses for exactly the same reason and the
    # operator is sent in a circle. Ask which case this is, and name the one that applies.
    if candidate_identity_problems(repo, "HEAD"):
        repair_b = ("  (b) the manifests on disk are an UNCOMMITTED DRAFT - they differ from HEAD\n"
                    "      too, so NO ref can certify them and --ref HEAD would refuse identically.\n"
                    "      Validate the draft ITSELF. This snapshots the working tree into a\n"
                    "      throwaway commit and validates that, committing and publishing nothing:\n"
                    "        python tools/arifi-sync/arifi_sync.py protected-win validate --draft\n")
    else:
        repair_b = ("  (b) you meant to certify this checkout, not that ref:\n"
                    "        python tools/arifi-sync/arifi_sync.py protected-win validate --ref HEAD\n")
    raise Loud(
        "WRONG-TREE REFUSAL: the config/manifests on disk are not the ones at %s, so they cannot\n"
        "certify it. Every count this command would print would be about a tree it never read.\n\n"
        "  checkout : %s\n"
        "  candidate: %s (%s)\n\n"
        "  %s\n\n"
        "Repair - run the command from a checkout OF the candidate, whichever is true:\n"
        "  (a) the checkout IS the candidate and is merely behind:\n"
        "        git -C %s switch %s && git -C %s pull --ff-only\n"
        "%s"
        "  (c) the candidate lives in another checkout - run ITS copy of this script:\n"
        "        python <candidate-checkout>/tools/arifi-sync/arifi_sync.py ... --ref %s\n"
        % (ref, repo, ref, _resolve_ref(repo, ref)[:9], "\n  ".join(problems), repo, ref, repo,
           repair_b, ref))


# ------------------------------------------------- build/judge tree identity (lane-229 / HQ-57)
#
# `cmd_build` and `cmd_judge` act on `repo`'s own build dir. Nothing recorded WHICH source tree the
# binaries in it came from, so a build of the old base and a judge run minutes later against a
# freshly switched tree were indistinguishable from a build and judge of the same thing. The
# receipt below closes that: `build` writes the identity it built, `judge` refuses any binary whose
# receipt is missing or describes a different tree. The status hash is part of the identity because
# a dirty tree is not described by its HEAD tree sha.

BUILD_RECEIPT = "arifi-build-identity.json"


def _sha256_file(path: str) -> str:
    import hashlib as _hl
    h = _hl.sha256()
    with open(path, "rb") as fh:
        for blk in iter(lambda: fh.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()


def _canon(path: str) -> str:
    """One canonical spelling: symlinks resolved, `..` collapsed, case folded on Windows.
    Two spellings of one directory must never read as two directories, and vice versa."""
    return os.path.normcase(os.path.normpath(os.path.realpath(path)))


def _under(child: str, parent: str) -> bool:
    c, p = _canon(child), _canon(parent)
    return c == p or c.startswith(p + os.sep)


def _strict_descendant(child: str, parent: str) -> bool:
    """`child` is INSIDE `parent` and is not `parent` itself.

    `_under` accepts equality, and that acceptance was a hole (CHECK-UPDATE-ROOT-R2 finding 1):
    `build_dir: "."` satisfied every root check, and `_status_entries` then skipped the whole
    checkout - so an in-source build excluded every dirty source path from the identity that
    certifies it. The identity said `dirty: False` over a tree with edited sources. A build
    directory has to be a STRICT descendant, so that skipping it can never skip a source input.
    """
    c, p = _canon(child), _canon(parent)
    return c != p and c.startswith(p + os.sep)


def _status_entries(repo: str, skip_dir: str = None) -> list:
    """`[(xy, path, origin)]` from `status --porcelain -z -uall`, parsed, never split on newlines.

    `-z` is mandatory: a path with a space or a quote is mangled by the default format, and a
    rename entry carries its ORIGIN path as a second NUL-separated field. Miss that second field
    and every entry after a rename is read as a status line, which desynchronizes the whole parse
    into a stable-but-wrong digest.

    `skip_dir` drops the build directory. The receipt, the object files and the linker's scratch
    live there; without this the build's own output would be an input to the identity that
    certifies it, and no build could ever match its own receipt.

    A `skip_dir` that is not a STRICT descendant of the checkout is REFUSED here rather than
    honoured. The exclusion exists to drop generated output; pointed at the checkout itself (or at
    an ancestor of it) it drops the source instead, and the identity then certifies a tree it never
    looked at. `build_root_problems` refuses the same configuration before the compiler, but
    `cmd_judge` samples the identity before it reaches that check, so the refusal lives at the
    point of use as well as at the point of configuration.
    """
    if skip_dir and not _strict_descendant(skip_dir, repo):
        raise Loud(
            "the build directory %s is not a strict descendant of the checkout %s, so excluding it\n"
            "  from the source identity would exclude source inputs. An in-source build (build_dir\n"
            "  '.', or any directory containing the checkout) is refused: its exclusion would make\n"
            "  every dirty source file invisible to the identity that certifies the binaries.\n"
            "  Configure a build directory INSIDE the checkout, e.g. build.build_dir = \"build-vulkan\"."
            % (_canon(skip_dir), _canon(repo)))
    _, raw, _ = git(repo, "status", "--porcelain", "-z", "--untracked-files=all")
    fields = raw.split("\0")
    out, i = [], 0
    while i < len(fields):
        entry = fields[i]
        i += 1
        if not entry:
            continue
        xy, path = entry[:2], entry[3:] if len(entry) > 3 else entry[2:].lstrip()
        origin = ""
        if "R" in xy or "C" in xy:
            origin = fields[i] if i < len(fields) else ""
            i += 1
        if skip_dir and _under(os.path.join(repo, path.replace("/", os.sep)), skip_dir):
            continue
        out.append((xy, path, origin))
    return out


def _content_sha256(repo: str, entries: list) -> str:
    """Hash of the ACTUAL BYTES the compiler will read, for every path that is not at HEAD.

    THE DIRTY-BYTES TRAP (CHECK-UPDATE-ROOT finding 1, reproduced 2026-09-11): the identity used
    to be HEAD + HEAD's tree + a hash of the `status` LISTING. A listing names which files
    changed; it never says what they now contain. Editing an already-modified `source.c` from v1
    to v2 left all three values identical, so a receipt written before the edit certified the
    binary built after it, and the comment claiming a mid-build edit could not be certified was
    false. The build input is the bytes, so the bytes are what is hashed.
    """
    import hashlib as _hl
    h = _hl.sha256()
    for xy, path, origin in entries:
        h.update(("%s\0%s\0%s\0" % (xy, path, origin)).encode("utf-8"))
        full = os.path.join(repo, path.replace("/", os.sep))
        if os.path.isfile(full):
            h.update(b"F" + _sha256_file(full).encode("ascii"))
        else:
            # deleted, or a submodule/directory git collapsed: recorded as an absence, which is
            # itself a state the next run has to match.
            h.update(b"ABSENT")
    return h.hexdigest()


def _tree_identity(repo: str, build_dir: str = None) -> dict:
    import hashlib as _hl
    entries = _status_entries(repo, build_dir)
    listing = "".join("%s %s %s\n" % e for e in entries)
    return {"head": gout(repo, "rev-parse", "HEAD"),
            "tree": gout(repo, "rev-parse", "HEAD^{tree}"),
            "status_sha256": _hl.sha256(listing.encode("utf-8")).hexdigest(),
            "content_sha256": _content_sha256(repo, entries),
            "dirty": bool(entries)}


def _identity_matches(a: dict, b: dict) -> bool:
    return all(a.get(k) == b.get(k)
               for k in ("head", "tree", "status_sha256", "content_sha256"))


# ------------------------------------------------- what the compiler is actually pointed at
#
# A CMakeCache in the right directory does not mean it configures THIS checkout. `parse_cache`
# drops INTERNAL entries, and `CMAKE_HOME_DIRECTORY` - the source root cmake will build - is an
# INTERNAL entry, so nothing ever read it. A build directory holding a cache that names another
# checkout compiled that other checkout's sources and then wrote THIS repository's receipt
# (CHECK-UPDATE-ROOT finding 2). The build directory itself can also be absolute or traverse out
# of the tree. Both are canonicalized and bound here, before the compiler is invoked.

def _cache_internal(path: str, key: str):
    """One raw cache entry INCLUDING the INTERNAL class that `parse_cache` deliberately drops."""
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.strip()
            if line.startswith(key + ":") and "=" in line:
                return line.split("=", 1)[1].strip()
    return None


def canonical_cache(repo: str, cfg: dict) -> str:
    """THE cache: the `CMakeCache.txt` inside the build directory the compiler is handed.

    `cmd_build` invokes `cmake --build <build_dir>`, so that directory's cache is the configuration
    that will actually be compiled. Everything that reasons about the recipe has to read THAT file
    and no other."""
    return os.path.join(os.path.join(repo, cfg["build"]["build_dir"]), "CMakeCache.txt")


def build_root_problems(repo: str, cfg: dict) -> list:
    bdir = os.path.join(repo, cfg["build"]["build_dir"])
    out = []
    if not _under(bdir, repo):
        out.append("build directory %s is OUTSIDE the checkout %s. A build dir that is not in the "
                   "tree cannot be certified as this tree's build." % (_canon(bdir), _canon(repo)))
    elif not _strict_descendant(bdir, repo):
        # CHECK-UPDATE-ROOT-R2 finding 1. `build_dir: "."` passed every previous check: it is
        # `_under` the checkout, its CMakeCache names the right source root, and the identity then
        # excluded the entire tree as "the build directory". The dirty-source guard was switched
        # off by configuration alone, silently, with `dirty: False` printed over edited sources.
        out.append(
            "build directory %s IS the checkout %s (or contains it). A build directory is excluded "
            "from the source identity, so an in-source build would exclude every source file from "
            "the identity that certifies its own binaries. Use a strict subdirectory, e.g. "
            "\"build-vulkan\"." % (_canon(bdir), _canon(repo)))
    else:
        # The exclusion may only drop GENERATED output. A build directory that holds tracked files
        # at HEAD is a source directory wearing a build directory's name, and excluding it hides
        # real source edits exactly as an in-source build would.
        rel = os.path.relpath(_canon(bdir), _canon(repo)).replace("\\", "/")
        rc, tracked, _ = git(repo, "ls-tree", "-r", "--name-only", "HEAD", "--", rel, check=False)
        names = [l for l in tracked.splitlines() if l.strip()] if rc == 0 else []
        if names:
            out.append(
                "build directory %s holds %d TRACKED file(s) at HEAD (e.g. %s). The build directory "
                "is excluded from the source identity, so a directory carrying source would remove "
                "that source from the identity. Build into a generated, untracked directory."
                % (_canon(bdir), len(names), ", ".join(names[:3])))
    # CHECK-UPDATE-ROOT-R2 finding 2: the recipe was compared against `build.live_cache`, which is
    # a SEPARATE config key from `build.build_dir`. A decoy cache at `live_cache` could match the
    # recorded recipe while the cache cmake would actually build lacked a required flag - the diff
    # passed, the compiler ran, and a receipt was published over a configuration nobody compared.
    # The recipe is bound to the canonical cache here, before the compiler and again in `judge`.
    declared = cfg["build"].get("live_cache")
    if declared:
        live = os.path.join(repo, declared)
        if _canon(live) != _canon(canonical_cache(repo, cfg)):
            out.append(
                "build.live_cache resolves to %s, but the cache cmake will build from is %s. The "
                "recipe comparison would certify a cache the compiler never reads. Point "
                "build.live_cache at the CMakeCache.txt inside build.build_dir."
                % (_canon(live), _canon(canonical_cache(repo, cfg))))
    home = _cache_internal(os.path.join(bdir, "CMakeCache.txt"), "CMAKE_HOME_DIRECTORY")
    if not home:
        out.append("CMakeCache.txt in %s states no CMAKE_HOME_DIRECTORY, so the source root cmake "
                   "would compile is unknown. Reconfigure the build dir from the recorded recipe."
                   % bdir)
    elif _canon(home) != _canon(repo):
        out.append("the configured CMake source root is %s, but this checkout is %s. The compiler "
                   "would build another tree and the receipt would name this one."
                   % (_canon(home), _canon(repo)))
    return out


# ------------------------------------------------- the artifacts a verdict is actually about
#
# The receipt used to carry source metadata only, so a manually replaced or partially rebuilt
# llama-server.exe kept its certification (CHECK-UPDATE-ROOT finding 3). `build` now records the
# bytes of the judge binary and of the shared libraries beside it; `judge` re-hashes them.

JUDGE_BINARY_CANDIDATES = ("bin/llama-server.exe", "bin/llama-server",
                           "bin/Release/llama-server.exe")
_RUNTIME_SUFFIXES = (".exe", ".dll", ".so", ".dylib")


def _judge_binary(bdir: str):
    for rel in JUDGE_BINARY_CANDIDATES:
        p = os.path.join(bdir, rel.replace("/", os.sep))
        if os.path.exists(p):
            return p
    return None


def _is_runtime_artifact(name: str) -> bool:
    low = name.lower()
    return low.endswith(_RUNTIME_SUFFIXES) or ".so." in low


_RUNTIME_LIBRARY_SUFFIXES = (".dll", ".so", ".dylib")


def _is_runtime_library(name: str) -> bool:
    """A shared library the loader may resolve beside the judge binary.

    Separated from `_is_runtime_artifact` because the two classes get different rules
    (CHECK-UPDATE-ROOT-R2 finding 4). An extra EXECUTABLE beside the server is ordinary - building
    `llama-cli` later must not brick the judge - but an extra LIBRARY is not: on Windows the
    directory of the executable is searched first, so dropping a new `ggml-vulkan.dll` beside
    llama-server changes what the process loads without changing one recorded byte."""
    low = name.lower()
    return low.endswith(_RUNTIME_LIBRARY_SUFFIXES) or ".so." in low


def artifact_key(bdir: str, binary: str) -> str:
    return os.path.relpath(binary, bdir).replace("\\", "/")


def artifact_digest(bdir: str, binary: str) -> dict:
    """sha256 per artifact, keyed by path relative to the build dir.

    Scoped to the directory of the RESOLVED binary, because the candidate list spans `bin/` and
    `bin/Release/` and a build that scanned one while judge resolved the other would refuse every
    real run. `.pdb`/`.ilk`/`.exp` are out on purpose: relink churn, never what executes.

    The EXPLICITLY SELECTED binary is hashed unconditionally, whatever its name. `bin/llama-server`
    is a supported candidate and carries no suffix, so the suffix filter silently dropped it: a
    build that selected it produced a receipt with no hash for its own judge binary, and `judge`
    then refused that build forever (CHECK-UPDATE-ROOT-R2 finding 4). Selection is the authority
    here; the suffix filter only decides what ELSE in the directory comes along."""
    out = {}
    d = os.path.dirname(binary)
    for name in sorted(os.listdir(d)):
        p = os.path.join(d, name)
        if os.path.isfile(p) and _is_runtime_artifact(name):
            out[artifact_key(bdir, p)] = _sha256_file(p)
    if os.path.isfile(binary):
        out[artifact_key(bdir, binary)] = _sha256_file(binary)
    return out


def artifact_problems(bdir: str, binary: str, recorded: dict) -> list:
    """The recorded artifacts must still be present and byte-identical, AND the adjacent runtime
    LIBRARY set must be the one the build recorded.

    Three mutations invalidate a receipt: a recorded artifact whose bytes changed, a recorded
    artifact that is gone, and a library that has APPEARED. The third was permitted until
    CHECK-UPDATE-ROOT-R2 finding 4: only recorded paths were checked, so a new DLL dropped beside
    the server passed every gate while being first in the loader's search order. An added
    EXECUTABLE is still permitted - that is an ordinary second target, and it is not on any
    loader path."""
    key = artifact_key(bdir, binary)
    now = artifact_digest(bdir, binary)
    out = []
    if key not in recorded:
        out.append("the receipt records no hash for the judge binary %s - it was not the binary "
                   "this build produced." % key)
    for k in sorted(recorded):
        if k not in now:
            out.append("%s is recorded in the receipt but is MISSING from the build directory - "
                       "a failed or partial rebuild." % k)
        elif now[k] != recorded[k]:
            out.append("%s has DIFFERENT BYTES than the build recorded - the binary was replaced "
                       "or rebuilt outside this tool." % k)
    for k in sorted(now):
        if k not in recorded and _is_runtime_library(os.path.basename(k)):
            out.append("%s is an ADDED runtime library that this build never produced. It sits in "
                       "the judge binary's own directory, which the loader searches first, so it "
                       "can change what the process loads without changing a recorded byte." % k)
    return out


def _drop_receipt(rpath: str) -> None:
    if os.path.exists(rpath):
        os.remove(rpath)


def _write_receipt(rpath: str, ident: dict) -> None:
    """Atomic publish: a half-written receipt must never be readable as a certification."""
    tmp = rpath + ".tmp"
    with open(tmp, "w", encoding="utf-8") as fh:
        json.dump(ident, fh, indent=2, sort_keys=True)
        fh.write("\n")
    os.replace(tmp, rpath)


def load_protected_wins(repo: str) -> list:
    data = _load_json(repo, PROTECTED_WINS)
    wins = data.get("wins", [])
    if not wins:
        raise Loud("%s carries zero entries. FAIL-CLOSED: an empty manifest is indistinguishable\n"
                   "  from a manifest nobody wrote, so it is refused rather than trusted." % PROTECTED_WINS)
    return wins


def _anchor_present(repo: str, ref: str, anchor: str) -> bool:
    """SOURCE-ONLY. Every home that merely RECORDS an anchor is excluded, and that is load-bearing.

    An anchor names a mechanism in live code. A whole-tree grep cannot tell the mechanism from a
    file that only writes the mechanism's name down, so after a fork ingest deleted the branch from
    the source file the grep still hit - in our own record - and `validate` reported the manifest
    healthy over a win that no longer existed. A green gate on a silent overwrite is the one failure
    this function exists to catch. Each excluded home is a record, never the mechanism:

    - `patches/series` (R6 finding 2): this fork bakes every mechanism into a generated
      `*.patch` by design, so the series always carries the anchor as removed-then-added text.
    - `tools/arifi-sync` (R6B, the blocking finding): `protected-wins.json` is TRACKED and lists
      every anchor literal by construction, so the grep had a guaranteed non-source hit for all 35
      of them - the rule could never report an anchor absent while its own manifest was committed.
      `protected-win-resolutions.json` and `protected-win-baseline.json` quote anchors the same way.
    - `*.md` (R6B, note N2): prose. This pattern has no FNM_PATHNAME, so it matches Markdown at
      ANY depth - which is what makes docs/CATALOG.md, README.md and CHANGELOG.md stop counting.
    - `docs` (R6B, note N2): the non-Markdown record files under the root docs tree; root-relative,
      so it does not reach a nested `<subsystem>/docs/`, which `*.md` already covers for prose.

    CHECKED lossless when written: all 35 current anchors still resolve in code under these four
    exclusions (`81-R6B-anchor-exclusion-probe.txt`). A future anchor that lives ONLY in prose
    would now fail validation - correctly, because prose is not a mechanism."""
    rc, _, _ = git(repo, "grep", "-q", "-F", anchor, ref,
                   "--", ".", ":(exclude)patches/series", ":(exclude)tools/arifi-sync",
                   ":(exclude)docs", ":(exclude)*.md", check=False)
    return rc == 0


# ------------------------------------------------- symbols: a DEFINITION, not a mention (WI-1700)
#
# An anchor proves a STRING is still in source. It does not prove a symbol still has a body. During
# the R13 b10819 sync `ggml_cuda_moe_cache_mmv_fused` was deleted from ggml-cuda/mmvq.cu while its
# declaration (mmvq.cuh) and its only caller (moe-cache.cu) both survived, and `validate` reported
# 41/41 anchors healthy over a tree that does not link on CUDA (WI-1699). This estate compiles Vulkan
# only, so nothing else could catch it. The `symbols` class closes that: an entry may name, per file,
# the functions/kernels that must have a DEFINITION in that file, and a declaration ending in `;` or
# a call site never satisfies it.

_DEF_TAIL_KEYWORDS = ("const", "noexcept", "override", "final", "__restrict__", "volatile")


def _close_paren(text: str, start: int, limit: int = 8000):
    """Index of the `)` that closes the `(` just before `start`, or None. Bounded so a stray `(`
    in a comment cannot make the scan quadratic over a 20k-line file."""
    depth = 1
    for i in range(start, min(len(text), start + limit)):
        c = text[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
    return None


_COMMENT_OR_STRING = re.compile(r'"(?:\\.|[^"\\\n])*"|/\*.*?\*/|//[^\n]*', re.S)


def _strip_comments(text: str) -> str:
    """Blank `//…$` and `/*…*/` so a commented-out body cannot read as a definition (WI-1700 check
    FIX-1). One left-to-right alternation, string literals kept verbatim: `/* http://x */` is one
    block comment, `// see /* x` is one line comment, and a `//` inside `"…"` is text. `#if 0`
    blocks are deliberately out of scope - the preprocessor is not a lexer's job."""
    return _COMMENT_OR_STRING.sub(lambda m: m.group(0) if m.group(0)[0] == '"' else " ", text)


def _definition_present(text: str, name: str) -> bool:
    """True when `text` DEFINES `name`: a definition-shaped match is `name` + `(` ... `)` followed,
    after optional trailing qualifiers, by `{` on the same or following lines - or an explicit
    template instantiation `template ... name<...>(...);`. What does NOT satisfy it, by
    construction: a `.cuh`/`.h` declaration (`)` is followed by `;`), a call site (`;` again, or the
    enclosing `)` of an `if`/`while` condition), a macro argument, a `[[host_name("name")]]`
    attribute (no `(` follows the name), and a body inside a `//` or `/* */` comment."""
    text = _strip_comments(text)
    pat = re.compile(r"(?<![\w.>])" + re.escape(name) + r"\s*(?:<[^;{}()]*>)?\s*\(")
    for m in pat.finditer(text):
        e = _close_paren(text, m.end())
        if e is None:
            continue
        tail = text[e + 1:e + 200].lstrip()
        changed = True
        while changed:
            changed = False
            for kw in _DEF_TAIL_KEYWORDS:
                if tail.startswith(kw):
                    tail = tail[len(kw):].lstrip()
                    changed = True
            am = re.match(r"__attribute__\s*\(\(.*?\)\)\s*", tail, re.S)
            if am:
                tail = tail[am.end():]
                changed = True
        if tail.startswith("{"):
            return True
    inst = re.compile(r"^\s*template\b[^;{]*?(?<![\w.>])" + re.escape(name) +
                      r"\s*<[^;{]*>\s*\([^;{]*\)\s*;", re.M)
    return inst.search(text) is not None


def _symbol_defined_at(repo: str, ref: str, path: str, name: str):
    """(exists, defined) for `name` in `ref:path`."""
    rc, blob, _ = git(repo, "show", "%s:%s" % (ref, path), check=False, binary=True)
    if rc != 0:
        return False, False
    text = blob.decode("utf-8", "ignore") if isinstance(blob, bytes) else blob
    return True, _definition_present(text, name)


def _symbol_survivors(repo: str, ref: str, path: str, name: str) -> list:
    """Where the name is STILL mentioned in source at `ref` - the declaration and the callers that
    make a lost definition a link failure rather than dead code. Same source-only exclusions as
    anchors; the file that should hold the definition is excluded too."""
    rc, out, _ = git(repo, "grep", "-n", "-F", name, ref,
                     "--", ".", ":(exclude)" + path, ":(exclude)patches/series",
                     ":(exclude)tools/arifi-sync", ":(exclude)docs", ":(exclude)*.md", check=False)
    if rc != 0:
        return []
    rows = []
    for line in out.splitlines():
        # `<ref>:<path>:<line>:<text>` - drop the ref prefix, keep path:line
        parts = line.split(":", 3)
        if len(parts) == 4:
            rows.append("%s:%s" % (parts[1], parts[2]))
    return rows[:6]


def _symbol_last_definer(repo: str, ref: str, path: str, name: str) -> str:
    """The last commit whose diff added or removed the symbol (`git log -S`). On a tree where the
    definition is gone that is the commit that put it there, which is what a repairer needs to read
    first. Scope widens file -> directory -> whole tree: with a single-file pathspec, history
    simplification drops the merge that deleted the body AND the commit that added it (CHECKED on
    697027562: the file scope returned nothing, the directory scope returned 1c3e9547a)."""
    scopes = [path, os.path.dirname(path) + "/", None]
    for scope in scopes:
        args = ["log", "-n1", "--format=%h %ad %s", "--date=short", "-m", "-S" + name, ref]
        if scope:
            args += ["--", scope]
        out = gout(repo, *args)
        if out:
            return "%s  [pickaxe scope: %s]" % (out, scope or "whole tree")
    return "(no commit in %s ever touched %r)" % (ref, name)


def _symbols_problems(repo: str, ref: str, wid: str, symbols) -> list:
    """Problems for one entry's `symbols` field. Shape: {"<file>": ["name", ...], ...}. A malformed
    field is itself a problem - fail closed, never skip silently."""
    out = []
    if not isinstance(symbols, dict) or not symbols:
        return ["%s: `symbols` must be a non-empty object {file: [names]} - got %r" % (wid, symbols)]
    for path, names in symbols.items():
        if not isinstance(names, list) or not names or not all(isinstance(n, str) and n for n in names):
            out.append("%s: symbols[%r] must be a non-empty list of names" % (wid, path))
            continue
        for name in names:
            exists, defined = _symbol_defined_at(repo, ref, path, name)
            if not exists:
                out.append("%s: symbols file %s does not exist at %s - the file that must DEFINE %r "
                           "is gone" % (wid, path, ref, name))
                continue
            if defined:
                continue
            survivors = _symbol_survivors(repo, ref, path, name)
            out.append(
                "%s: symbol %r has NO DEFINITION in %s at %s (WI-1700). A declaration or a call site "
                "is not a mechanism%s. Last commit that touched the definition: %s"
                % (wid, name, path, ref,
                   " - it is still mentioned at %s" % ", ".join(survivors) if survivors
                   else " - and nothing else in source mentions it either",
                   _symbol_last_definer(repo, ref, path, name)))
    return out


def _evidence_resolves(repo: str, locator: str) -> bool:
    """An evidence locator is a path or a `commit:<sha>`. Both are checked, never assumed.

    `commit:<sha>` is a first-class form on purpose: several wins carry their measurement table
    in the commit message itself (146f1bf8b's per-op ESCHA_MM grid is the clearest), and
    pointing at a report that paraphrases it would be weaker evidence, not stronger."""
    if locator.startswith("commit:"):
        return _resolve_ref(repo, locator.split(":", 1)[1]) != ""
    for root in (repo, os.path.dirname(os.path.dirname(os.path.dirname(repo.rstrip("/\\")))),
                 STUDIO_ROOT):
        if root and os.path.exists(os.path.join(root, locator)):
            return True
    return False


STUDIO_ROOT = "C:/ArifiLabs"


def validate_protected_wins(repo: str, ref: str, cfg=None) -> list:
    """Return the list of problems. Empty list = the manifest is complete AND still true.

    'Still true' is the half a schema check cannot do: a protected path that no longer exists,
    or an anchor that no longer appears in the tree, means the win was renamed or removed and
    the manifest is now guarding nothing. That is exactly the silent-overwrite the mandate is
    about, so it is a validation failure and not a warning.

    When `cfg` is supplied, STRICT REGISTRATION is also enforced (WI-1688): a commit that landed
    with a real `Measured-effect:` and is covered neither by a manifest entry nor by the dated
    discovery baseline is a problem, not a note. That is the enforcement gap `R2-CLOSURE-REPORT`
    6 named as the honest weak point of the previous pass - the manifest could be complete and
    still true while a win landed beside it completely unguarded. Because `cmd_protected_win_check`
    runs this function first and `cmd_bump` leg 1b runs `check`, wiring it HERE is what makes an
    unregistered win refuse the normal update path, with no parallel updater invented."""
    # Before a single entry is read: the manifest on disk has to BE the manifest at `ref`.
    # Everything below reads the working tree and compares it against `ref`; if those are two
    # different trees the whole report is about neither of them. See `require_candidate_identity`.
    require_candidate_identity(repo, ref)
    problems = []
    wins = load_protected_wins(repo)
    seen = set()
    for i, w in enumerate(wins):
        wid = w.get("id") or "<entry %d>" % i
        if wid in seen:
            problems.append("%s: duplicate id" % wid)
        seen.add(wid)
        status = w.get("status")
        if status not in WIN_STATUSES:
            problems.append("%s: status must be one of %s, got %r" % (wid, WIN_STATUSES, status))
        missing = [f for f in WIN_REQUIRED if not w.get(f)]
        if missing:
            problems.append("%s: INCOMPLETE - missing/empty %s" % (wid, ", ".join(missing)))
            continue
        if status == "superseded":
            for f in ("superseded_by", "comparison_evidence", "valid_conditions"):
                if not w.get(f):
                    problems.append("%s: superseded entries must keep %s - the mandate banks the "
                                    "superseded win with the conditions it still wins on" % (wid, f))
        if _resolve_ref(repo, w["commit"]) == "":
            problems.append("%s: commit %s does not resolve" % (wid, w["commit"]))
        for p in w["protected_paths"]:
            rc, _, _ = git(repo, "cat-file", "-e", "%s:%s" % (ref, p), check=False)
            if rc != 0:
                problems.append("%s: protected path %s does not exist at %s - the win was moved, "
                                "renamed or removed and this entry now guards nothing" % (wid, p, ref))
        for a in w["anchors"]:
            if not _anchor_present(repo, ref, a):
                problems.append("%s: anchor %r is absent from %s - the mechanism it names is gone "
                                "or was renamed" % (wid, a, ref))
        if "symbols" in w:
            problems.extend(_symbols_problems(repo, ref, wid, w["symbols"]))
        for e in w["evidence"]:
            if not _evidence_resolves(repo, e):
                problems.append("%s: evidence locator %r does not resolve" % (wid, e))
    if cfg is not None:
        problems.extend(baseline_boundary_problems(repo))
        problems.extend(replay_provenance(repo, cfg)[0])
        for sha, subject, eff in unregistered_measured_wins(repo, cfg, ref):
            problems.append(
                "UNREGISTERED measured win %s %r - it landed with `Measured-effect: %s` and is "
                "covered by no manifest entry and no baseline row. Register it in %s, or, if it "
                "is honestly not a protectable win, add it to %s with a written disposition. "
                "Never invent a field to close a row."
                % (sha[:9], subject[:70], " ".join(eff.split())[:90],
                   PROTECTED_WINS, PROTECTED_BASELINE))
    return problems


def cmd_protected_win_validate(repo: str, cfg: dict, args) -> int:
    ref = args.ref
    if getattr(args, "draft", False):
        # A draft validates THIS working tree. Certifying some other ref with it is the wrong-tree
        # trap wearing a new flag, so an explicitly named ref that is not HEAD is refused outright.
        if getattr(args, "ref_explicit", False) and \
                _resolve_ref(repo, args.ref) != _resolve_ref(repo, "HEAD"):
            raise Loud("--draft validates the UNCOMMITTED WORKING TREE, so it cannot also certify\n"
                       "  --ref %s. Drop one of the two: --draft for the tree in front of you,\n"
                       "  --ref %s for that committed candidate." % (args.ref, args.ref))
        allow_new = list(getattr(args, "allow_new", None) or [])
        ref = draft_snapshot(repo, allow_new)
        say("DRAFT snapshot   : %s  (working tree of %s, parent %s)"
            % (ref[:9], repo, gout(repo, "rev-parse", "--short", "HEAD")))
        say("                   Nothing was committed and no ref was created. The snapshot IS a")
        say("                   Git commit object, so its blobs are written into .git/objects and")
        say("                   stay there until a gc; that is why untracked files are refused")
        say("                   rather than swept in. Every gate below runs against it unmodified.")
        if allow_new:
            say("                   explicitly authorised new file(s): %s" % ", ".join(allow_new))
    step("Validating %s against %s" % (PROTECTED_WINS, ref))
    problems = validate_protected_wins(repo, ref, cfg)
    wins = load_protected_wins(repo)
    active = [w for w in wins if w.get("status") == "active"]
    say("entries          : %d  (%d active, %d superseded)"
        % (len(wins), len(active), len(wins) - len(active)))
    say("protected paths  : %d" % sum(len(w.get("protected_paths") or []) for w in wins))
    say("anchors          : %d" % sum(len(w.get("anchors") or []) for w in wins))
    say("symbols          : %d  (definitions required, WI-1700)"
        % sum(len(v) for w in wins for v in (w.get("symbols") or {}).values() if isinstance(v, list)))
    if problems:
        say("\nFAIL: %d problem(s):" % len(problems))
        for p in problems:
            say("  %s" % p)
        return 1
    say("\nPASS: every entry is complete and every path/anchor/evidence locator still resolves.")
    return 0


def _has_measured_quantity(value: str) -> bool:
    """True when the trailer states a NUMBER next to a THROUGHPUT/LATENCY UNIT.

    This is the half that cannot be laundered. A prefix test alone lets a real win hide behind any
    of the 30 NO_EFFECT_PREFIXES - `correctness fix, and -12.79% per dispatch` classified as NO
    EFFECT, and so did seven other shapes CHECKER-R3 reproduced (D2, 2026-09-04). It is the same
    class as the bare `-` prefix bug the previous pass fixed, on the other thirty prefixes.

    Units are unconditional where they can only mean a rate or a duration (`%`, `ms`, `us`, `µs`,
    `s/tok`, `t/s`, `tok/s`, `GB/s`, `MB/s`). A bare `x` multiplier is NOT unconditional: `2x16 GB`
    and `4x tile` are configuration, not effect, so `x` counts only when the number carries a sign
    or a decimal point (`-1.8x`, `+2.0x`). Plain `GB`/`MB` never count - they size a carve, not a
    speed."""
    v = value or ""
    if _MEASURED_QUANTITY.search(v):
        return True
    return bool(_MEASURED_MULTIPLIER.search(v))


_MEASURED_QUANTITY = re.compile(
    r"[-+−]?\d+(?:\.\d+)?\s*(?:%|ms\b|msec\b|us\b|usec\b|µs\b|s/tok\b|"
    r"tok/s\b|tokens?/s\b|t/s\b|[KMGT]B/s\b)",
    re.IGNORECASE)
_MEASURED_MULTIPLIER = re.compile(r"(?:[-+−]\d+(?:\.\d+)?|\d+\.\d+)\s*x\b", re.IGNORECASE)


def _measured_effect_is_real(value: str) -> bool:
    """Quantity FIRST, prefix list only as the tiebreak when no quantity is stated.

    Order is the whole fix (D2). The exact placeholders still win - `-`, `n/a`, `none` and friends
    state an absence and carry no number. After that, a stated measured quantity makes the value
    REAL no matter what words precede it, because a number with a throughput unit in a
    `Measured-effect:` trailer IS a measured effect; the prose in front of it is the author's
    framing, not a classification. Only when no quantity is present does the prefix vocabulary
    decide, which is what keeps `unmeasured on this seat` and `documentation only` quiet."""
    v = (value or "").strip().lower()
    if not v or v in NO_EFFECT_EXACT:
        return False
    if _has_measured_quantity(v):
        return True
    return not v.startswith(NO_EFFECT_PREFIXES)


def load_discovery_baseline(repo: str) -> dict:
    """Sha-pinned, dated disposition ledger for the measured-effect commits that were already
    landed when strict registration was switched on (WI-1688, 2026-09-04).

    Same shape and same reasoning as `load_grandfather`: the alternative to a baseline is either
    bricking `validate`/`check`/`bump` on 24 pre-existing rows, or - far worse - filing real wins
    under a soft reason to make the gate go quiet. Neither is acceptable, and the second one is
    the discard the President's mandate forbids. So the pre-existing set is recorded ONCE, with a
    written disposition per row, and strict registration then bites on everything AFTER it.

    Rows whose disposition is `REGISTRATION OWED` are real wins that are not yet fully described;
    they stay visible in the baseline and in `discover --all` until their entry can be written
    honestly. The baseline is CLOSED to new shas by convention - a new measured win must be
    registered, not baselined - and `protected-win discover` prints it so the ledger cannot rot
    unseen."""
    data = _load_json(repo, PROTECTED_BASELINE, {"commits": []})
    return {row["sha"]: row for row in data.get("commits", []) if row.get("sha")}


def baseline_boundary_problems(repo: str) -> list:
    """The baseline is closed BY MECHANISM now, not by convention (CHECKER-R3 residual, 2026-09-04).

    The hole this closes: `unregistered_measured_wins` skips any sha listed in the baseline, so
    appending one line to `protected-win-baseline.json` silenced the gate for a brand-new win
    forever. The previous pass disclosed that honestly rather than implying it away; disclosure is
    not a wall.

    The wall is the ledger's own `tip_when_recorded` field. The baseline exists to disposition what
    had ALREADY landed when strict registration was switched on, so every row must be an ANCESTOR
    of that tip. A commit made after the tip cannot have predated the gate, so it can never be
    grandfathered by appending its sha - it has to be REGISTERED. Genuine history is untouched:
    all 24 original rows are ancestors of `bda5865cb` and stay silent.

    Fail-closed on the field itself: a missing, unresolvable or emptied `tip_when_recorded` is a
    problem, not a pass. Otherwise deleting one line would restore the old hole."""
    data = _load_json(repo, PROTECTED_BASELINE, {"commits": []})
    rows = [r for r in data.get("commits", []) if r.get("sha")]
    if not rows:
        return []
    tip = (data.get("tip_when_recorded") or "").strip()
    if not tip:
        return ["%s: %d row(s) but no `tip_when_recorded` - the baseline boundary is what stops a "
                "post-gate win being grandfathered by appending its sha, so an absent boundary is "
                "refused rather than treated as 'no limit'." % (PROTECTED_BASELINE, len(rows))]
    tip_full = _resolve_ref(repo, tip)
    # `_resolve_ref` echoes a well-formed hex string back even when no such object exists, so the
    # existence of the commit is asked for separately. Otherwise a boundary of forty `f`s would
    # read as "resolved" and every row would then fail as a non-ancestor - a confusing refusal
    # instead of the true one.
    rc_tip, _, _ = git(repo, "cat-file", "-e", "%s^{commit}" % tip_full, check=False)
    if not tip_full or rc_tip != 0:
        return ["%s: `tip_when_recorded` %r does not resolve in this repo - the boundary cannot be "
                "checked, so it is refused." % (PROTECTED_BASELINE, tip)]
    problems = []
    for r in rows:
        sha = r["sha"]
        rc, _, _ = git(repo, "merge-base", "--is-ancestor", sha, tip_full, check=False)
        if rc != 0:
            problems.append(
                "%s: baseline row %s is NOT an ancestor of tip_when_recorded %s, so it landed "
                "AFTER strict registration was switched on and cannot be grandfathered. Register "
                "it in %s instead - the baseline dispositions pre-existing history only."
                % (PROTECTED_BASELINE, sha[:9], tip[:9], PROTECTED_WINS))
    return problems


# ------------------------------------------------- replay provenance (CHECK-UPDATE-ROOT-R2 6)
#
# THE REWRITTEN-COMMIT TRAP. An upstream bump is a REBASE: `git rebase --onto <upstream> <base>
# <ours>` replays every fork commit onto the new base, and every replayed commit gets a NEW SHA.
# `protected-wins.json` names the OLD sha. So the moment the documented repair sequence runs, the
# registered win becomes, to `unregistered_measured_wins`, a landed measured effect that no entry
# covers - and the fail-closed preflight refuses the very update it is there to protect. R2
# reproduced exactly that (`REAL_PREFLIGHT_REPAIRED_RC 1 UNREGISTERED_REFUSAL True`), and the R2
# test hid it by stubbing `cmd_protected_win_check` away.
#
# What is NOT the answer: matching on the subject line, fuzzy-matching, or relaxing registration.
# A message is not evidence - it is the one part of a commit an author types freely, and two
# commits can share it. The answer below is an EXPLICIT, MACHINE-READABLE, ONE-TO-ONE ledger, in
# which every row must survive re-derivation at use time:
#
#   * the ORIGINAL must still be registered in the manifest, or dispositioned in the baseline.
#     Nothing here can register anything; it can only carry an EXISTING registration across a
#     rewrite.
#   * the original must still be REACHABLE from a retained ref named in the row. A rebase leaves
#     the originals unreferenced, and "it was in the reflog" is not evidence anybody else can
#     check, so the operator retains a real ref and the row names it.
#   * the patch must be EQUIVALENT, by `git patch-id --stable` computed live on both sides. A
#     conflict-edited replay produces a different patch id and is refused - correctly, because a
#     patch somebody edited during a rebase is a new change that nobody measured.
#   * the trailers and the subject must correspond EXACTLY, both to each other and to the bytes
#     recorded in the row when it was written.
#   * the mapping must be ONE-TO-ONE. Two rows naming one original, or one rebased commit mapped
#     twice, is ambiguity, and ambiguity refuses.
#
# Every other gate keeps biting unchanged: protected paths, anchors, symbol definitions and the
# baseline ancestry boundary are all still evaluated against the CURRENT ref.

REPLAY_PROVENANCE = "replay-provenance.json"
REPLAY_ROW_REQUIRED = ("rebased", "original", "patch_id", "subject", "trailers",
                       "retained_ref", "recorded_utc", "recorded_by")


def _patch_id(repo: str, sha: str) -> str:
    """`git patch-id --stable` for one commit, or "" when it has no patch id.

    patch-id is the right equivalence here precisely because it is NOT a sha: it canonicalises
    away line numbers, whitespace and the blob hashes in the `index` lines, all of which change
    legitimately when a commit is replayed onto a different base. What it does NOT canonicalise
    away is the content of the change, which is the thing that has to be the same.

    It DOES hash context lines. A replay whose context shifted - upstream edited the lines next to
    ours, the rebase applied cleanly anyway - therefore yields a different id and is refused. That
    is a real limitation and it is deliberately left fail-closed: such a commit must be registered
    by hand, because its patch genuinely is not the patch that was measured."""
    p1 = subprocess.run(["git", "-C", repo, "diff-tree", "-p", "--no-commit-id", "--binary", sha],
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if p1.returncode != 0 or not p1.stdout.strip():
        return ""
    p2 = subprocess.run(["git", "-C", repo, "patch-id", "--stable"],
                        input=p1.stdout, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out = p2.stdout.decode("utf-8", "replace").split()
    return out[0] if out else ""


def _is_commit(repo: str, sha: str) -> bool:
    rc, _, _ = git(repo, "cat-file", "-e", "%s^{commit}" % sha, check=False)
    return rc == 0


def _dupes(values) -> set:
    seen, dup = set(), set()
    for v in values:
        (dup if v in seen else seen).add(v)
    return dup


def _replay_rows(repo: str) -> list:
    data = _load_json(repo, REPLAY_PROVENANCE, {"mappings": []})
    return [r for r in data.get("mappings", []) if isinstance(r, dict)]


def replay_provenance(repo: str, cfg: dict):
    """`(problems, {rebased_sha: original_sha})` for every row that survives re-derivation.

    A row that fails ANY check contributes a problem and maps nothing, so the rebased commit it
    names goes on to be reported as UNREGISTERED as well. Both messages are correct and both name
    the same manual action; nothing is accepted on the strength of the ledger alone."""
    rows = _replay_rows(repo)
    if not rows:
        return [], {}
    covered = covered_win_commits(repo)
    baseline = load_discovery_baseline(repo)
    problems, mapped = [], {}
    # ONE-TO-ONE is decided over the WHOLE ledger before any row is accepted. Detecting a duplicate
    # while walking would let the FIRST of two conflicting rows through and only refuse the second,
    # which is the opposite of what ambiguity means: when two rows disagree about which registered
    # win a commit is, neither is evidence. Both sides of every duplicate are dropped.
    dup_new = {s for s in _dupes(str(r.get("rebased")) for r in rows if r.get("rebased"))}
    dup_old = {s for s in _dupes(str(r.get("original")) for r in rows if r.get("original"))}
    for i, r in enumerate(rows):
        tag = "%s row %d" % (REPLAY_PROVENANCE, i + 1)
        bad = [f for f in REPLAY_ROW_REQUIRED if not r.get(f)]
        if bad:
            problems.append("%s: INCOMPLETE - missing/empty %s" % (tag, ", ".join(bad)))
            continue
        why = recorded_by_problem(str(r.get("recorded_by") or ""))
        if why:
            problems.append("%s: recorded_by is %s. The row states WHO carried this registration "
                            "across the rewrite, so it must be a concrete operator. Re-run "
                            "`replay-map --recorded-by <a real identity> --write`." % (tag, why))
            continue
        new_sha, old_sha = str(r["rebased"]), str(r["original"])
        tag = "%s (%s <- %s)" % (REPLAY_PROVENANCE, new_sha[:9], old_sha[:9])
        if new_sha in dup_new or old_sha in dup_old:
            problems.append(
                "%s: AMBIGUOUS - this rebased commit or this original appears in more than one "
                "row. Replay provenance must be one-to-one; a many-to-one map cannot say which "
                "registered win a commit is, so NEITHER side is accepted. Remove the wrong row, or "
                "register the commit directly in %s." % (tag, PROTECTED_WINS))
            continue
        if not _is_commit(repo, new_sha) or not _is_commit(repo, old_sha):
            problems.append("%s: one of the two shas is not a commit in this repository, so the "
                            "mapping cannot be checked." % tag)
            continue
        retained = str(r["retained_ref"])
        if _resolve_ref(repo, retained) == "" or not _is_commit(repo, retained):
            problems.append(
                "%s: retained_ref %r does not resolve. A rebase leaves the original commit "
                "unreferenced, so the evidence for this mapping is a REF that still contains it. "
                "Retain one (git tag/branch on the pre-rebase tip) and name it here."
                % (tag, retained))
            continue
        rc, _, _ = git(repo, "merge-base", "--is-ancestor", old_sha, retained, check=False)
        if rc != 0:
            problems.append("%s: the original is NOT contained in retained_ref %r, so that ref is "
                            "not evidence of it." % (tag, retained))
            continue
        pid_new, pid_old = _patch_id(repo, new_sha), _patch_id(repo, old_sha)
        if not pid_new or not pid_old:
            problems.append("%s: one side has no patch id (an empty or unreadable diff), so patch "
                            "equivalence cannot be established." % tag)
            continue
        if pid_new != pid_old:
            problems.append(
                "%s: the replayed patch is NOT EQUIVALENT to the original (patch-id %s vs %s). A "
                "commit whose diff changed during the replay - a conflict resolved by hand, or a "
                "context shift - is a different change from the one that was measured. Register it "
                "in %s on its own evidence." % (tag, pid_new[:12], pid_old[:12], PROTECTED_WINS))
            continue
        if pid_new != str(r["patch_id"]):
            problems.append("%s: the recorded patch_id %s is not the patch id these commits have "
                            "now (%s). The ledger is stale or was hand-edited."
                            % (tag, str(r["patch_id"])[:12], pid_new[:12]))
            continue
        tr_new, tr_old = commit_trailers(repo, new_sha), commit_trailers(repo, old_sha)
        if tr_new != tr_old:
            problems.append("%s: the trailers differ between the original and the replay, so the "
                            "provenance and the measured effect are not the same statement." % tag)
            continue
        if _canon_json(tr_new) != _canon_json(r["trailers"]):
            problems.append("%s: the recorded trailers are not the trailers these commits carry "
                            "now." % tag)
            continue
        sub_new = gout(repo, "log", "-1", "--format=%s", new_sha)
        sub_old = gout(repo, "log", "-1", "--format=%s", old_sha)
        if sub_new != sub_old or sub_new != str(r["subject"]):
            problems.append("%s: the subjects do not correspond to each other and to the recorded "
                            "one." % tag)
            continue
        if old_sha not in covered and old_sha not in baseline:
            problems.append(
                "%s: the ORIGINAL is itself unregistered - it is in no %s entry and no %s row. "
                "Replay provenance carries an existing registration across a rewrite; it can never "
                "create one. Register the win, then map it."
                % (tag, PROTECTED_WINS, PROTECTED_BASELINE))
            continue
        mapped[new_sha] = old_sha
    return problems, mapped


def cmd_protected_win_replay_map(repo: str, cfg: dict, args) -> int:
    """Derive the rebased -> original mapping for a replayed line, and optionally WRITE the ledger.

    Candidates for the original side are drawn ONLY from commits that are already registered in the
    manifest or dispositioned in the baseline, and only where they are contained in `--retained-ref`.
    So this command cannot register anything: it can only find the already-registered commit that a
    replayed commit IS, and it refuses to guess when it cannot tell."""
    # The same rule the strict read applies, applied BEFORE anything is written: a placeholder or
    # unsafe identity that reaches disk is a ledger row that every later read refuses anyway.
    why = recorded_by_problem(getattr(args, "recorded_by", "") or "")
    if why:
        raise Loud("--recorded-by is %s. It names the person making the registration statement; "
                   "pass the identity you sign commits with." % why)
    base = cfg["base"]["upstream_sha"]
    ref = args.ref
    retained = args.retained_ref
    if _resolve_ref(repo, retained) == "" or not _is_commit(repo, retained):
        raise Loud("--retained-ref %s does not resolve. Retain the PRE-REBASE tip as a real ref "
                   "before rewriting:\n    git tag pre-rebase-<date> <old tip>" % retained)
    step("Replay provenance: %s..%s against retained %s" % (base[:9], ref, retained))

    originals = []
    for sha in set(covered_win_commits(repo)) | set(load_discovery_baseline(repo)):
        if not _is_commit(repo, sha):
            continue
        rc, _, _ = git(repo, "merge-base", "--is-ancestor", sha, retained, check=False)
        if rc == 0:
            originals.append(sha)

    rows, unmatched, ambiguous = [], [], []
    for sha, subject, eff in unregistered_measured_wins(repo, cfg, ref):
        pid = _patch_id(repo, sha)
        tr = commit_trailers(repo, sha)
        hits = [o for o in originals
                if pid and _patch_id(repo, o) == pid
                and commit_trailers(repo, o) == tr
                and gout(repo, "log", "-1", "--format=%s", o) == subject]
        if len(hits) == 1:
            rows.append({"rebased": _resolve_ref(repo, sha), "original": _resolve_ref(repo, hits[0]),
                         "patch_id": pid, "subject": subject, "trailers": tr,
                         "retained_ref": retained, "recorded_utc": now_iso(),
                         "recorded_by": args.recorded_by})
            say("  MAPPED     %s <- %s  %s" % (sha[:9], hits[0][:9], subject[:60]))
        elif hits:
            ambiguous.append((sha, subject, hits))
            say("  AMBIGUOUS  %s  %s" % (sha[:9], subject[:60]))
        else:
            unmatched.append((sha, subject, eff))
            say("  UNMATCHED  %s  %s" % (sha[:9], subject[:60]))

    say("\nmapped: %d   ambiguous: %d   unmatched: %d" % (len(rows), len(ambiguous), len(unmatched)))
    for sha, subject, hits in ambiguous:
        say("\n  AMBIGUOUS %s %r matches %d registered originals: %s"
            % (sha[:9], subject[:60], len(hits), ", ".join(h[:9] for h in hits)))
        say("  A one-to-one map cannot be written. Register this commit directly in %s."
            % PROTECTED_WINS)
    for sha, subject, eff in unmatched:
        say("\n  UNMATCHED %s %r (Measured-effect: %s)"
            % (sha[:9], subject[:60], " ".join(eff.split())[:80]))
        say("  No registered original in %s has the same patch, trailers and subject. This is a new"
            % retained)
        say("  measured win, or a replay whose patch changed. Register it by hand in %s."
            % PROTECTED_WINS)

    if not getattr(args, "write", False):
        say("\nNothing written. Re-run with --write to create tools/arifi-sync/%s, then COMMIT it:"
            % REPLAY_PROVENANCE)
        say("  git add -- tools/arifi-sync/%s" % REPLAY_PROVENANCE)
        say("  git commit -- tools/arifi-sync/%s     # needs an Origin: trailer" % REPLAY_PROVENANCE)
        return 1 if (ambiguous or unmatched) else 0

    dest = os.path.join(repo, "tools", "arifi-sync", REPLAY_PROVENANCE)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with open(dest, "w", encoding="utf-8", newline="\n") as fh:
        json.dump({"version": 1, "mappings": rows}, fh, indent=2, sort_keys=True)
        fh.write("\n")
    say("\nwrote %s (%d mapping(s)). It is NOT committed - commit it by pathspec:" % (dest, len(rows)))
    say("  git add -- tools/arifi-sync/%s" % REPLAY_PROVENANCE)
    say("  git commit -- tools/arifi-sync/%s" % REPLAY_PROVENANCE)
    if ambiguous or unmatched:
        say("\nFAIL: %d commit(s) could not be mapped one-to-one. They are named above and must be "
            "registered by hand." % (len(ambiguous) + len(unmatched)))
        return 1
    return 0


def covered_win_commits(repo: str) -> set:
    covered = set()
    for w in _load_json(repo, PROTECTED_WINS, {"wins": []}).get("wins", []):
        for c in [w.get("commit")] + list(w.get("also_commits") or []):
            if c:
                full = _resolve_ref(repo, c)
                covered.add(full or c)
    return covered


def unregistered_measured_wins(repo: str, cfg: dict, ref: str) -> list:
    """The STRICT set: landed commits with a real `Measured-effect:` that no manifest entry covers
    AND no baseline row dispositions. This is the single predicate `discover` prints and
    `validate` fails on, so the report and the gate can never disagree about the set - the same
    discipline `has_loose_provenance` already gives `cmd_provenance` and `regen_preflight`."""
    base = cfg["base"]["upstream_sha"]
    covered = covered_win_commits(repo)
    baseline = load_discovery_baseline(repo)
    # A REPLAYED registration counts, and only on the terms `replay_provenance` re-derives live:
    # a retained original that is itself registered, an equivalent patch, corresponding trailers
    # and a one-to-one map. Rows that fail those checks map nothing and their commits stay in the
    # set below, so a broken ledger cannot quiet the gate.
    _, replayed = replay_provenance(repo, cfg)
    rows = []
    for sha in gout(repo, "rev-list", "--reverse", "%s..%s" % (base, ref)).split():
        if sha in covered or sha in baseline or sha in replayed:
            continue
        eff = commit_trailers(repo, sha)["Measured-effect"]
        if _measured_effect_is_real(eff):
            rows.append((sha, gout(repo, "log", "-1", "--format=%s", sha), eff))
    return rows


def cmd_protected_win_discover(repo: str, cfg: dict, args) -> int:
    """Every commit in the series carrying a real Measured-effect that no entry covers.

    This is the anti-silence half of assignment 2. The manifest is authored, so the failure mode
    it cannot see by itself is a win that was measured, landed, and then never registered.

    Two modes, deliberately kept apart:
      * default - the STRICT set (unregistered AND un-baselined). This is exactly what
        `validate`, and therefore `check` and `bump`, now refuse on, so what this prints is what
        will block an update. `--strict` additionally makes THIS command exit non-zero, for use
        as a standalone job.
      * `--all` - the INFORMATIONAL candidate set, which also lists every baselined row with its
        recorded disposition. Nothing is hidden by the baseline; it is a ledger, not a mute."""
    base = cfg["base"]["upstream_sha"]
    ref = args.ref
    step("Scanning %s..%s for measured wins with no manifest entry" % (base[:9], ref))
    rows = unregistered_measured_wins(repo, cfg, ref)
    baseline = load_discovery_baseline(repo)
    say("UNREGISTERED and un-baselined (this set REFUSES validate/check/bump): %d" % len(rows))
    for sha, subject, eff in rows:
        say("  %s  %s" % (sha[:9], subject))
        say("             %s" % " ".join(eff.split())[:150])
    if getattr(args, "all", False):
        say("\nbaselined rows (recorded disposition, %s): %d" % (PROTECTED_BASELINE, len(baseline)))
        for sha, row in baseline.items():
            say("  %s  %s" % (sha[:9], row.get("subject", "")[:80]))
            say("             %s" % row.get("disposition", "")[:150])
    if rows and getattr(args, "strict", False):
        say("\nFAIL (--strict): the set above is measured, landed and unprotected.")
        return 1
    say("\nNOTE: a row belongs in the manifest only when its protected paths, anchors, evidence,")
    say("quality gate and workload are all known. Never invent a field to close a row - record a")
    say("written disposition in %s instead, which is visible and dated." % PROTECTED_BASELINE)
    return 0


def load_resolutions(repo: str) -> list:
    data = _load_json(repo, PROTECTED_RESOLUTIONS, {"resolutions": []})
    return data.get("resolutions", [])


def _resolution_problems(r: dict) -> list:
    """A resolution is what buys the right to change a protected mechanism. It is checked hard,
    because a resolution nobody validated is the silent overwrite wearing a receipt."""
    bad = [f for f in RESOLUTION_REQUIRED if not r.get(f)]
    out = ["missing/empty %s" % ", ".join(bad)] if bad else []
    if r.get("verdict") and r["verdict"] not in RESOLUTION_VERDICTS:
        out.append("verdict must be one of %s" % (RESOLUTION_VERDICTS,))
    arms = r.get("arms") or {}
    # Three arms, President 2026-09-02 (UPDATE-RUNBOOK 4.1b duty 3): upstream alone, ours alone,
    # and the UNION. An arm may be declared inapplicable, but only in writing and by name -
    # "not run" is not a value.
    for arm in ("upstream", "ours", "union"):
        if not arms.get(arm):
            out.append("arms.%s is missing - name its result, or state in that field why the arm "
                       "is inapplicable" % arm)
    return out


def protected_collisions(repo: str, wins: list, changed: set, diff_text: str) -> list:
    """A collision is a protected PATH the incoming change touches, or a protected ANCHOR whose
    line the incoming diff removes. The anchor half matters: upstream can delete our dispatch
    branch while leaving the file name intact, and a path-only check would pass that.

    A protected path may name a DIRECTORY, and that case is matched on the directory boundary
    (R6 finding 1). `git cat-file -e <ref>:<dir>` succeeds on a tree, so `validate` called such an
    entry healthy, while this function's set-intersection could never hit it - `git diff
    --name-only` emits files only. A whole vendored subtree could therefore be registered and
    guarded by nothing at all. Matching on `<dir>/` rather than the raw string keeps a sibling
    like `kernel.c.bak` out of `kernel.c`'s guard.

    The cost is deliberate: any touch anywhere under a protected subtree now REFUSES until a
    comparison resolution is recorded. Under a fail-closed mandate that is the designed price of
    protecting a whole tree, and it is why a subtree is registered only where the tree is ours."""
    removed = set()
    for line in diff_text.splitlines():
        if line.startswith("-") and not line.startswith("---"):
            removed.add(line)
    out = []
    for w in wins:
        if w.get("status") != "active":
            continue
        paths = sorted(p for p in set(w["protected_paths"])
                       if p in changed
                       or any(c.startswith(p.rstrip("/") + "/") for c in changed))
        anchors = sorted(a for a in w["anchors"] if any(a in l for l in removed))
        if paths or anchors:
            out.append({"win": w, "paths": paths, "anchors": anchors})
    return out


def cmd_protected_win_check(repo: str, cfg: dict, args) -> int:
    """FAIL-CLOSED ingest/update preflight (mandate assignment 3).

    Refuses when an incoming upstream/fork change touches a protected path, or deletes a line
    carrying a protected anchor, unless a recorded comparison resolution covers that exact
    (win, incoming) pair. The resolution must name three arms, a quality result and resolvable
    evidence - i.e. upstream superiority has to be MEASURED before it may replace ours."""
    ref = args.ref
    incoming = args.incoming
    step("Protected-win preflight: %s vs incoming %s" % (ref, incoming))

    problems = validate_protected_wins(repo, ref, cfg)
    if problems:
        say("FAIL: the manifest itself does not validate, so the preflight cannot run.")
        for p in problems:
            say("  %s" % p)
        say("\nThis is fail-closed by design: an incomplete manifest cannot be used to prove that")
        say("nothing is at risk. Fix the manifest, then re-run.")
        return 1
    wins = load_protected_wins(repo)

    if _resolve_ref(repo, incoming) == "":
        raise Loud("cannot resolve --incoming %s" % incoming)
    mb = gout(repo, "merge-base", ref, incoming)
    if not mb:
        raise Loud("no merge base between %s and %s" % (ref, incoming))
    rng = "%s..%s" % (mb, incoming)
    # `--no-renames` on BOTH calls, and it is load-bearing on both (D1, CHECKER-R3 2026-09-04).
    # With rename detection on, a 100%-similarity rename of a protected file reports ONLY the
    # destination path (`--name-only` prints `kernel_renamed.c` alone) and emits NO content lines
    # (`similarity index 100% / rename from / rename to`), so `protected_collisions` sees neither
    # the protected path nor an anchor removal and the fail-closed preflight PASSES a change that
    # moved the win out from under the manifest. Suppressing rename detection restores both halves
    # at once: the rename becomes delete+add, the old path returns to `changed`, and every line of
    # the old file returns as a `-` line so the anchors fire too.
    changed = set(p for p in gout(repo, "diff", "--no-renames", "--name-only", rng).splitlines() if p)
    _, diff_text, _ = git(repo, "diff", "--no-renames", "--unified=0", rng)
    say("incoming range   : %s (%d file(s))" % (rng[:24] + "...", len(changed)))

    collisions = protected_collisions(repo, wins, changed, diff_text)
    if not collisions:
        say("\nPASS: the incoming change touches no protected path and removes no protected anchor.")
        say("That is a CHECKED claim over %d active entr(ies), not an assumption." %
            len([w for w in wins if w.get("status") == "active"]))
        return 0

    resolutions = load_resolutions(repo)
    inc_full = _resolve_ref(repo, incoming)
    unresolved, accepted = [], []
    for c in collisions:
        wid = c["win"]["id"]
        match = None
        for r in resolutions:
            if r.get("win_id") != wid:
                continue
            if _resolve_ref(repo, str(r.get("incoming", ""))) != inc_full:
                continue
            match = r
            break
        if match is None:
            unresolved.append((c, ["no resolution recorded for (%s, %s @ %s)"
                                   % (wid, incoming, inc_full[:9])]))
            continue
        bad = _resolution_problems(match)
        bad += ["evidence locator %r does not resolve" % e
                for e in (match.get("evidence") or []) if not _evidence_resolves(repo, e)]
        if bad:
            unresolved.append((c, bad))
        else:
            accepted.append((c, match))

    for c, r in accepted:
        say("  RESOLVED  %-28s verdict=%s  (%s)" % (c["win"]["id"], r["verdict"], r["decided_by"]))
    if not unresolved:
        say("\nPASS: every collision carries an evidence-backed comparison verdict.")
        return 0

    say("\n" + "=" * 78)
    say("REFUSED: %d protected win(s) collide with the incoming change and are NOT resolved."
        % len(unresolved))
    say("=" * 78)
    for c, why in unresolved:
        w = c["win"]
        say("\n  win        : %s" % w["id"])
        say("  mechanism  : %s" % w["mechanism"])
        say("  measured   : %s" % " ".join(w["measured_effect"].split())[:140])
        if c["paths"]:
            say("  paths hit  : %s" % ", ".join(c["paths"]))
        if c["anchors"]:
            say("  anchors cut: %s" % ", ".join(c["anchors"]))
        for b in why:
            say("  BLOCKER    : %s" % b)
    say("\nWhat unblocks this is measurement, never source priority. Run the weakest-successor")
    say("procedure in UPDATE-RUNBOOK 4.1c: preserve the local candidate, build upstream / ours /")
    say("union, run the registered workload and the quality gate on each arm, retain the winner,")
    say("archive every arm's result, and record the verdict in")
    say("  tools/arifi-sync/%s" % PROTECTED_RESOLUTIONS)
    say("A superseded local win stays in the manifest with the conditions it still wins on.")
    return 1


def _exact_tag(repo: str, sha: str) -> str:
    rc, out, _ = git(repo, "describe", "--tags", "--exact-match", sha, check=False)
    return out.strip() if rc == 0 else ""


def cmd_set_base_pin(repo: str, cfg: dict, args) -> int:
    """Move base.upstream_sha AND base.upstream_tag in sources.json, as ONE checked operation.

    This command exists because step 3 of the printed bump repair used to be the line
    `edit tools/arifi-sync/sources.json: ...`, which is prose in a block that calls itself a
    command sequence (CHECK-UPDATE-ROOT-R3 finding 5). An operator cannot run prose and a test
    cannot execute it, so the test reached that state through a private helper of its own and the
    printed sequence was never the sequence under test. It is a real subcommand now.

    The pin is a PAIR (sources.json's own _comment, lane-139): moving the sha and leaving the tag
    behind produces a base nobody can name. So both move here, in one write, and the tag is
    DERIVED from the ref by `git describe --tags --exact-match` rather than typed - the same
    verification the base-move notes in sources.json already record by hand.

    Every check runs before a single byte is written, and the write is a temp file plus
    os.replace, so a refusal leaves the file exactly as it was and an interrupted write cannot
    leave a half-written config behind.
    """
    path = os.path.abspath(args.config)
    with open(path, encoding="utf-8", newline="") as fh:
        raw = fh.read()
    # The rewrite keeps the file's OWN line endings. This tree is checked out CRLF; a write that
    # normalised to LF would turn a two-value edit into a whole-file diff on the day of an update.
    eol = "\r\n" if "\r\n" in raw else "\n"
    try:
        doc = json.loads(raw)
    except ValueError as e:
        raise Loud("%s is not parseable JSON (%s). Repair it by hand; this command refuses to\n"
                   "overwrite a file it could not read first." % (path, e))
    base = doc.get("base")
    if not isinstance(base, dict):
        raise Loud("%s has no `base` object, so there is no pin to move. This is not the config\n"
                   "this command is for." % path)
    for k in ("upstream_sha", "upstream_tag"):
        if not isinstance(base.get(k), str) or not base[k].strip():
            raise Loud("base.%s is missing or not a string in %s. The pin is a (sha, tag) PAIR;\n"
                       "half of one is not a state this command will move from." % (k, path))

    old, old_tag = base["upstream_sha"].strip(), base["upstream_tag"].strip()
    expect = (args.expect_sha or "").strip()
    if len(expect) < 7 or not re.fullmatch(r"[0-9a-fA-F]+", expect):
        raise Loud("--expect-sha %r is not a sha prefix of at least 7 hex characters." % expect)
    if not old.lower().startswith(expect.lower()):
        raise Loud("REFUSED: the pin on disk is not the one you expected.\n"
                   "  on disk   : base.upstream_sha = %s\n"
                   "  --expect-sha: %s\n"
                   "Somebody else moved this pin, or you are in a different checkout than you\n"
                   "think. Re-read %s before moving it." % (old, expect, path))

    new = _resolve_ref(repo, args.onto)
    if new == "":
        raise Loud("cannot resolve %s in %s - fetch first: arifi_sync.py currency"
                   % (args.onto, repo))
    if new == old:
        raise Loud("REFUSED: base.upstream_sha is already %s, so this would move nothing. A pin\n"
                   "move that changes nothing is a sign the ref is not the one you meant." % new)

    tag = (args.upstream_tag or "").strip() or _exact_tag(repo, new)
    if not tag:
        raise Loud("REFUSED: %s (%s) carries no exact tag, so the pin's tag cannot be DERIVED.\n"
                   "The pin is a (sha, tag) pair and the tag is the name a stranger reads. Either\n"
                   "bump onto a tagged commit, or name the tag yourself:\n"
                   "  --upstream-tag <tag>" % (args.onto, new[:9]))
    if args.upstream_tag:
        named = _resolve_ref(repo, tag)
        if named != new:
            raise Loud("REFUSED: --upstream-tag %s resolves to %s, not to %s (%s). The tag must\n"
                       "name the commit being pinned." % (tag, named[:9] or "nothing", new[:9],
                                                          args.onto))

    base["upstream_sha"], base["upstream_tag"] = new, tag
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline=eol) as fh:
        fh.write(json.dumps(doc, indent=2, ensure_ascii=False) + "\n")
    os.replace(tmp, path)
    step("base pin MOVED in %s" % path)
    say("  upstream_sha : %s -> %s" % (old[:9], new[:9]))
    say("  upstream_tag : %s -> %s" % (old_tag, tag))
    say("Nothing was staged and nothing was committed - that is yours, by pathspec:")
    say("  git -C %s add -- tools/arifi-sync/sources.json" % repo)
    say("  git -C %s commit -- tools/arifi-sync/sources.json" % repo)
    return 0


BUMP_STEP_PREFIX = "    "

# No `<`, `>`, `"`, `'`, `\`, `$` or `%`: a value carrying any of those is metasyntax in some shell
# the printed block may be pasted into, or is eaten by `shlex.split(posix=True)`. A space is allowed
# because `user.name` is usually two words; `_shell_word` quotes it.
_RECORDED_BY_SAFE = re.compile(r"^[A-Za-z0-9._@+ -]+$")

# CHECK-UPDATE-ROOT-R6 finding 2: `unknown-operator` used to be printed as the identity when nothing
# resolved. It is shell-safe and nonempty, so strict provenance accepted it - and a row recording it
# is a formally valid registration statement made by NOBODY. These values name nobody by
# construction, so they are refused on the way in AND on every read.
# UPDATE-RUNBOOK.md's pasted bash ladder MIRRORS this pattern (and `_RECORDED_BY_SAFE`) as a
# `grep -Ei`, so a placeholder never even reaches argv; `test_the_runbook_identity_rule_IS_the_python_one`
# pins both pattern strings and the grep flags, so editing either one alone is a RED test. Keep this
# pattern POSIX-ERE-compatible and keep case-insensitivity in the flag, not in an inline `(?i)`.
_RECORDED_BY_PLACEHOLDER = re.compile(
    r"^(unknown([ ._-]?operator)?|operator|someone|somebody|anonymous|nobody|none|n/?a|tbd|you|"
    r"your[ ._-]?(name|address)|changeme|[^@]*@example\.(com|org|net|invalid))$", re.IGNORECASE)


def _shell_word(value: str) -> str:
    """One shell word for `value`, quoted only when it has to be.

    DOUBLE quotes, never `shlex.quote`'s single quotes: single quotes do not group in cmd.exe, and
    the printed block is copy-pasted on this (Windows) box as often as in bash. Double quotes group
    in bash, PowerShell and cmd, and `shlex.split(posix=True)` - which is how the convergence test
    executes the printed line - strips them.
    """
    return value if re.match(r"^[A-Za-z0-9._@+-]+$", value) else '"%s"' % value


def recorded_by_identity(repo: str) -> str:
    """The operator identity for the printed `replay-map --recorded-by`, RESOLVED at print time.

    CHECK-UPDATE-ROOT-R4 finding 3: the line used to print `--recorded-by <you>`, which a shell
    reads as input redirection - so the one line of the sequence that was not runnable as printed
    was in a block whose banner said every line was. A shell EXPANSION (`$(git config user.email)`)
    would not fix it either: the convergence test runs the printed line through `shlex.split` and
    `main`, with no shell, so the expansion would be recorded verbatim as the identity. A concrete
    value resolved here is the only form that is both executable as printed and executed as printed.

    `git config user.email`, then `user.name`, then the OS user - each candidate validated before
    the next is considered, so an unsafe email falls through to a safe name instead of winning by
    being merely nonempty. Returns "" when NOTHING resolves: there is no constant to fall back on
    (CHECK-UPDATE-ROOT-R6 finding 2), because a registration statement is about a person and no
    default names one. `--recorded-by` stays REQUIRED in the parser; this only supplies the operator
    the checkout already names.
    """
    candidates = []
    for key in ("user.email", "user.name"):
        rc, out, _ = git(repo, "config", "--get", key, check=False)
        candidates.append(out.strip() if rc == 0 else "")
    try:
        candidates.append((getpass.getuser() or "").strip())
    except Exception:
        candidates.append("")
    for value in candidates:
        if not recorded_by_problem(value):
            return value
    return ""


def recorded_by_problem(value: str) -> str:
    """Why `value` is not an operator identity, or "" when it is one.

    ONE rule, used by the resolver, by `replay-map --write` and by every strict provenance read, so
    a value the printer would not print is also a value the ledger will not keep."""
    v = (value or "").strip()
    if not v:
        return "empty"
    if not _RECORDED_BY_SAFE.match(v):
        return ("not shell-safe (%r carries characters that are metasyntax in a shell, or that "
                "`shlex.split` eats)" % v)
    if _RECORDED_BY_PLACEHOLDER.match(v):
        return "a placeholder: %r names nobody, so it cannot make a registration statement" % v
    return ""


RETAINED_REF_PREFIX = "arifi-pre-rebase"


def _commit_sha(repo: str, ref: str) -> str:
    """The exact commit `ref` names right now, or "" if it names no commit."""
    rc, out, _ = git(repo, "rev-parse", "--verify", "--quiet", "%s^{commit}" % ref, check=False)
    return out.strip() if rc == 0 else ""


def retained_ref_name(tip: str) -> str:
    """The retained ref NAMES the object it retains.

    CHECK-UPDATE-ROOT-R9 finding 1: with one fixed name, a stale `arifi-pre-rebase` left by an
    EARLIER cycle satisfies a gate that only asks "does the name resolve?". `git tag` then fails
    because the name is taken, `rev-parse` succeeds on the stale object, and the rebase rewrites
    this cycle's tip with nothing retaining it - the one failure that cannot be repaired afterwards.

    Deriving the name from the tip closes that without deleting anything: another cycle's retained
    ref is a DIFFERENT name, so it can neither satisfy this gate nor be overwritten to make room.
    Deleting a stale fixed-name tag would have been the only other repair, and it breaks the
    reachability check on every prior cycle's ledger rows, which name that ref.
    """
    return "%s-%s" % (RETAINED_REF_PREFIX, tip)


def retention_gate_lines(repo: str, ref: str, tip: str) -> list:
    """The retention gate: capture, create-if-absent, verify-exact, all before any rewrite.

    ONE source for the sequence `bump` prints and for the block UPDATE-RUNBOOK.md publishes -
    `test_the_runbook_retention_block_IS_the_generated_one` asserts the runbook's block is these
    lines with the operator placeholders in it, so the two cannot drift (CHECK-UPDATE-ROOT-R9
    finding 2: the runbook grew a fence the printed repair never had).

    Every line propagates its own failure with `|| exit $?` or an explicit `exit 1`, so none of
    the stops needs the caller to have run `set -e`. `git tag` is allowed to fail ONLY when the
    name already exists, and the line after it is what makes that safe: the ref must resolve to
    the tip captured BEFORE the switch, or the sequence stops with nothing rewritten and nothing
    overwritten.
    """
    name = retained_ref_name(tip)
    return [
        "git -C %s switch %s || exit $?" % (repo, ref),
        'test "$(git -C %s rev-parse --verify --quiet "%s^{commit}")" = %s || '
        '{ echo "REFUSE: %s is no longer %s - re-run the bump for the tip you have" >&2; exit 1; }'
        % (repo, ref, tip, ref, tip),
        'git -C %s rev-parse --verify --quiet "%s^{commit}" >/dev/null || '
        "git -C %s tag %s %s || exit $?" % (repo, name, repo, name, tip),
        'test "$(git -C %s rev-parse --verify --quiet "%s^{commit}")" = %s || '
        '{ echo "REFUSE: %s exists at another object - nothing rewritten, nothing overwritten" '
        '>&2; exit 1; }' % (repo, name, tip, name),
    ]


def bump_repair_sequence(repo: str, cfg: dict, args, base: str) -> list:
    """The literal, complete command sequence that materialises the candidate.

    ONE source for the printed repair and for the convergence test, because CHECK-UPDATE-ROOT-R2
    finding 6 was precisely a divergence between them: the printed sequence omitted
    `git add -- patches/series`, the test's own helper performed that step privately, and the test
    therefore proved the convergence of a workflow no operator was ever shown. The test now reads
    these lines back out of the refusal it captured and runs them; a step that is not printed here
    is a step that does not run there.

    Every INDENTED line is a command; the `#` lines are comments and the block is copy-pasteable
    as a whole. R3 found the one line that was neither - `edit tools/arifi-sync/sources.json: ...`,
    prose that no operator can run and no test can execute - and it is `set-base-pin` now.

    Why `add` is needed for the series and not for sources.json: `series regen` writes NEW files
    into the generated directory, and `git commit -- <pathspec>` only commits paths git already
    tracks. sources.json is tracked and merely modified, so committing it by pathspec is enough.
    """
    series = cfg["series_dir"]
    purity = (" --i-have-read-bench-purity"
              if getattr(args, "i_have_read_bench_purity", False) else "")
    L = []
    p = BUMP_STEP_PREFIX
    L.append("  # 0. RETAIN the pre-rebase tip as a real ref BEFORE rewriting anything. The rebase")
    L.append("  #    gives every replayed commit a new sha, and a registered win's manifest entry")
    L.append("  #    names the OLD one. `replay-map` can only carry that registration across the")
    L.append("  #    rewrite while the originals are still reachable from a ref somebody can name.")
    tip = _commit_sha(repo, args.ref)
    if not tip:
        # The retained ref is named after the object it retains, so an unresolvable ref would emit
        # a tag name with nothing on the end of it and a gate comparing against the empty string:
        # executable-looking lines that retain nothing. Same shape as the no-identity branch below,
        # and the same stop - the one step that IS runnable now.
        L.append("  #    STOP HERE. %s names no commit in this checkout, so there is no tip to"
                 % args.ref)
        L.append("  #    retain and nothing below may run. Fetch, or name the ref you actually")
        L.append("  #    have, then re-run the bump: the sequence prints resolved against a real")
        L.append("  #    tip, or it does not print the rewrite at all.")
        L.append(p + "python tools/arifi-sync/arifi_sync.py bump --onto %s --ref %s%s || exit $?"
                 % (args.onto, args.ref, purity))
        return L
    retained = retained_ref_name(tip)
    L.append("  #    The ref is named after the tip it retains (%s), so a stale retained ref from" % tip[:9])
    L.append("  #    an EARLIER cycle is a different name: it cannot satisfy this gate, and it is")
    L.append("  #    never overwritten to make room. The line after `tag` is the fence - the ref")
    L.append("  #    MUST resolve to exactly that tip, or nothing is rewritten.")
    for line in retention_gate_lines(repo, args.ref, tip):
        L.append(p + line)
    L.append("  # 1. rebase our line onto the new upstream.")
    L.append(p + "git -C %s rebase --onto %s %s %s || exit $?"
             % (repo, args.onto, base, args.ref))
    L.append("  # 2. map the replayed commits back to their registered originals, and COMMIT the")
    L.append("  #    ledger. Refuses on anything it cannot map one-to-one; those must be")
    L.append("  #    registered by hand in %s." % PROTECTED_WINS)
    recorded_by = recorded_by_identity(repo)
    if not recorded_by:
        # CHECK-UPDATE-ROOT-R6 finding 2. There used to be a constant here so that a line always
        # printed; the line was then executable, concrete-looking and made a registration statement
        # on behalf of nobody. `--recorded-by` names the PERSON making that statement and cannot be
        # derived, defaulted or invented - so the sequence stops at the one step that IS runnable
        # as printed, and resumes, fully resolved, on the next bump.
        L.append("  #    STOP HERE. This checkout names no shell-safe operator identity, and")
        L.append("  #    `--recorded-by` is a statement about a PERSON: it has no default. Set the")
        L.append("  #    identity you sign commits with - `git config user.email` (or user.name) -")
        L.append("  #    with your REAL address, then re-run the bump below: the complete sequence")
        L.append("  #    then prints with the identity resolved into it, and a placeholder value")
        L.append("  #    would be refused by replay-map and by every provenance read anyway.")
        L.append(p + "python tools/arifi-sync/arifi_sync.py bump --onto %s --ref %s%s || exit $?"
                 % (args.onto, args.ref, purity))
        return L
    L.append("  #    `--recorded-by` is filled in below from THIS checkout's `git config"
             " user.email`")
    L.append("  #    (then user.name, then the OS user); change it if you are recording for"
             " somebody else.")
    L.append(p + "python tools/arifi-sync/arifi_sync.py protected-win replay-map --ref %s "
             "--retained-ref %s --recorded-by %s --write || exit $?"
             % (args.ref, retained, _shell_word(recorded_by)))
    L.append(p + "git -C %s add -- tools/arifi-sync/%s || exit $?" % (repo, REPLAY_PROVENANCE))
    L.append(p + "git -C %s commit -- tools/arifi-sync/%s || exit $?   # needs an Origin: trailer"
             % (repo, REPLAY_PROVENANCE))
    L.append("  # 3. move the base pin BEFORE regenerating. `series regen` generates base..%s, so"
             % args.ref)
    L.append("  #    regenerating against the OLD pin re-emits the whole upstream delta as patches.")
    L.append("  #    The pin is a (sha, tag) PAIR, so `set-base-pin` moves both in one write; it")
    L.append("  #    derives the tag with `git describe --tags --exact-match` and refuses if the")
    L.append("  #    pin on disk is not the one named by --expect-sha.")
    L.append(p + "python tools/arifi-sync/arifi_sync.py set-base-pin --onto %s --expect-sha %s"
             " || exit $?"
             % (args.onto, base))
    L.append(p + "git -C %s add -- tools/arifi-sync/sources.json || exit $?" % repo)
    L.append(p + "git -C %s commit -- tools/arifi-sync/sources.json || exit $?"
             "   # needs an Origin: trailer"
             % repo)
    L.append("  # 4. regenerate the series against the NEW pin, STAGE it, and COMMIT it. The `add`")
    L.append("  #    is not optional: regen writes NEW patch files, and `git commit -- <path>`")
    L.append("  #    commits only what git already tracks, so without it the new patch stays")
    L.append("  #    untracked - it passes the byte compare and is then absent from a fresh clone.")
    L.append(p + "python tools/arifi-sync/arifi_sync.py series regen --ref %s || exit $?"
             % args.ref)
    L.append(p + "git -C %s add -- %s || exit $?" % (repo, series))
    L.append(p + "git -C %s commit -- %s || exit $?   # needs an Origin: trailer"
             % (repo, series))
    L.append("  # 5. re-run the bump. Every gate is re-evaluated; none is skipped on the second try.")
    L.append(p + "python tools/arifi-sync/arifi_sync.py bump --onto %s --ref %s%s || exit $?"
             % (args.onto, args.ref, purity))
    return L


def cmd_bump(repo: str, cfg: dict, args) -> int:
    step("BUMP to %s - replay, then build, then judge. All three must pass." % args.onto)
    if _resolve_ref(repo, args.onto) == "":
        raise Loud("cannot resolve %s - fetch first: arifi_sync.py currency" % args.onto)

    step("1/4  rebase surface: what upstream touched that our patches also touch")
    base = cfg["base"]["upstream_sha"]
    # lane-152: this read `args.ref if hasattr(args, "ref") else "master"`, and `bump`'s parser had no
    # --ref, so it ALWAYS took the hardcoded fallback. sources.json's own _comment says the three
    # argparse "master" defaults were fixed at lane-139 - this fourth one was missed, and `master` is
    # still a live branch here (9aa7b3d2b, the pre-recut history), so it printed a plausible rebase
    # surface computed against a dead branch instead of failing. Silent wrong numbers, not an error.
    _, ours, _ = git(repo, "diff", "--name-only", base, args.ref)
    our_paths = set(p for p in ours.split("\n") if p)
    _, theirs, _ = git(repo, "diff", "--name-only", base, args.onto)
    their_paths = set(p for p in theirs.split("\n") if p)
    overlap = sorted(our_paths & their_paths)
    say("our patches touch     : %d files" % len(our_paths))
    say("upstream delta touches: %d files" % len(their_paths))
    say("REBASE SURFACE        : %d files (computed, not guessed)" % len(overlap))
    for p in overlap[:80]:
        say("    %s" % p)
    if len(overlap) > 80:
        say("    ... and %d more" % (len(overlap) - 80))

    # President mandate 2026-09-04, assignment 3. The collision set above is exactly the surface
    # the mandate is about, so the protected-win preflight runs HERE, on it, before a single
    # further leg. It is fail-closed: an unresolved collision refuses the bump.
    step("1b/4 protected-win preflight over the rebase surface")
    pw = argparse.Namespace(ref=args.ref, incoming=args.onto)
    if cmd_protected_win_check(repo, cfg, pw) != 0:
        say("\nBUMP REFUSED: an incoming change collides with a protected win and no measured")
        say("comparison authorises it. This is the President's law, not a style preference:")
        say("upstream superiority is an empirical replacement proposal, never authority to")
        say("overwrite. Run UPDATE-RUNBOOK 4.1c and record the verdict, then re-run bump.")
        return 1

    step("2/4  replay the series onto %s" % args.onto)
    replayed = {}
    if _replay(repo, cfg, args.onto, os.path.join(repo, cfg["series_dir"]),
               record=replayed, keep_branch=True) != 0:
        say("\nBUMP REFUSED: replay failed.")
        return 1
    say("candidate RETAINED on branch %s -> %s (tree %s). The replay worktree is gone; the"
        % (replayed.get("branch") or REPLAY_BRANCH, str(replayed.get("tip"))[:9],
           str(replayed.get("tree"))[:9]))
    say("candidate itself is not, so it can be diffed, inspected and checked out.")

    # THE OLD-TREE TRAP (UPDATE-RUNBOOK 2.1, lane-152; closed lane-229, corrected here). Legs 3
    # and 4 act on `repo`'s own build dir - the checkout you are standing in, which is still on the
    # OLD base. A green bump used to mean "the series replays AND my old checkout still builds".
    #
    # The first correction compared RAW TREE SHAS, which no real candidate can ever satisfy
    # (CHECK-UPDATE-ROOT finding 4): the generator excludes `series_dir` from itself, so the
    # replayed tree carries no series at all while the checkout carries the committed one. A
    # correctly materialised candidate was refused forever, and the printed repair could not
    # reach a passing state. The identity is NORMALISED instead - equal everywhere outside the
    # generated directory - and the excluded directory is proved separately, by
    # `series_integrity_problems`, to be exactly what git generates and to be committed. Together
    # those two are the whole tree, with no gate dropped.
    step("2b/4 candidate identity: this checkout must BE the replayed candidate")
    here = _tree_identity(repo, os.path.join(repo, cfg["build"]["build_dir"]))
    problems, outside = [], ""
    if here["dirty"]:
        problems.append("the checkout has uncommitted edits, so it has no identity a receipt can "
                        "name. Commit them (they are part of the candidate) or remove them.")
    else:
        _, outside, _ = git(repo, "diff", "--name-only", str(replayed.get("tip")), "HEAD",
                            "--", ".", ":(exclude)%s" % series_pathspec(cfg), check=False)
        if outside.strip():
            problems.append("the checkout differs from the replayed candidate OUTSIDE %s, so legs "
                            "3-4 would build a tree the replay never produced."
                            % cfg["series_dir"])
        problems.extend(series_integrity_problems(repo, cfg, "HEAD"))
    if problems:
        say("\n" + "=" * 78)
        say("BUMP REFUSED: the checkout is not the candidate, so legs 3-4 would build and judge")
        say("the wrong tree. Nothing was built and no verdict was produced.")
        say("=" * 78)
        say("  replayed candidate : %s (tree %s) onto %s, kept on %s"
            % (str(replayed.get("tip"))[:9], str(replayed.get("tree"))[:9], args.onto,
               replayed.get("branch") or REPLAY_BRANCH))
        say("  this checkout      : %s (tree %s)%s"
            % (here["head"][:9], here["tree"][:9], "  UNCOMMITTED EDITS" if here["dirty"] else ""))
        for p in problems:
            say("  BLOCKER            : %s" % p)
        for f in [l for l in outside.splitlines() if l.strip()][:20]:
            say("      differs outside the series: %s" % f)
        say("\nLeg 2 PASSED - the series replays cleanly onto %s. To finish the bump, MATERIALISE"
            % args.onto)
        say("the candidate in this checkout and re-run. Order matters, and every step is yours to")
        say("run - this tool never moves your branches and never fetches or pulls for you.")
        say("")
        say("THIS IS THE COMPLETE SEQUENCE, in this order; nothing is elided and no step is folded")
        say("into another. Every INDENTED line is a command to run as printed, with no placeholder")
        say("left to fill in - the `#` lines are comments.")
        say("`test_the_documented_repair_sequence_CONVERGES_to_a_green_bump` asserts EVERY one of")
        say("these lines verbatim and drives this sequence, so a step missing here is a RED test and")
        say("not a surprise on the day of an update. What it also EXECUTES as printed is the two")
        say("`arifi_sync.py replay-map` / `set-base-pin` lines: their printed argv is split and run")
        say("through the public CLI. The commit lines carry no `-m`, because they expect your editor")
        say("and an Origin: trailer, so the test asserts them and then supplies those messages")
        say("non-interactively; `series regen` is likewise asserted and then run via the fixture.")
        for line in bump_repair_sequence(repo, cfg, args, base):
            say(line)
        say("\nOr drive legs 3-4 by hand on the rebased branch (UPDATE-RUNBOOK 4.2/4.3):")
        say("    python tools/arifi-sync/arifi_sync.py build")
        say("    python tools/arifi-sync/arifi_sync.py judge --i-have-read-bench-purity")
        return 1
    say("candidate identity : the checkout is IDENTICAL to the replayed candidate outside %s,"
        % cfg["series_dir"])
    say("                     and %s regenerates byte-identically from git and is committed."
        % cfg["series_dir"])

    step("3/4  build")
    if cmd_build(repo, cfg, args) != 0:
        say("\nBUMP REFUSED: build failed.")
        return 1
    step("4/4  llama-server regression judge (F-085)")
    if cmd_judge(repo, cfg, args) != 0:
        say("\nBUMP REFUSED: judge did not pass.")
        return 1
    say("\nAll three gates passed, on the candidate itself: the tree that was built and judged is")
    say("the tree the series replays onto %s. If you reached this by materialising the candidate,"
        % args.onto)
    say("the base pin and the series are already updated and committed - verify with:")
    say("  python tools/arifi-sync/arifi_sync.py series check --ref %s" % args.ref)
    return 0


# ---------------------------------------------------------------- status

# ---------------------------------------------------------------------------------------------
# scope - the ingest A/B scope, DERIVED from the collision map (UPDATE-RUNBOOK 4.1b.4, President
# 2026-09-07 06:5x, F-161). Works for an upstream bump (old=base, new=tag) and for a fork re-ingest
# (old=the ref we ingested last time, new=the fork's current ref). Prints the map + ONE scope line.
# ---------------------------------------------------------------------------------------------
SPEED_PATH_PREFIXES = ("ggml/", "src/llama-", "src/llama.cpp", "common/speculative", "tools/server/",
                       "tools/cli/", "common/sampling", "common/common.cpp")
SPEED_PATH_IGNORE_SUFFIX = (".md", ".txt", ".yml", ".yaml", ".json", ".svelte", ".ts", ".css", ".html")


def _is_speed_path(path: str) -> bool:
    if path.endswith(SPEED_PATH_IGNORE_SUFFIX):
        return False
    return path.startswith(SPEED_PATH_PREFIXES)


def _hunks(repo: str, a: str, b: str, path: str, pad: int = 3):
    """Post-image line ranges of every hunk a..b touches in path (from `git diff -U0`), padded."""
    _, out, _ = git(repo, "diff", "-U0", a, b, "--", path, check=False)
    ranges = []
    for line in out.split("\n"):
        if not line.startswith("@@"):
            continue
        # @@ -l,s +l,s @@
        try:
            new = line.split("+", 1)[1].split(" ", 1)[0]
            start, _, count = new.partition(",")
            start = int(start); count = int(count) if count else 1
            ranges.append((max(1, start - pad), start + max(count, 1) + pad))
        except ValueError:
            continue
    return ranges


def _overlaps(r1, r2) -> int:
    n = 0
    for a0, a1 in r1:
        for b0, b1 in r2:
            if a0 <= b1 and b0 <= a1:
                n += 1
                break
    return n


def scope_verdict(collisions: int, speed_touched: int) -> str:
    if collisions > 0:
        return "named"
    if speed_touched > 0:
        return "full"
    return "sanity"


def cmd_scope(repo: str, cfg: dict, args) -> int:
    old = args.old or cfg["base"]["upstream_sha"]
    new = args.new
    ours = args.ref
    for r in (old, new, ours):
        if _resolve_ref(repo, r) == "":
            raise Loud("cannot resolve %s - fetch first: arifi_sync.py currency" % r)
    step("SCOPE %s -> %s (ours = %s): the A/B scope is derived from the collision map, never inherited" % (old, new, ours))
    _, theirs, _ = git(repo, "diff", "--name-only", old, new)
    their_paths = sorted(p for p in theirs.split("\n") if p)
    _, mine, _ = git(repo, "diff", "--name-only", old, ours)
    our_paths = set(p for p in mine.split("\n") if p)
    inter = [p for p in their_paths if p in our_paths]
    speed = [p for p in their_paths if _is_speed_path(p)]
    collisions = []
    for p in inter:
        up = _hunks(repo, old, new, p)
        us = _hunks(repo, old, ours, p)
        n = _overlaps(up, us)
        say("  %-60s upstream %d hunk(s), ours %d hunk(s), COLLIDING %d" % (p, len(up), len(us), n))
        if n:
            collisions.append((p, n))
    say("upstream delta files   : %d" % len(their_paths))
    say("our patch files        : %d" % len(our_paths))
    say("intersecting files     : %d" % len(inter))
    say("speed-path files moved : %d" % len(speed))
    for p in speed[:40]:
        say("    speed: %s" % p)
    say("named collisions       : %d" % len(collisions))
    for p, n in collisions:
        say("    COLLISION %s (%d hunk pair(s))" % (p, n))
    verdict = scope_verdict(len(collisions), len(speed))
    owed = {
        "sanity": "build + correctness gates + F-141 both arms + ONE file A/B (seated line, plain + DFlash2), expected CI containing 0",
        "named": "4.1b.3 three arms on each named collision surface + the sanity pair",
        "full": "the five-file matrix (a speed path moved with no collision: the change can move every file) - name the forcing commit",
    }[verdict]
    say("SCOPE: %s -- %s" % (verdict, owed))
    if getattr(args, "json", None):
        # `Path` was never imported - `scope --json` raised NameError on the one line that writes
        # its output. Disclosed by the R2 maker and re-disclosed by the R2 checker; fixed here with
        # the stdlib call the rest of this file already uses.
        import json as _json
        with open(args.json, "w", encoding="utf-8", newline="\n") as _fh:
            _fh.write(_json.dumps({
                "old": old, "new": new, "ours": ours, "upstream_files": their_paths,
                "our_files": sorted(our_paths), "intersection": inter, "speed_paths": speed, "collisions": collisions,
                "scope": verdict, "owed": owed,
            }, indent=2))
        say("map written: %s" % args.json)
    return 0


def _scope_selfcheck() -> None:
    # RED/GREEN: the three verdicts and the overlap arithmetic, no git needed.
    assert scope_verdict(0, 0) == "sanity"
    assert scope_verdict(0, 2) == "full"
    assert scope_verdict(1, 5) == "named"
    assert _overlaps([(10, 20)], [(15, 30)]) == 1
    assert _overlaps([(10, 20)], [(21, 30)]) == 0
    assert _overlaps([(10, 20), (40, 50)], [(45, 46), (5, 12)]) == 2
    assert _is_speed_path("ggml/src/ggml-vulkan/ggml-vulkan.cpp") and not _is_speed_path("ggml/src/README.md")
    assert not _is_speed_path("tools/ui/src/app.svelte") and _is_speed_path("tools/server/server.cpp")


def cmd_status(repo: str, cfg: dict, args) -> int:
    base = cfg["base"]["upstream_sha"]
    ref = cfg["base"].get("ref", "master")
    say("repo            : %s" % repo)
    say("HEAD            : %s  (%s)" % (gout(repo, "rev-parse", "--short", "HEAD"),
                                        gout(repo, "rev-parse", "--abbrev-ref", "HEAD")))
    say("pinned base     : %s  %s" % (cfg["base"]["upstream_tag"], base[:9]))
    say("series ref      : %s" % ref)
    say("series commits  : %s" % gout(repo, "rev-list", "--count", "%s..%s" % (base, ref)))
    say("merge commits   : %s   (must be 0 - the series is linear on purpose)"
        % gout(repo, "rev-list", "--count", "--merges", "%s..%s" % (base, ref)))
    say("patches on disk : %d" % len(series_files(os.path.join(repo, cfg["series_dir"]))))
    st = read_state(repo, cfg)
    say("last currency   : %s" % st.get("last_currency_run", "NEVER"))
    say("")
    say("Next: series check | provenance --strict | currency | recipe diff")
    return 0


# ---------------------------------------------------------------- cli

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="arifi_sync", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config", default=DEFAULT_CONFIG)
    ap.add_argument("--repo", default=None, help="repo root (default: discovered from --config)")
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("status")

    p = sub.add_parser("scope", help="ingest A/B scope from the collision map (runbook 4.1b.4)")
    p.add_argument("--old", default=None, help="old base / last-ingested ref (default sources.json base)")
    p.add_argument("--new", required=True, help="new upstream tag or fork ref")
    p.add_argument("--ref", default="arifi/main", help="our line")
    p.add_argument("--json", default=None, help="write the map as JSON")
    p.add_argument("--selftest", action="store_true")
    p = sub.add_parser("provenance")
    p.add_argument("--ref", default=None)
    p.add_argument("--strict", action="store_true",
                   help="also fail on loose-only trailers and missing Measured-effect")

    ps = sub.add_parser("series")
    pss = ps.add_subparsers(dest="sub", required=True)
    a = pss.add_parser("regen"); a.add_argument("--ref", default=None)
    a = pss.add_parser("check"); a.add_argument("--ref", default=None)
    a = pss.add_parser("replay"); a.add_argument("--onto", required=True)

    p = sub.add_parser("currency")
    p.add_argument("--no-fetch", action="store_true", help="report only; do not touch the network")

    pr = sub.add_parser("recipe")
    prs = pr.add_subparsers(dest="sub", required=True)
    a = prs.add_parser("diff")
    a.add_argument("--snapshot"); a.add_argument("--live")
    a = prs.add_parser("export")
    a.add_argument("--live"); a.add_argument("--initial-cache", default=None)
    a.add_argument("--snapshot", action="store_true", help="also refresh the committed snapshot")

    pw = sub.add_parser("protected-win")
    pws = pw.add_subparsers(dest="sub", required=True)
    a = pws.add_parser("validate")
    a.add_argument("--ref", default=None)
    a.add_argument("--draft", action="store_true",
                   help="validate the UNCOMMITTED working tree by snapshotting it into a "
                        "throwaway commit; commits nothing and certifies no other ref")
    a.add_argument("--allow-new", action="append", default=[], metavar="PATH",
                   help="with --draft: authorise ONE untracked file to be included in the "
                        "snapshot. Repeatable. Without it, any untracked non-ignored file "
                        "REFUSES the draft before a Git object is written.")
    a = pws.add_parser("replay-map",
                       help="derive the rebased->original mapping for a replayed line")
    a.add_argument("--ref", default=None, help="our line AFTER the rebase")
    a.add_argument("--retained-ref", required=True,
                   help="a real ref that still contains the PRE-REBASE commits, e.g. a tag made "
                        "before rewriting. The reflog is not evidence anybody else can check.")
    a.add_argument("--recorded-by", required=True,
                   help="who is recording this mapping (it is a registration statement)")
    a.add_argument("--write", action="store_true",
                   help="write tools/arifi-sync/replay-provenance.json (still uncommitted)")
    a = pws.add_parser("check")
    a.add_argument("--ref", default=None)
    a.add_argument("--incoming", required=True,
                   help="the upstream tag/sha or fork ref you are about to ingest")
    a = pws.add_parser("discover")
    a.add_argument("--ref", default=None)
    a.add_argument("--strict", action="store_true",
                   help="exit 1 when a measured win has no manifest entry")
    a.add_argument("--all", action="store_true",
                   help="informational: also list the dated baseline rows and their dispositions")

    p = sub.add_parser("set-base-pin",
                       help="move base.upstream_sha + base.upstream_tag in sources.json")
    p.add_argument("--onto", required=True,
                   help="the ref being pinned, e.g. b10825 - resolved here, never typed as a sha")
    p.add_argument("--expect-sha", required=True,
                   help="the pin you believe is on disk right now. A mismatch REFUSES: somebody "
                        "else moved it, or this is not the checkout you think it is.")
    p.add_argument("--upstream-tag", default=None,
                   help="override the tag, which is otherwise DERIVED with "
                        "`git describe --tags --exact-match`. It must name the same commit.")

    sub.add_parser("build")
    p = sub.add_parser("judge")
    p.add_argument("--i-have-read-bench-purity", action="store_true")
    p = sub.add_parser("bump")
    p.add_argument("--onto", required=True)
    # lane-152 added --ref because cmd_bump fell back to "master", a dead branch, and printed a
    # plausible rebase surface computed against it. lane-229 makes it REQUIRED: the default read
    # `base.ref` out of the very sources.json whose staleness is the thing under suspicion, so the
    # candidate was named by the config instead of by the operator. It is explicit now.
    p.add_argument("--ref", required=True,
                   help="our line, e.g. arifi/main - explicit, never defaulted from sources.json")
    p.add_argument("--i-have-read-bench-purity", action="store_true")

    args = ap.parse_args(argv)
    try:
        cfg = load_config(args.config)
        repo = args.repo or repo_root(os.path.dirname(os.path.abspath(args.config)))
    except Loud as e:
        say("\n" + "=" * 78)
        say("STOP: %s" % e)
        say("=" * 78)
        return 2

    # `repo` is DERIVED from the config's directory, so a --repo that points somewhere else means
    # one checkout's configuration is driving another checkout's tree - the wrong-tree trap at its
    # source, before any subcommand gets a chance to be careful about it.
    cfg_path = os.path.abspath(args.config)
    expected_cfg = os.path.join(os.path.abspath(repo), "tools", "arifi-sync")
    if os.path.dirname(cfg_path) != expected_cfg:
        say("STOP: the config and the repository are two different checkouts.\n"
            "  --config : %s\n"
            "  --repo   : %s\n"
            "Configuration is part of the tree it configures. Run the copy of this script that\n"
            "lives in the repository you mean:\n"
            "  python %s\\tools\\arifi-sync\\arifi_sync.py ..." % (cfg_path, repo, repo))
        return 2

    # base sha and the branch it describes are one pair; --ref only overrides it explicitly.
    # Whether the operator NAMED a ref is recorded before the default is applied - `--draft` has to
    # be able to tell "no ref given" from "the config's ref", and after this line it cannot.
    args.ref_explicit = getattr(args, "ref", None) is not None
    if getattr(args, "ref", "") is None:
        args.ref = cfg["base"].get("ref", "master")

    table = {
        ("status", None): cmd_status,
        ("provenance", None): cmd_provenance,
        ("series", "regen"): cmd_series_regen,
        ("series", "check"): cmd_series_check,
        ("series", "replay"): cmd_series_replay,
        ("currency", None): cmd_currency,
        ("recipe", "diff"): cmd_recipe_diff,
        ("recipe", "export"): cmd_recipe_export,
        ("build", None): cmd_build,
        ("judge", None): cmd_judge,
        ("bump", None): cmd_bump,
        ("set-base-pin", None): cmd_set_base_pin,
        ("scope", None): cmd_scope,
        ("protected-win", "validate"): cmd_protected_win_validate,
        ("protected-win", "check"): cmd_protected_win_check,
        ("protected-win", "discover"): cmd_protected_win_discover,
        ("protected-win", "replay-map"): cmd_protected_win_replay_map,
    }
    fn = table[(args.cmd, getattr(args, "sub", None))]
    try:
        return fn(repo, cfg, args)
    except Loud as e:
        say("\n" + "=" * 78)
        say("STOP: %s" % e)
        say("=" * 78)
        return 2


if __name__ == "__main__":
    sys.exit(main())
