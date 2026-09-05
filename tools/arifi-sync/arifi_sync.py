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
    bump          replay + build + judge onto a new upstream ref; refuses unless ALL THREE pass
    protected-win validate   check the protected-win manifest is complete and still true of the tree
    protected-win check      FAIL-CLOSED ingest preflight: refuse an incoming change that touches a
                             protected win without a recorded, evidence-backed comparison verdict
    protected-win discover   list commits with a real Measured-effect that no manifest entry covers

Exit code is 0 only when the subcommand fully passed.
"""
from __future__ import annotations

import argparse
import datetime as _dt
import filecmp
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


def git(repo: str, *args: str, check: bool = True, binary: bool = False):
    p = subprocess.run(["git", "-C", repo, *args],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check and p.returncode != 0:
        raise Loud("git %s failed in %s\n%s" %
                   (" ".join(args), repo, p.stderr.decode("utf-8", "replace")))
    out = p.stdout if binary else p.stdout.decode("utf-8", "replace")
    return p.returncode, out, p.stderr.decode("utf-8", "replace")


def gout(repo: str, *args: str) -> str:
    return git(repo, *args)[1].strip()


def load_config(path: str) -> dict:
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


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
    strict, loose, meas, native, gf = 0, 0, 0, 0, 0
    loose_only, missing, no_meas = [], [], []
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
            no_meas.append((sha[:9], subject))
        if is_loose and not is_strict:
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


def cmd_series_check(repo: str, cfg: dict, args) -> int:
    base = cfg["base"]["upstream_sha"]
    committed = os.path.join(repo, cfg["series_dir"])
    step("Integrity check: regenerating the series and byte-comparing against %s" % cfg["series_dir"])
    tmp = tempfile.mkdtemp(prefix="arifi-series-")
    try:
        format_patch(repo, cfg, base, args.ref, tmp)
        have = series_files(committed)
        want = series_files(tmp)
        rc = 0
        only_committed = [f for f in have if f not in want]
        only_regen = [f for f in want if f not in have]
        if only_committed:
            say("FAIL: committed but not regenerated (stale):")
            for f in only_committed:
                say("  %s" % f)
            rc = 1
        if only_regen:
            say("FAIL: regenerated but not committed (missing):")
            for f in only_regen:
                say("  %s" % f)
            rc = 1
        differing = [f for f in want if f in have
                     and not filecmp.cmp(os.path.join(tmp, f), os.path.join(committed, f),
                                         shallow=False)]
        if differing:
            say("FAIL: content differs from git for %d patch(es):" % len(differing))
            for f in differing:
                say("  %s" % f)
            rc = 1
        if rc:
            say("\nThe committed series has DRIFTED from git. Fix with:")
            say("  python tools/arifi-sync/arifi_sync.py series regen")
            return 1
        say("PASS: %d patches byte-identical to a fresh generation from git." % len(want))

        # The comparison above is against the WORKING DIRECTORY, which is not the same thing as
        # "committed" no matter what the help text used to say. An untracked patch file satisfies
        # every check above and then does not exist in a fresh clone, so `git am` dies on it.
        # That defect shipped twice (lane-147, then again inside lane-151's own regen), so the
        # committed-ness is now asserted separately and by name. Added lane-151, 2026-08-18.
        step("Tracked check: every patch in the series is a committed blob at %s" % args.ref)
        tracked = set()
        out = gout(repo, "ls-tree", "-r", "--name-only", args.ref, "--", cfg["series_dir"]).splitlines()
        prefix = cfg["series_dir"].replace("\\", "/").rstrip("/") + "/"
        for line in out:
            line = line.strip().replace("\\", "/")
            if line.startswith(prefix):
                tracked.add(line[len(prefix):])
        untracked = [f for f in want if f not in tracked]
        if untracked:
            say("FAIL: %d patch(es) exist on disk but are NOT COMMITTED at %s:" % (len(untracked), args.ref))
            for f in untracked:
                say("  %s" % f)
            say("\nA fresh clone would die at the first of these. Fix with:")
            say("  git add %s && git commit" % cfg["series_dir"])
            return 1
        say("PASS: all %d patches are committed blobs, not just files on disk." % len(want))

        step("Replay check: git am the committed series onto %s" % base[:9])
        return _replay(repo, cfg, base, committed, expect_ref=args.ref)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def _replay(repo: str, cfg: dict, onto: str, series_path: str, expect_ref: str = "") -> int:
    """Replay the series onto `onto` in a throwaway worktree. Loud, per-patch, never auto-resolves."""
    files = [os.path.join(series_path, f) for f in series_files(series_path)]
    if not files:
        raise Loud("no patches found in %s" % series_path)
    parent = os.path.dirname(repo.rstrip("/\\"))
    wt = os.path.join(parent, ".arifi-replay")
    branch = "arifi-sync/replay"
    if os.path.exists(wt):
        git(repo, "worktree", "remove", "--force", wt, check=False)
        shutil.rmtree(wt, ignore_errors=True)
    git(repo, "branch", "-D", branch, check=False)
    say("worktree: %s   (short path on purpose - llama.cpp's tools/ui tree blows past" % wt)
    say("           Windows MAX_PATH from a deep temp directory)")
    git(repo, "worktree", "add", "--detach", wt, onto)
    try:
        git(wt, "checkout", "-B", branch, onto)
        for i, patch in enumerate(files, 1):
            name = os.path.basename(patch)
            rc, out, err = git(wt, "am", "--keep-non-patch", patch, check=False)
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
        say("\nreplayed %d patches cleanly -> %s (tree %s)" % (len(files), tip[:9], tree[:9]))
        if expect_ref:
            # The series excludes its own directory (see format_patch), so the replayed tree
            # legitimately lacks it. Everything else must match the reference to the byte, and
            # that is asserted by a real diff rather than by trusting the patch count.
            expect = gout(repo, "rev-parse", expect_ref)
            rc, out, _ = git(wt, "diff", "--stat", expect, "HEAD",
                             "--", ".", ":(exclude)%s" % cfg["series_dir"], check=False)
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

def cmd_build(repo: str, cfg: dict, args) -> int:
    bdir = os.path.join(repo, cfg["build"]["build_dir"])
    if not os.path.exists(os.path.join(bdir, "CMakeCache.txt")):
        raise Loud("no configured build at %s.\n"
                   "Configure it from the recorded recipe first:\n"
                   "  python tools/arifi-sync/arifi_sync.py recipe export\n"
                   "  cmake -S . -B %s -C arifi-recipe.cmake"
                   % (bdir, cfg["build"]["build_dir"]))
    step("recipe diff before building (a silently changed flag is a failed build later)")
    cmd_recipe_diff(repo, cfg, argparse.Namespace(snapshot=None, live=None))
    targets = cfg["build"]["targets"]
    step("cmake --build %s --target %s" % (cfg["build"]["build_dir"], " ".join(targets)))
    cmd = ["cmake", "--build", bdir, "--config", "Release"]
    for t in targets:
        cmd += ["--target", t]
    p = subprocess.run(cmd)
    if p.returncode != 0:
        say("\nBUILD FAILED (exit %d). Stopping - a bump is never accepted on a failed build."
            % p.returncode)
        return 1
    say("build ok")
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
    cands = [os.path.join(bdir, "bin", "llama-server.exe"),
             os.path.join(bdir, "bin", "llama-server"),
             os.path.join(bdir, "bin", "Release", "llama-server.exe")]
    binary = next((c for c in cands if os.path.exists(c)), None)
    if not binary:
        raise Loud("llama-server not found. Looked in:\n  %s" % "\n  ".join(cands))
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
        for e in w["evidence"]:
            if not _evidence_resolves(repo, e):
                problems.append("%s: evidence locator %r does not resolve" % (wid, e))
    if cfg is not None:
        problems.extend(baseline_boundary_problems(repo))
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
    step("Validating %s against %s" % (PROTECTED_WINS, ref))
    problems = validate_protected_wins(repo, ref, cfg)
    wins = load_protected_wins(repo)
    active = [w for w in wins if w.get("status") == "active"]
    say("entries          : %d  (%d active, %d superseded)"
        % (len(wins), len(active), len(wins) - len(active)))
    say("protected paths  : %d" % sum(len(w.get("protected_paths") or []) for w in wins))
    say("anchors          : %d" % sum(len(w.get("anchors") or []) for w in wins))
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
    rows = []
    for sha in gout(repo, "rev-list", "--reverse", "%s..%s" % (base, ref)).split():
        if sha in covered or sha in baseline:
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
    if _replay(repo, cfg, args.onto, os.path.join(repo, cfg["series_dir"])) != 0:
        say("\nBUMP REFUSED: replay failed.")
        return 1
    step("3/4  build")
    if cmd_build(repo, cfg, args) != 0:
        say("\nBUMP REFUSED: build failed.")
        return 1
    step("4/4  llama-server regression judge (F-085)")
    if cmd_judge(repo, cfg, args) != 0:
        say("\nBUMP REFUSED: judge did not pass.")
        return 1
    say("\nAll three gates passed. Update base.upstream_sha/upstream_tag in sources.json,")
    say("then: python tools/arifi-sync/arifi_sync.py series regen")
    return 0


# ---------------------------------------------------------------- status

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
    a = pws.add_parser("validate"); a.add_argument("--ref", default=None)
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

    sub.add_parser("build")
    p = sub.add_parser("judge")
    p.add_argument("--i-have-read-bench-purity", action="store_true")
    p = sub.add_parser("bump")
    p.add_argument("--onto", required=True)
    p.add_argument("--ref", default=None)   # lane-152: without this, cmd_bump fell back to "master"
    p.add_argument("--i-have-read-bench-purity", action="store_true")

    args = ap.parse_args(argv)
    cfg = load_config(args.config)
    repo = args.repo or repo_root(os.path.dirname(os.path.abspath(args.config)))

    # base sha and the branch it describes are one pair; --ref only overrides it explicitly
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
        ("protected-win", "validate"): cmd_protected_win_validate,
        ("protected-win", "check"): cmd_protected_win_check,
        ("protected-win", "discover"): cmd_protected_win_discover,
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
