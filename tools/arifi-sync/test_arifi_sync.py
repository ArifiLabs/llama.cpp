#!/usr/bin/env python3
"""Focused tests for the two chokepoints lane-209's R2 closure hardened.

    python tools/arifi-sync/test_arifi_sync.py

stdlib unittest, no fixtures, no network, no dependency on this repository's own history -
every case builds a throwaway git repo in a temp directory. That independence is the point:
`arifi/main` cannot exercise the regen SUCCESS path at all (it carries 10 pre-existing merge
commits, so regen is expected to refuse there), and a test that could only run against one
branch state would prove nothing about the guard.

What is asserted, and why each one exists:

  regen  - the pre-flight now runs BEFORE the delete. The negative cases therefore assert the
           property that actually failed on 2026-09-04: after a refusal, patches/series/ is
           byte-for-byte what it was. A file COUNT would have passed the old code on the day it
           deleted 415 files, so the assertion hashes contents.
  protected-win - the preflight is FAIL-CLOSED, which means the interesting tests are the ones
           where it must REFUSE: a collision with no resolution, and a manifest that is itself
           incomplete. A guard that only ever passes is a guard nobody has tested.
"""
import contextlib
import hashlib
import io
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import arifi_sync as A  # noqa: E402


def git(repo, *args, check=True):
    r = subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True)
    if check and r.returncode != 0:
        raise AssertionError("git %s failed: %s%s" % (" ".join(args), r.stdout, r.stderr))
    return r.stdout.strip()


def commit_json(repo_path, rel, doc, trailers=False):
    """Write a tools/arifi-sync JSON fixture AND COMMIT IT, by pathspec.

    The manifests are TRACKED files of the tree they describe, and since lane-229 the validator
    refuses to certify a ref with config/manifests that are not the ones at that ref (the
    wrong-tree trap: lane-206's 21-entry manifest validated against arifi/main, which carries 30).
    A fixture that only wrote to disk would therefore be exercising an invocation the tool now
    refuses, and every case below it would be measuring the refusal instead of its own subject.
    """
    p = os.path.join(repo_path, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(doc, fh)
        fh.write("\n")
    git(repo_path, "add", "--", rel)
    if git(repo_path, "status", "--porcelain", "--", rel):
        msg = "fixture: %s" % rel
        if trailers:
            # Only where the fixture's own commits have to survive `regen_preflight`, which
            # refuses a range containing a commit with no provenance at all.
            msg += "\n\nOrigin: ArifiLabs (native)\nMeasured-effect: tooling only - fixture\n"
        git(repo_path, "commit", "-q", "-m", msg, "--", rel)


def series_fingerprint(d):
    """Content hash of every file in the series directory. Not a count - a count is exactly
    what would have failed to notice the incident this guard exists for."""
    if not os.path.isdir(d):
        return {}
    out = {}
    for name in sorted(os.listdir(d)):
        p = os.path.join(d, name)
        if os.path.isfile(p):
            with open(p, "rb") as fh:
                out[name] = hashlib.sha256(fh.read()).hexdigest()
    return out


class Repo:
    """A throwaway fork-shaped repository: a base commit, a series dir, and a sources.json."""

    def __init__(self):
        # Each fixture gets its OWN parent directory. `_replay` puts its worktree in the
        # repository's SIBLING `../.arifi-replay` (short path, Windows MAX_PATH), so every repo
        # created straight into %TEMP% shared one replay path - fine while every replay was
        # stubbed, a flake source now that real `git am` replays run here.
        self.home = tempfile.mkdtemp(prefix="arifi-test-")
        self.path = os.path.join(self.home, "repo")
        os.makedirs(self.path)
        git(self.path, "init", "-q", "-b", "main")
        git(self.path, "config", "user.email", "test@arifilabs.invalid")
        git(self.path, "config", "user.name", "arifi-sync test")
        git(self.path, "config", "commit.gpgsign", "false")
        # Two settings that would otherwise make the fixtures lie about the code under test:
        # autocrlf rewrites a generated patch on checkout, which looks exactly like a mutation
        # the guard failed to prevent; and `git add -A` would sweep the generated series and the
        # manifest into a commit, so a branch switch would delete them. Commit by pathspec only -
        # the same rule the estate applies to itself.
        git(self.path, "config", "core.autocrlf", "false")
        self.write("README.md", "base\n")
        git(self.path, "add", "--", "README.md")
        git(self.path, "commit", "-q", "-m", "base")
        self.base = git(self.path, "rev-parse", "HEAD")
        os.makedirs(os.path.join(self.path, "tools", "arifi-sync"), exist_ok=True)
        os.makedirs(os.path.join(self.path, "patches", "series"), exist_ok=True)
        self.cfg = {
            "base": {"upstream_sha": self.base, "upstream_tag": "b-test", "ref": "main"},
            "series_dir": "patches/series",
            "remotes": [], "local_checkouts": [],
        }

    # -- helpers -------------------------------------------------------------
    def write(self, rel, text):
        p = os.path.join(self.path, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)

    def commit(self, subject, files, trailers=True, ref=None, effect="test fixture"):
        for rel, text in files.items():
            self.write(rel, text)
        git(self.path, "add", "--", *files)
        msg = subject
        if trailers:
            msg += "\n\nOrigin: ArifiLabs (native)\nMeasured-effect: %s\n" % effect
        git(self.path, "commit", "-q", "-m", msg)
        return git(self.path, "rev-parse", "HEAD")

    @property
    def series_dir(self):
        return os.path.join(self.path, "patches", "series")

    def regen(self, ref="main"):
        args = type("A", (), {"ref": ref})()
        try:
            return A.cmd_series_regen(self.path, self.cfg, args)
        except A.Loud:
            return 2

    def regen_and_commit(self, ref="main"):
        """Regenerate the series and COMMIT it, which is what a real update does. An uncommitted
        patch satisfies a byte compare and then does not exist in a fresh clone."""
        assert self.regen(ref) == 0
        git(self.path, "add", "--", "patches/series")
        if git(self.path, "status", "--porcelain", "--", "patches/series"):
            git(self.path, "commit", "-q", "-m",
                "regen: bank the series\n\nOrigin: ArifiLabs (native)\n"
                "Measured-effect: generated artifact refresh\n", "--", "patches/series")

    def close(self):
        shutil.rmtree(self.home, ignore_errors=True)


class SeriesRegenTest(unittest.TestCase):
    def setUp(self):
        self.r = Repo()

    def tearDown(self):
        self.r.close()

    def test_success_produces_the_expected_series(self):
        self.r.commit("feat: one", {"a.txt": "1\n"})
        self.r.commit("feat: two", {"b.txt": "2\n"})
        self.assertEqual(0, self.r.regen())
        names = sorted(os.listdir(self.r.series_dir))
        patches = [n for n in names if n.endswith(".patch")]
        self.assertEqual(2, len(patches), names)
        self.assertIn("SERIES", names)
        self.assertIn("MANIFEST.md", names)
        with open(os.path.join(self.r.series_dir, "SERIES"), encoding="utf-8") as fh:
            listed = [l.strip() for l in fh if l.strip() and not l.startswith("#")]
        self.assertEqual(patches, listed)

    def test_non_linear_branch_refuses_and_leaves_the_series_intact(self):
        self.r.commit("feat: one", {"a.txt": "1\n"})
        self.assertEqual(0, self.r.regen())
        before = series_fingerprint(self.r.series_dir)
        self.assertTrue(before)

        # a merge commit in base..ref - the exact condition arifi/main is in today
        git(self.r.path, "checkout", "-q", "-b", "side", self.r.base)
        self.r.commit("feat: side", {"c.txt": "3\n"})
        git(self.r.path, "checkout", "-q", "main")
        git(self.r.path, "merge", "-q", "--no-ff", "-m", "merge side", "side")

        self.assertNotEqual(0, self.r.regen())
        self.assertEqual(before, series_fingerprint(self.r.series_dir),
                         "regen refused but mutated patches/series/")

    def test_provenance_failure_refuses_and_leaves_the_series_intact(self):
        self.r.commit("feat: one", {"a.txt": "1\n"})
        self.assertEqual(0, self.r.regen())
        before = series_fingerprint(self.r.series_dir)
        self.assertTrue(before)

        self.r.commit("feat: untrailered", {"d.txt": "4\n"}, trailers=False)
        self.assertNotEqual(0, self.r.regen())
        self.assertEqual(before, series_fingerprint(self.r.series_dir),
                         "regen refused on provenance but mutated patches/series/")

    def _provenance(self, strict=True, ref="main"):
        """Run cmd_provenance and return (rc, printed output)."""
        args = type("A", (), {"ref": ref, "strict": strict})()
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = A.cmd_provenance(self.r.path, self.r.cfg, args)
        return rc, buf.getvalue()

    def test_a_sha_pinned_trailer_exemption_clears_strict_but_is_never_reported_as_repaired(self):
        """The exemption ledger is the ONLY way a loose-only commit can pass --strict without
        amending it, and passing must never look like a repair. `native-grandfather.json` does
        not reach the loose-only class at all (cmd_provenance consults it for the missing
        Measured-effect case only), which is why this second ledger exists."""
        # A LOOSE-ONLY commit: the tokens are in the message, but a column-0 continuation line
        # ends git's trailer block, so nothing parses. The real defect, reproduced.
        self.r.write("e.txt", "5\n")
        git(self.r.path, "add", "--", "e.txt")
        git(self.r.path, "commit", "-q", "-m",
            "feat: wrapped\n\nOrigin: ArifiLabs (native) because the value wraps\nonto column zero, which ends the block.\n")
        sha = git(self.r.path, "rev-parse", "HEAD")

        rc, out = self._provenance()
        self.assertNotEqual(0, rc, "a loose-only commit must fail --strict before exemption")
        self.assertIn("FAIL (--strict)", out)

        # Grandfathering it is NOT the remedy - the closed ledger does not cover this class.
        commit_json(self.r.path, "tools/arifi-sync/native-grandfather.json",
                    {"commits": [{"sha": sha}]}, trailers=True)
        rc, _ = self._provenance()
        self.assertNotEqual(0, rc, "native-grandfather.json must not clear a loose-only commit")

        commit_json(self.r.path, "tools/arifi-sync/trailer-exemptions.json",
                    {"commits": [{"sha": sha, "subject": "feat: wrapped",
                                  "classification": ["loose-only"],
                                  "reason": "test fixture"}]}, trailers=True)
        rc, out = self._provenance()
        self.assertEqual(0, rc, out)
        self.assertIn("EXEMPT, NOT REPAIRED      : ", out)
        self.assertIn(sha[:9], out)
        self.assertIn("EXEMPT, not repaired", out)
        self.assertNotIn("PASS: every commit carries provenance.", out,
                         "an exempted commit must never produce the unqualified PASS line")

    def test_a_no_provenance_commit_clears_only_with_a_ledger_origin(self):
        """NO PROVENANCE AT ALL is the hard failure. An exemption row clears it only when it is
        classified `no-provenance` AND records the origin itself; a bare row does not."""
        self.r.write("np.txt", "1\n")
        git(self.r.path, "add", "--", "np.txt")
        git(self.r.path, "commit", "-q", "-m", "feat: landed without trailers\n")
        sha = git(self.r.path, "rev-parse", "HEAD")
        rc, out = self._provenance(strict=False)
        self.assertEqual(1, rc)
        self.assertIn("NO PROVENANCE AT ALL", out)
        row = {"sha": sha, "subject": "feat: landed without trailers",
               "classification": ["no-provenance", "no-measured-effect"], "reason": "test fixture"}
        commit_json(self.r.path, "tools/arifi-sync/trailer-exemptions.json",
                    {"commits": [row]}, trailers=True)
        rc, _ = self._provenance()
        self.assertEqual(1, rc, "a row without an origin must not clear the hard failure")
        row["origin"] = "ArifiLabs (native) - test"
        commit_json(self.r.path, "tools/arifi-sync/trailer-exemptions.json",
                    {"commits": [row]}, trailers=True)
        rc, out = self._provenance()
        self.assertEqual(0, rc, out)
        self.assertIn("ledger origin: ArifiLabs (native) - test", out)
        self.assertNotIn("PASS: every commit carries provenance.", out)

    def test_preflight_runs_before_any_deletion_even_on_a_fresh_directory(self):
        """The series directory is empty, so a count-based assertion cannot distinguish the
        old order from the new one. This asserts the pre-flight fires at all."""
        self.r.commit("feat: untrailered", {"d.txt": "4\n"}, trailers=False)
        self.assertNotEqual(0, self.r.regen())
        self.assertEqual({}, series_fingerprint(self.r.series_dir))


PATHS = {"ggml/kernel.c": "int win(void) { return ARIFI_FAST_PATH; }\n"}


class ProtectedWinTest(unittest.TestCase):
    def setUp(self):
        self.r = Repo()
        self.win_sha = self.r.commit("vulkan: our measured win", dict(PATHS))
        self.manifest_path = os.path.join(self.r.path, "tools", "arifi-sync", "protected-wins.json")
        self._write_manifest(self._entry())

    def tearDown(self):
        self.r.close()

    def _entry(self, **over):
        e = {
            "id": "test-win", "status": "active", "mechanism": "the fast path",
            "commit": self.win_sha, "protected_paths": ["ggml/kernel.c"],
            "anchors": ["ARIFI_FAST_PATH"], "evidence": ["commit:" + self.win_sha],
            "quality_gate": "byte-identical output", "workload": "the seat serve line",
            "measured_effect": "-12% per dispatch",
        }
        e.update(over)
        return e

    def _write_manifest(self, *entries):
        commit_json(self.r.path, "tools/arifi-sync/protected-wins.json",
                    {"version": 1, "wins": list(entries)})

    def _write_resolutions(self, *rows):
        commit_json(self.r.path, "tools/arifi-sync/protected-win-resolutions.json",
                    {"resolutions": list(rows)})

    def _incoming(self, files, subject="upstream: change", start=None):
        """A branch with a genuine merge base against `main`.

        `start` defaults to the BASE, which models an UPSTREAM bump: upstream never carries our
        code, so its diff against the merge base can only ADD or move files - never remove one of
        our anchor lines. Pass `start="main"` to model a FORK INGEST, where the tracked fork does
        share our lineage and can delete our branch while keeping the file."""
        git(self.r.path, "checkout", "-q", "-b", "incoming", start or self.r.base)
        sha = self.r.commit(subject, files)
        git(self.r.path, "checkout", "-q", "main")
        return sha

    def _check(self, incoming):
        args = type("A", (), {"ref": "main", "incoming": incoming})()
        try:
            return A.cmd_protected_win_check(self.r.path, self.r.cfg, args)
        except A.Loud:
            return 2

    # -- cases ---------------------------------------------------------------
    def test_validate_passes_on_a_complete_manifest(self):
        self.assertEqual([], A.validate_protected_wins(self.r.path, "main"))

    def test_no_collision_passes(self):
        self.assertEqual(0, self._check(self._incoming({"docs/x.md": "unrelated\n"})))

    def test_collision_without_a_recorded_comparison_is_REFUSED(self):
        inc = self._incoming({"ggml/kernel.c": "int win(void) { return 0; }\n"},
                             "upstream: rewrite the kernel")
        self.assertEqual(1, self._check(inc))

    def test_anchor_removal_alone_is_a_collision(self):
        """A tracked fork can keep the file OUT of our protected-path list and still delete our
        branch. The entry below deliberately protects an unrelated path, so only the anchor half
        of the guard can catch this - which is the whole reason the anchor half exists."""
        self._write_manifest(self._entry(protected_paths=["README.md"]))
        inc = self._incoming({"ggml/kernel.c": "int win(void) { return 0; }\n"},
                             "fork: drop the fast path", start="main")
        self.assertEqual(1, self._check(inc))

    def test_a_pure_RENAME_of_a_protected_file_is_REFUSED(self):
        """CHECKER-R3 finding 1, 2026-09-04, reproduced here as a regression.

        A 100%-similarity rename was the one mutation of a protected file that PASSED the
        fail-closed preflight. `git diff --name-only` reports only the DESTINATION path, so the
        protected path never entered `changed`; and the unified body is `similarity index 100% /
        rename from / rename to` with no content lines, so no anchor appeared as removed either.
        Both halves of the guard went blind at once, on the exact move that takes a win out from
        under its manifest entry while leaving the code intact.

        `--no-renames` on both git calls turns the rename back into delete+add, which restores the
        path hit AND the `-` anchor lines. This test fails on the old code."""
        git(self.r.path, "checkout", "-q", "-b", "incoming", "main")
        git(self.r.path, "mv", "ggml/kernel.c", "ggml/kernel_renamed.c")
        git(self.r.path, "commit", "-q", "-m", "fork: rename the kernel file")
        inc = git(self.r.path, "rev-parse", "HEAD")
        git(self.r.path, "checkout", "-q", "main")
        self.assertEqual(1, self._check(inc))

    def test_renaming_an_UNPROTECTED_file_still_passes(self):
        """The must-not-fire half: `--no-renames` widens what the diff shows, so the guard has to
        stay quiet on a rename that touches nothing of ours. Otherwise every upstream file move
        would refuse the ingest and the gate gets switched off."""
        self.r.commit("chore: an unprotected file", {"docs/notes.md": "text\n"},
                      effect="documentation only")
        git(self.r.path, "checkout", "-q", "-b", "incoming", "main")
        git(self.r.path, "mv", "docs/notes.md", "docs/notes-renamed.md")
        git(self.r.path, "commit", "-q", "-m", "upstream: move the notes")
        inc = git(self.r.path, "rev-parse", "HEAD")
        git(self.r.path, "checkout", "-q", "main")
        self.assertEqual(0, self._check(inc))

    def test_incomplete_active_entry_fails_closed(self):
        self._write_manifest(self._entry(quality_gate=""))
        problems = A.validate_protected_wins(self.r.path, "main")
        self.assertTrue(any("INCOMPLETE" in p for p in problems), problems)
        self.assertEqual(1, self._check(self._incoming({"docs/x.md": "unrelated\n"})))

    def test_missing_manifest_fails_closed(self):
        inc = self._incoming({"docs/x.md": "unrelated\n"})
        # Deleted AFTER the branch dance, not before: `_incoming` checks out `incoming` and back,
        # and a checkout restores a TRACKED file that was only removed from the worktree - so the
        # old ordering silently tested a repository whose manifest was still there.
        os.remove(self.manifest_path)
        self.assertEqual(2, self._check(inc))

    def test_stale_anchor_fails_validation(self):
        """A win whose anchor no longer exists is guarding nothing, and must say so."""
        self._write_manifest(self._entry(anchors=["ANCHOR_THAT_WAS_RENAMED"]))
        problems = A.validate_protected_wins(self.r.path, "main")
        self.assertTrue(any("anchor" in p for p in problems), problems)

    def test_a_protected_SUBTREE_is_actually_guarded(self):
        """R6 finding 1, 2026-09-05. A directory `protected_path` validated but guarded NOTHING.

        `git cat-file -e <ref>:<dir>` succeeds on a tree, so `validate` reported the entry healthy.
        `protected_collisions` then intersected the path SET against `git diff --name-only`, which
        only ever emits FILES, so a registered subtree could never produce a hit. Registered path,
        zero guard, green gate - the exact silent overwrite the mandate is about. This test fails
        on the old code."""
        self._write_manifest(self._entry(protected_paths=["ggml"]))
        self.assertEqual([], A.validate_protected_wins(self.r.path, "main"))
        inc = self._incoming({"ggml/other.c": "int upstream(void) { return 1; }\n"},
                             "upstream: land a file inside our protected subtree")
        self.assertEqual(1, self._check(inc))

    def test_a_path_that_merely_SHARES_A_PREFIX_still_passes(self):
        """The must-not-fire half of the subtree fix: prefix matching is on the DIRECTORY boundary,
        not on the raw string, so a sibling that happens to start with a protected file's name is
        not a collision. Without this, `ggml/kernel.c` would swallow `ggml/kernel.c.bak`."""
        inc = self._incoming({"ggml/kernel.c.bak": "backup\n"}, "upstream: an unrelated sibling")
        self.assertEqual(0, self._check(inc))

    def test_an_anchor_that_survives_only_in_patches_series_is_STALE(self):
        """R6 finding 2, 2026-09-05. `_anchor_present` grepped the WHOLE tree, and this fork bakes
        every mechanism into `patches/series/*.patch` by design. So after a fork ingest deleted our
        branch from the source file, the anchor was still found - in our own generated patch - and
        `validate` stayed GREEN over a manifest that no longer guarded live code.

        The series is a RECORD of the mechanism, never the mechanism itself, so it is excluded from
        the anchor search. This test fails on the old code."""
        self.r.commit("regen: bank the series",
                      {"patches/series/0001-our-win.patch":
                       "+int win(void) { return ARIFI_FAST_PATH; }\n"},
                      effect="generated artifacts only")
        self.r.commit("fork: drop the fast path",
                      {"ggml/kernel.c": "int win(void) { return 0; }\n"},
                      effect="correctness only")
        problems = A.validate_protected_wins(self.r.path, "main")
        self.assertTrue(any("anchor" in p for p in problems), problems)

    def test_an_anchor_that_survives_only_in_the_COMMITTED_MANIFEST_is_STALE(self):
        """R6B finding, 2026-09-05. The real fork shape the series test could not reach.

        `_write_manifest` writes `protected-wins.json` to disk and never `git add`s it, so in the
        fixture the manifest was UNTRACKED and `git grep <ref>` could not see it. The real fork
        always has it committed - and it lists every anchor literal by construction. So the
        whole-tree grep had a guaranteed hit for every anchor that was not source, and `validate`
        could never report an anchor absent. Deleting the mechanism from live code left the gate
        GREEN: a record of the win satisfying a check meant for the win.

        This commits the manifest first, exactly as the fork carries it, then deletes the anchor
        from the source file while KEEPING the file - so only the anchor half of validation can
        fire, not the protected-path half. This test fails with `:(exclude)tools/arifi-sync`
        removed from `_anchor_present`, and passes with it."""
        # `commit_json` banked the manifest in setUp (lane-229: the validator refuses an on-disk
        # manifest that is not the one at the ref, so every fixture carries it committed now).
        # The explicit add/commit that used to live here would fail on a clean tree.
        # The manifest is now tracked and carries the anchor literal, same as the fork. Asserted,
        # because a fixture that quietly failed to reproduce that shape would make this test pass
        # for the wrong reason - which is exactly how the untracked-manifest blind spot survived.
        tracked = git(self.r.path, "grep", "-l", "-F", "ARIFI_FAST_PATH", "main")
        self.assertIn("tools/arifi-sync/protected-wins.json", tracked,
                      "fixture did not reproduce the real shape: manifest not tracked")

        self.r.commit("fork: drop the fast path but keep the file",
                      {"ggml/kernel.c": "int win(void) { return 0; }\n"},
                      effect="correctness only")
        problems = A.validate_protected_wins(self.r.path, "main")
        self.assertTrue(any("anchor" in p and "ARIFI_FAST_PATH" in p for p in problems), problems)

    def test_evidence_backed_resolution_unblocks_the_collision(self):
        inc = self._incoming({"ggml/kernel.c": "int win(void) { return 0; }\n"},
                             "upstream: rewrite the kernel")
        self._write_resolutions({
            "win_id": "test-win", "incoming": inc, "verdict": "adopt-union",
            "arms": {"upstream": "2100 us", "ours": "2224 us", "union": "1980 us"},
            "quality": "byte-identical on all three arms",
            "evidence": ["commit:" + inc], "decided_utc": "2026-09-04T21:00:00Z",
            "decided_by": "HQ",
        })
        self.assertEqual(0, self._check(inc))

    def test_a_resolution_missing_an_arm_does_NOT_unblock(self):
        """Three arms or a written reason. 'We benched theirs and it was faster' is the exact
        blind comparison the President ruled against on 2026-09-02."""
        inc = self._incoming({"ggml/kernel.c": "int win(void) { return 0; }\n"},
                             "upstream: rewrite the kernel")
        self._write_resolutions({
            "win_id": "test-win", "incoming": inc, "verdict": "adopt-upstream",
            "arms": {"upstream": "2100 us", "ours": "2224 us"},
            "quality": "ok", "evidence": ["commit:" + inc],
            "decided_utc": "2026-09-04T21:00:00Z", "decided_by": "HQ",
        })
        self.assertEqual(1, self._check(inc))

    def test_a_resolution_for_a_DIFFERENT_incoming_does_NOT_unblock(self):
        inc = self._incoming({"ggml/kernel.c": "int win(void) { return 0; }\n"},
                             "upstream: rewrite the kernel")
        self._write_resolutions({
            "win_id": "test-win", "incoming": self.r.base, "verdict": "keep-ours",
            "arms": {"upstream": "a", "ours": "b", "union": "c"}, "quality": "ok",
            "evidence": ["commit:" + self.r.base], "decided_utc": "2026-09-04T21:00:00Z",
            "decided_by": "HQ",
        })
        self.assertEqual(1, self._check(inc))


class StrictRegistrationTest(unittest.TestCase):
    """WI-1688: a landed measured win that nobody registered must REFUSE the normal update path.

    `R2-CLOSURE-REPORT.md` section 6 named this as the honest weak point of the previous pass:
    the manifest could be complete and still true while a measured win landed beside it,
    completely unguarded, because nothing forced a lane to register what it landed. The
    enforcement lives inside `validate_protected_wins`, which `check` calls and which `bump` leg
    1b calls through `check` - so there is one predicate and no parallel updater.

    The cases below are the fail-closed half plus, deliberately, the two ways this gate could be
    WORSE than no gate: refusing a registered win (it would be switched off within a week), and
    firing on a documentation commit (the same, with extra noise)."""

    def setUp(self):
        self.r = Repo()
        self.win_sha = self.r.commit("vulkan: our measured win", dict(PATHS))
        self.manifest = os.path.join(self.r.path, "tools", "arifi-sync", "protected-wins.json")
        commit_json(self.r.path, "tools/arifi-sync/protected-wins.json", {"version": 1, "wins": [{
            "id": "test-win", "status": "active", "mechanism": "the fast path",
            "commit": self.win_sha, "protected_paths": ["ggml/kernel.c"],
            "anchors": ["ARIFI_FAST_PATH"], "evidence": ["commit:" + self.win_sha],
            "quality_gate": "byte-identical output", "workload": "the seat serve line",
            "measured_effect": "-12% per dispatch"}]})

    def tearDown(self):
        self.r.close()

    def _baseline(self, sha, disposition="NOT A MECHANISM - fixture", tip="__main__"):
        """`tip` defaults to whatever `main` is RIGHT NOW, which is what a genuine baseline looks
        like: it was recorded at a tip, and every row it dispositions predates that tip. Pass an
        explicit tip (or `None`) to model the tampering shapes."""
        if tip == "__main__":
            tip = git(self.r.path, "rev-parse", "main").strip()
        doc = {"version": 1, "commits": [
            {"sha": sha, "subject": "fixture", "disposition": disposition}]}
        if tip is not None:
            doc["tip_when_recorded"] = tip
        commit_json(self.r.path, "tools/arifi-sync/protected-win-baseline.json", doc)

    def _validate(self):
        return A.validate_protected_wins(self.r.path, "main", self.r.cfg)

    def _check(self):
        """An incoming change that touches nothing protected. Without strict registration this
        preflight PASSES, so any refusal here can only come from the new gate."""
        git(self.r.path, "checkout", "-q", "-b", "incoming", self.r.base)
        inc = self.r.commit("upstream: unrelated", {"docs/x.md": "unrelated\n"})
        git(self.r.path, "checkout", "-q", "main")
        args = type("A", (), {"ref": "main", "incoming": inc})()
        return A.cmd_protected_win_check(self.r.path, self.r.cfg, args)

    # -- the gate bites -------------------------------------------------------
    def test_an_unregistered_measured_win_fails_validate(self):
        landed = self.r.commit("vulkan: a SECOND measured win nobody registered",
                               {"ggml/other.c": "int f(void){return 1;}\n"},
                               effect="-9.4% per dispatch, 8 launches")
        problems = self._validate()
        self.assertTrue(any("UNREGISTERED" in p and landed[:9] in p for p in problems), problems)

    def test_an_unregistered_measured_win_REFUSES_the_preflight(self):
        self.r.commit("vulkan: a SECOND measured win nobody registered",
                      {"ggml/other.c": "int f(void){return 1;}\n"},
                      effect="-9.4% per dispatch, 8 launches")
        self.assertEqual(1, self._check())

    def test_an_unregistered_measured_win_REFUSES_bump_before_it_replays(self):
        """Leg 1b must refuse BEFORE the series is replayed. Asserting the return code alone
        would pass even if the refusal happened after a replay had already run, so `_replay` is
        replaced with something that fails the test loudly if it is ever reached."""
        self.r.commit("vulkan: a SECOND measured win nobody registered",
                      {"ggml/other.c": "int f(void){return 1;}\n"},
                      effect="-9.4% per dispatch, 8 launches")
        git(self.r.path, "checkout", "-q", "-b", "onto", self.r.base)
        self.r.commit("upstream: unrelated", {"docs/x.md": "unrelated\n"})
        git(self.r.path, "checkout", "-q", "main")

        def boom(*a, **k):
            raise AssertionError("bump replayed the series despite an unregistered measured win")

        real_replay, real_build = A._replay, A.cmd_build
        A._replay, A.cmd_build = boom, boom
        try:
            args = type("A", (), {"ref": "main", "onto": "onto"})()
            self.assertEqual(1, A.cmd_bump(self.r.path, self.r.cfg, args))
        finally:
            A._replay, A.cmd_build = real_replay, real_build

    # -- the gate does NOT bite where it must not -----------------------------
    def test_a_registered_win_passes(self):
        self.assertEqual([], self._validate())
        self.assertEqual(0, self._check())

    def test_a_baselined_win_passes(self):
        landed = self.r.commit("vulkan: a win with a written disposition",
                               {"ggml/other.c": "int f(void){return 1;}\n"},
                               effect="-9.4% per dispatch")
        self._baseline(landed)
        self.assertEqual([], self._validate())
        self.assertEqual(0, self._check())

    def test_documentation_and_unmeasured_trailers_are_NOT_false_positives(self):
        """The classifier decides what counts as a measured win. If it fired on every docs commit
        the gate would be noise, and noise is how a fail-closed gate gets a skip flag."""
        for effect in ("documentation only", "tooling only - no runtime change",
                       "unmeasured", "n/a", "comment only", "no code change"):
            self.r.commit("docs: %s" % effect, {"docs/%s.md" % abs(hash(effect)): "x\n"},
                          effect=effect)
        self.assertEqual([], self._validate())
        self.assertEqual(0, self._check())

    def test_a_negative_delta_is_a_real_measured_effect(self):
        """Regression for the defect these tests found on 2026-09-04. `NO_EFFECT_PREFIXES` used to
        contain a bare `-` and was tested as a PREFIX, so `Measured-effect: -12.79% per dispatch`
        classified as NO EFFECT - the classifier was blind to the single most common shape a win
        on this fork takes. A dash placeholder is an EXACT match now."""
        self.assertTrue(A._measured_effect_is_real("-12.79% per dispatch, -15.66 ms"))
        self.assertTrue(A._measured_effect_is_real("-9.4%"))
        self.assertFalse(A._measured_effect_is_real("-"))
        self.assertFalse(A._measured_effect_is_real(" -- "))
        self.assertFalse(A._measured_effect_is_real(""))
        self.assertFalse(A._measured_effect_is_real("n/a"))

    def test_a_commit_with_no_trailers_at_all_is_not_a_false_positive(self):
        self.r.commit("chore: no trailers", {"docs/none.md": "x\n"}, trailers=False)
        self.assertEqual([], self._validate())

    # -- D2: a measured quantity cannot be laundered behind a prefix ----------
    #
    # CHECKER-R3 finding 2, 2026-09-04. The classifier was prefix-ONLY, so a real win hidden
    # behind any of the thirty NO_EFFECT_PREFIXES was invisible to discover/validate/check/bump.
    # It is the SAME class as the bare `-` bug the previous pass fixed, on the other thirty
    # prefixes - which is why the fix is an ORDER change (quantity first, prefix as the tiebreak)
    # and not another entry added to a list.
    LAUNDERED = (
        "correctness fix, and -12.79% per dispatch on the ub512 graph",
        "kernel dispatch only; +9.4% prefill",
        "enables the fused path; measured +7% decode",
        "none on CPU, but -18% per dispatch on Vulkan",
        "unmeasured on our seat; on the reporter's box we reproduced +14%",
        "documentation only. Also: -20% per dispatch",
        "no runtime change is claimed, though decode rose 12%",
        "comment only -- and the reorder underneath it is worth -11.4% per dispatch",
    )

    def test_every_laundered_shape_CHECKER_R3_found_is_a_real_measured_effect(self):
        for shape in self.LAUNDERED:
            self.assertTrue(A._measured_effect_is_real(shape),
                            "laundered shape classified NO EFFECT: %r" % shape)

    def test_a_laundered_win_FAILS_validate_and_REFUSES_the_preflight(self):
        """The unit assertion above is not enough on its own: it proves the predicate, not the
        gate. This drives the whole path - a commit lands with the effect stated behind a
        NO_EFFECT prefix, and the preflight must refuse it."""
        landed = self.r.commit("vulkan: a laundered second win",
                               {"ggml/other.c": "int f(void){return 1;}\n"},
                               effect="correctness fix, and -12.79% per dispatch")
        problems = self._validate()
        self.assertTrue(any("UNREGISTERED" in p and landed[:9] in p for p in problems), problems)
        self.assertEqual(1, self._check())

    def test_true_no_effect_values_STILL_classify_as_no_effect(self):
        """The must-not-fire half. A gate that fires on every documentation commit is switched
        off within a week, so the widening is only safe if these stay quiet. Each of these states
        an absence and carries no measured quantity."""
        for shape in ("documentation only",
                      "documentation only; no code, build option or engine behaviour changed",
                      "unmeasured",
                      "unmeasured on this seat; no number was produced",
                      "none",
                      "n/a",
                      "comment only",
                      "no runtime change",
                      "build fix only",
                      "packaging only",
                      "tooling and documentation",
                      "generated artifact refresh: 415 patches",
                      "file relocation only",
                      "process guardrail; 24 rows recorded",
                      "-", "--", "", "tbd"):
            self.assertFalse(A._measured_effect_is_real(shape),
                             "must-not-fire shape classified REAL: %r" % shape)

    def test_a_bare_multiplier_is_configuration_not_effect(self):
        """`x` is the one risky unit: `2x16 GB` sizes a memory carve and `4x tile` names a shape.
        It counts only when signed or decimal, which is how a real speedup is written."""
        self.assertFalse(A._measured_effect_is_real("build variant only: 2x16 GB configuration"))
        self.assertFalse(A._measured_effect_is_real("documentation only; the 4x tile is unchanged"))
        self.assertTrue(A._measured_effect_is_real("comment only, but the path is 3.3x faster"))

    def test_correctness_only_is_no_effect_vocabulary_but_quantity_still_wins(self):
        """R6C: `correctness only` states the same absence as the already-recognised
        `correctness fix`, so it joins the vocabulary. The classifier stays quantity-FIRST, so a
        trailer that carries a number with a throughput unit remains REAL whatever words precede
        it, and the neighbouring vocabulary entries are unchanged."""
        self.assertFalse(A._measured_effect_is_real("correctness only, no throughput claim"))
        self.assertFalse(A._measured_effect_is_real("correctness only"))
        self.assertTrue(A._measured_effect_is_real("correctness only, improved 12%"))
        self.assertFalse(A._measured_effect_is_real("correctness fix"))
        self.assertTrue(A._measured_effect_is_real("correctness fix, and -12.79% per dispatch"))
        self.assertFalse(A._measured_effect_is_real("kernel dispatch only"))

    def test_the_discover_command_sees_the_laundered_win(self):
        """`discover` and `validate` must agree about the set, which is only true because they
        share one predicate. Asserted rather than assumed."""
        landed = self.r.commit("vulkan: a laundered second win",
                               {"ggml/other.c": "int f(void){return 1;}\n"},
                               effect="kernel dispatch only; +9.4% prefill")
        rows = A.unregistered_measured_wins(self.r.path, self.r.cfg, "main")
        self.assertIn(landed, [r[0] for r in rows])


class BaselineBoundaryTest(unittest.TestCase):
    """CHECKER-R3 residual, 2026-09-04: the baseline was closed to new shas BY CONVENTION only.

    Appending one line to `protected-win-baseline.json` silenced the gate for a brand-new win
    forever, and the previous pass disclosed that honestly rather than mechanizing it. Disclosure
    is not a wall. The wall is the ledger's own `tip_when_recorded`: the baseline exists to
    disposition what had ALREADY landed, so every row must be an ancestor of that tip.

    Both directions are pinned, because only the pair is evidence: genuine historical rows must
    keep passing (a gate that bricks real history gets switched off), and a post-baseline sha must
    fail (a gate that lets it through is the hole itself)."""

    def setUp(self):
        self.r = Repo()
        self.win_sha = self.r.commit("vulkan: our measured win", dict(PATHS))
        self.manifest = os.path.join(self.r.path, "tools", "arifi-sync", "protected-wins.json")
        commit_json(self.r.path, "tools/arifi-sync/protected-wins.json", {"version": 1, "wins": [{
            "id": "test-win", "status": "active", "mechanism": "the fast path",
            "commit": self.win_sha, "protected_paths": ["ggml/kernel.c"],
            "anchors": ["ARIFI_FAST_PATH"], "evidence": ["commit:" + self.win_sha],
            "quality_gate": "byte-identical output", "workload": "the seat serve line",
            "measured_effect": "-12% per dispatch"}]})
        self.bpath = os.path.join(self.r.path, "tools", "arifi-sync",
                                  "protected-win-baseline.json")

    def tearDown(self):
        self.r.close()

    def _write(self, rows, tip):
        doc = {"version": 1, "commits": rows}
        if tip is not None:
            doc["tip_when_recorded"] = tip
        commit_json(self.r.path, "tools/arifi-sync/protected-win-baseline.json", doc)

    def _row(self, sha):
        return {"sha": sha, "subject": "fixture", "disposition": "NOT A MECHANISM - fixture"}

    def _validate(self):
        return A.validate_protected_wins(self.r.path, "main", self.r.cfg)

    def test_genuine_historical_rows_pass(self):
        pre = self.r.commit("vulkan: a win that predates the gate",
                            {"ggml/pre.c": "int p(void){return 1;}\n"},
                            effect="-9.4% per dispatch")
        tip = git(self.r.path, "rev-parse", "main")
        self._write([self._row(pre)], tip)
        self.assertEqual([], self._validate())

    def test_a_post_baseline_sha_CANNOT_be_grandfathered_by_appending_it(self):
        pre = self.r.commit("vulkan: a win that predates the gate",
                            {"ggml/pre.c": "int p(void){return 1;}\n"},
                            effect="-9.4% per dispatch")
        tip = git(self.r.path, "rev-parse", "main")
        after = self.r.commit("vulkan: a win that landed AFTER the gate went on",
                              {"ggml/after.c": "int a(void){return 1;}\n"},
                              effect="-18% per dispatch")
        # The tamper: silence the new win by adding its sha to the ledger.
        self._write([self._row(pre), self._row(after)], tip)
        problems = self._validate()
        self.assertTrue(any("NOT an ancestor" in p and after[:9] in p for p in problems), problems)

    def test_deleting_the_boundary_field_is_refused_not_treated_as_no_limit(self):
        pre = self.r.commit("vulkan: a win that predates the gate",
                            {"ggml/pre.c": "int p(void){return 1;}\n"},
                            effect="-9.4% per dispatch")
        self._write([self._row(pre)], None)
        problems = self._validate()
        self.assertTrue(any("tip_when_recorded" in p for p in problems), problems)

    def test_an_unresolvable_boundary_is_refused(self):
        pre = self.r.commit("vulkan: a win that predates the gate",
                            {"ggml/pre.c": "int p(void){return 1;}\n"},
                            effect="-9.4% per dispatch")
        self._write([self._row(pre)], "deadbeefdeadbeefdeadbeefdeadbeefdeadbeef")
        problems = self._validate()
        self.assertTrue(any("does not resolve" in p for p in problems), problems)

    def test_an_empty_baseline_needs_no_boundary(self):
        """A repo that never recorded a baseline must not be forced to invent one."""
        self._write([], None)
        self.assertEqual([], A.baseline_boundary_problems(self.r.path))


class SymbolDefinitionTest(unittest.TestCase):
    """WI-1700, 2026-09-06. The anchor half proves a STRING survives; it cannot see a lost
    DEFINITION. R13's b10819 merge deleted the body of ggml_cuda_moe_cache_mmv_fused from
    ggml-cuda/mmvq.cu while the .cuh declaration and the moe-cache.cu call site both survived, and
    `validate` reported 41/41 anchors healthy over a tree that does not link on CUDA (WI-1699).
    These tests plant exactly that shape and require the `symbols` class to go RED on it."""

    KERNEL = ("#include \"kernel.h\"\n"
              "int win(void) { return ARIFI_FAST_PATH; }\n"
              "int caller(void) { return win() + 1; }\n")
    HEADER = "int win(void);\n"
    # the defect shape: declaration kept, caller kept, BODY gone
    KERNEL_HOLLOW = ("#include \"kernel.h\"\n"
                     "int win(void);\n"
                     "int caller(void) { return win() + 1; }\n"
                     "/* ARIFI_FAST_PATH is still mentioned, so the anchor half stays green */\n")

    def setUp(self):
        self.r = Repo()
        self.win_sha = self.r.commit("cuda: our fused entry point",
                                     {"ggml/kernel.c": self.KERNEL, "ggml/kernel.h": self.HEADER})
        self.manifest = os.path.join(self.r.path, "tools", "arifi-sync", "protected-wins.json")
        self._write_manifest(self._entry())

    def tearDown(self):
        self.r.close()

    def _entry(self, **over):
        e = {
            "id": "sym-win", "status": "active", "mechanism": "the fused entry point",
            "commit": self.win_sha, "protected_paths": ["ggml/kernel.h"],
            "anchors": ["ARIFI_FAST_PATH"], "evidence": ["commit:" + self.win_sha],
            "quality_gate": "links on CUDA", "workload": "n/a", "measured_effect": "-12% per dispatch",
            "symbols": {"ggml/kernel.c": ["win"]},
        }
        e.update(over)
        return e

    def _write_manifest(self, *entries):
        commit_json(self.r.path, "tools/arifi-sync/protected-wins.json",
                    {"version": 1, "wins": list(entries)})

    def test_definition_deleted_while_declaration_and_caller_survive_is_RED_then_GREEN_when_restored(self):
        self.assertEqual([], A.validate_protected_wins(self.r.path, "main"), "GREEN baseline first")
        self.r.commit("upstream: rewrite mmvq, drop our body", {"ggml/kernel.c": self.KERNEL_HOLLOW},
                      effect="unmeasured - merge")
        problems = A.validate_protected_wins(self.r.path, "main")
        self.assertEqual(1, len(problems), problems)
        p = problems[0]
        self.assertIn("'win'", p)
        self.assertIn("NO DEFINITION in ggml/kernel.c", p)
        self.assertIn("ggml/kernel.h:1", p, "the surviving declaration is named")
        self.assertIn(self.win_sha[:7], p, "the last commit that touched the definition is named")
        # the preflight inherits the refusal: check runs validate first
        git(self.r.path, "checkout", "-q", "-b", "incoming", self.r.base)
        self.r.commit("upstream: unrelated", {"docs/x.md": "text\n"})
        git(self.r.path, "checkout", "-q", "main")
        args = type("A", (), {"ref": "main", "incoming": "incoming"})()
        self.assertEqual(1, A.cmd_protected_win_check(self.r.path, self.r.cfg, args))
        # restore the body: GREEN again, and nothing else changed
        self.r.commit("cuda: graft the body back", {"ggml/kernel.c": self.KERNEL},
                      effect="unmeasured - restores a lost definition")
        self.assertEqual([], A.validate_protected_wins(self.r.path, "main"))

    def test_a_call_site_or_a_header_declaration_never_counts_as_a_definition(self):
        text = ("void ggml_cuda_moe_cache_mmv_fused(const void * up, float * dst);\n"
                "static void go(void) {\n"
                "    if (ok) {\n"
                "        ggml_cuda_moe_cache_mmv_fused(up, dst);\n"
                "    }\n"
                "    while (ggml_cuda_moe_cache_mmv_fused_ready(x)) { spin(); }\n"
                "}\n"
                "[[host_name(\"ggml_cuda_moe_cache_mmv_fused\")]]\n")
        self.assertFalse(A._definition_present(text, "ggml_cuda_moe_cache_mmv_fused"))

    def test_a_commented_out_definition_is_not_a_definition(self):
        """WI-1700 check FIX-1: a maker who disables a kernel by commenting it out must go RED."""
        self.assertFalse(A._definition_present(
            "// void win(int x) {\n//     body();\n// }\n", "win"))
        self.assertFalse(A._definition_present(
            "/*\nvoid win(int x) {\n    body();\n}\n*/\n", "win"))
        # a `//` inside a block comment does not swallow the closer
        self.assertFalse(A._definition_present(
            "/* see http://example.org\nvoid win(int x) { } */\n", "win"))
        # a `//` inside a string literal is text, not a comment: the definition after it survives
        self.assertTrue(A._definition_present(
            "const char * url = \"http://x\"; void win(int x) {\n}\n", "win"))
        # a trailing comment on the definition line does not hide the body
        self.assertTrue(A._definition_present(
            "void win(int x) // ArifiLabs fast path\n{\n}\n", "win"))

    def test_the_definition_shapes_this_fork_actually_uses_are_recognised(self):
        # C/CUDA body, parameters across lines, qualifier before the brace
        self.assertTrue(A._definition_present(
            "void ggml_cuda_moe_cache_mmv_fused(\n        const void * up_pool,\n"
            "        cudaStream_t stream) {\n    body();\n}\n", "ggml_cuda_moe_cache_mmv_fused"))
        self.assertTrue(A._definition_present("int f(void) const noexcept {\n}\n", "f"))
        # GLSL / Metal
        self.assertTrue(A._definition_present("void main() {\n}\n", "main"))
        self.assertTrue(A._definition_present(
            "[[host_name(\"kernel_mul_mv_tq3_4s_f32\")]]\nkernel void kernel_mul_mv_tq3_4s_f32(\n"
            "        device const void * src0) {\n}\n", "kernel_mul_mv_tq3_4s_f32"))
        # template helper, and an explicit instantiation line
        self.assertTrue(A._definition_present(
            "template <ggml_type type>\nstatic void helper_t(const void * a) {\n}\n", "helper_t"))
        self.assertTrue(A._definition_present(
            "template void mul_mat_q_case<GGML_TYPE_TQ3_4S>(ggml_backend_cuda_context & ctx);\n",
            "mul_mat_q_case"))
        # K&R / Allman brace on the next line
        self.assertTrue(A._definition_present("int g(int a)\n{\n    return a;\n}\n", "g"))

    def test_a_malformed_symbols_field_is_a_problem_not_a_skip(self):
        self._write_manifest(self._entry(symbols=[]))
        problems = A.validate_protected_wins(self.r.path, "main")
        self.assertTrue(any("`symbols` must be a non-empty object" in p for p in problems), problems)
        self._write_manifest(self._entry(symbols={"ggml/kernel.c": []}))
        problems = A.validate_protected_wins(self.r.path, "main")
        self.assertTrue(any("non-empty list of names" in p for p in problems), problems)
        self._write_manifest(self._entry(symbols={"ggml/gone.c": ["win"]}))
        problems = A.validate_protected_wins(self.r.path, "main")
        self.assertTrue(any("does not exist" in p for p in problems), problems)


class CandidateIdentityTest(unittest.TestCase):
    """lane-229 / HQ-57: the checkout you are STANDING IN is not the ref you named.

    Every manifest and the config are read from the working tree; `--ref` names a git ref whose
    tree is a different object. HQ reproduced the consequence on 2026-09-11: run from the lane-206
    checkout, `protected-win validate --ref arifi/main` read lane-206's 21-entry manifest and
    reported 52 findings about arifi/main, which carries 30 entries. It never read the ref.

    Both directions are pinned here. A guard that only ever refuses would be switched off inside a
    week, so the matching case has to stay green."""

    def setUp(self):
        self.r = Repo()
        self.win_sha = self.r.commit("vulkan: our measured win", dict(PATHS))
        self._manifest("test-win")

    def tearDown(self):
        self.r.close()

    def _entry(self, wid):
        return {"id": wid, "status": "active", "mechanism": "the fast path",
                "commit": self.win_sha, "protected_paths": ["ggml/kernel.c"],
                "anchors": ["ARIFI_FAST_PATH"], "evidence": ["commit:" + self.win_sha],
                "quality_gate": "byte-identical output", "workload": "the seat serve line",
                "measured_effect": "-12% per dispatch"}

    def _manifest(self, *ids):
        commit_json(self.r.path, "tools/arifi-sync/protected-wins.json",
                    {"version": 1, "wins": [self._entry(i) for i in ids]})

    # -- the guard bites ------------------------------------------------------
    def test_a_stale_checkout_manifest_cannot_certify_another_ref(self):
        """The 21-vs-30 shape: this checkout holds one entry, the ref holds two."""
        git(self.r.path, "checkout", "-q", "-b", "newer")
        self._manifest("test-win", "second-win")
        git(self.r.path, "checkout", "-q", "main")          # disk is back to ONE entry
        problems = A.candidate_identity_problems(self.r.path, "newer")
        self.assertTrue(any("protected-wins.json" in p and "DIFFERS" in p for p in problems),
                        problems)
        self.assertTrue(any("checkout: 1 entr" in p and "newer: 2" in p for p in problems),
                        problems)
        with self.assertRaises(A.Loud) as cm:
            A.validate_protected_wins(self.r.path, "newer")
        self.assertIn("WRONG-TREE REFUSAL", str(cm.exception))
        # The refusal has to be actionable, not just loud.
        self.assertIn("--ref HEAD", str(cm.exception))

    def test_an_uncommitted_manifest_edit_cannot_certify_the_ref(self):
        """The quieter half of the same trap: same branch, edited on disk, never committed."""
        self._draft_edit("smuggled-in")
        with self.assertRaises(A.Loud) as cm:
            A.validate_protected_wins(self.r.path, "main")
        # CHECK-UPDATE-ROOT finding 5: the refusal used to print `--ref HEAD`, which refuses for
        # exactly the same reason. It must name the repair that can actually work.
        self.assertIn("validate --draft", str(cm.exception))
        self.assertNotIn("validate --ref HEAD", str(cm.exception))

    def _draft_edit(self, *extra_ids):
        p = os.path.join(self.r.path, "tools", "arifi-sync", "protected-wins.json")
        with open(p, "w", encoding="utf-8", newline="\n") as fh:
            json.dump({"version": 1,
                       "wins": [self._entry(i) for i in ("test-win",) + extra_ids]}, fh)
        return p

    def _validate_cmd(self, **over):
        kw = {"ref": None, "draft": False, "ref_explicit": False}
        kw.update(over)
        return A.cmd_protected_win_validate(self.r.path, self.r.cfg, type("A", (), kw)())

    # -- finding 5: validating a DRAFT before committing it -------------------------------------
    def test_a_draft_snapshot_validates_an_uncommitted_manifest_edit(self):
        """The workflow the identity guard removed and nothing replaced: author an entry, check it,
        THEN commit. `--draft` snapshots the working tree into a throwaway commit and validates
        that, so root binding and content identity both still hold - against the draft itself."""
        self._draft_edit("second-win")
        self.assertEqual(0, self._validate_cmd(draft=True))

    def test_a_draft_that_is_actually_BROKEN_still_fails(self):
        """`--draft` is a way to name the candidate, never a way to soften a gate."""
        p = os.path.join(self.r.path, "tools", "arifi-sync", "protected-wins.json")
        bad = self._entry("second-win")
        bad["anchors"] = ["ANCHOR_THAT_NEVER_EXISTED"]
        with open(p, "w", encoding="utf-8", newline="\n") as fh:
            json.dump({"version": 1, "wins": [self._entry("test-win"), bad]}, fh)
        self.assertEqual(1, self._validate_cmd(draft=True))

    def test_a_draft_COMMITS_NOTHING_and_creates_no_ref(self):
        self._draft_edit("second-win")
        before_status = git(self.r.path, "status", "--porcelain")
        before_refs = git(self.r.path, "for-each-ref", "--format=%(refname)")
        before_head = git(self.r.path, "rev-parse", "HEAD")
        self.assertEqual(0, self._validate_cmd(draft=True))
        self.assertEqual(before_status, git(self.r.path, "status", "--porcelain"))
        self.assertEqual(before_refs, git(self.r.path, "for-each-ref", "--format=%(refname)"))
        self.assertEqual(before_head, git(self.r.path, "rev-parse", "HEAD"))

    def test_a_draft_cannot_be_used_to_certify_another_ref(self):
        """The trap wearing a new flag. A draft is THIS tree; certifying some other ref with it is
        the very thing the identity guard exists to refuse."""
        git(self.r.path, "checkout", "-q", "-b", "newer")
        self._manifest("test-win", "second-win")
        git(self.r.path, "checkout", "-q", "main")
        with self.assertRaises(A.Loud) as cm:
            self._validate_cmd(draft=True, ref="newer", ref_explicit=True)
        self.assertIn("cannot also certify", str(cm.exception))

    def test_the_draft_snapshot_really_carries_the_uncommitted_bytes(self):
        """Asserted, not assumed: a snapshot that silently held HEAD's manifest would make the
        test above pass for the wrong reason - the same way the untracked-manifest blind spot
        survived in the anchor tests."""
        self._draft_edit("second-win")
        snap = A.draft_snapshot(self.r.path)
        blob = git(self.r.path, "show", "%s:tools/arifi-sync/protected-wins.json" % snap)
        self.assertIn("second-win", blob)
        self.assertNotIn("second-win",
                         git(self.r.path, "show", "HEAD:tools/arifi-sync/protected-wins.json"))

    def test_a_divergent_sources_json_is_refused_too(self):
        """The config is the root of it: `main()` DERIVES the repo from the config's directory, so
        a stale `sources.json` chooses the base sha and the ref that everything else is computed
        against."""
        commit_json(self.r.path, "tools/arifi-sync/sources.json",
                    {"base": {"upstream_sha": self.r.base, "ref": "main"}})
        git(self.r.path, "checkout", "-q", "-b", "newer")
        commit_json(self.r.path, "tools/arifi-sync/sources.json",
                    {"base": {"upstream_sha": self.win_sha, "ref": "main"}})
        git(self.r.path, "checkout", "-q", "main")
        self.assertTrue(any("sources.json" in p
                            for p in A.candidate_identity_problems(self.r.path, "newer")))

    def test_an_unresolvable_ref_is_refused_before_anything_is_read(self):
        with self.assertRaises(A.Loud) as cm:
            A.validate_protected_wins(self.r.path, "no/such/ref")
        self.assertIn("does not resolve", str(cm.exception))

    def test_a_manifest_missing_at_BOTH_ends_still_fails_closed(self):
        """The pre-existing fail-closed path is not replaced by the identity guard."""
        r2 = Repo()
        try:
            with self.assertRaises(A.Loud):
                A.load_protected_wins(r2.path)
        finally:
            r2.close()

    # -- the guard must NOT bite ---------------------------------------------
    def test_a_matching_checkout_is_not_refused(self):
        self.assertEqual([], A.candidate_identity_problems(self.r.path, "main"))
        self.assertEqual([], A.validate_protected_wins(self.r.path, "main"))

    def test_line_endings_and_a_BOM_are_not_a_divergence(self):
        """Compared as parsed JSON on purpose. A Windows checkout may hold CRLF or a BOM that the
        blob does not, and a byte compare would refuse a legitimate invocation over whitespace."""
        p = os.path.join(self.r.path, "tools", "arifi-sync", "protected-wins.json")
        with open(p, "rb") as fh:
            body = fh.read().replace(b"\n", b"\r\n")
        with open(p, "wb") as fh:
            fh.write(b"\xef\xbb\xbf" + body)
        self.assertEqual([], A.candidate_identity_problems(self.r.path, "main"))


CACHE_HEAD = ("# CMake cache fixture\n"
              "CMAKE_HOME_DIRECTORY:INTERNAL=%s\n"
              "LLAMA_VULKAN:BOOL=ON\n"
              "CMAKE_BUILD_TYPE:STRING=Release\n")


class BuildJudgeIdentityTest(unittest.TestCase):
    """The binaries in a build dir used to have no recorded source, and then no recorded BYTES.
    `build` writes the identity AND the artifact hashes it produced; `judge` refuses a binary whose
    receipt is missing, describes a different tree, or does not match the bytes on disk.

    Nothing here executes llama-server, runs cmake or measures anything: the 'binary' is a small
    file and every case stops at a refusal or at the bench-purity gate, both of which are decided
    before a process is launched."""

    def setUp(self):
        self.r = Repo()
        self.bdir = os.path.join(self.r.path, "build-vulkan")
        self.bindir = os.path.join(self.bdir, "bin")
        os.makedirs(self.bindir)
        self.cache = os.path.join(self.bdir, "CMakeCache.txt")
        with open(self.cache, "w", encoding="utf-8") as fh:
            fh.write(CACHE_HEAD % self.r.path.replace("\\", "/"))
        self.binary = os.path.join(self.bindir, "llama-server.exe")
        self._put(self.binary, "SERVER-V1")
        self.lib = os.path.join(self.bindir, "ggml-vulkan.dll")
        self._put(self.lib, "LIB-V1")
        self.model = os.path.join(self.r.path, "model.gguf")
        open(self.model, "w").close()
        self.cfg = dict(self.r.cfg)
        self.cfg["build"] = {"build_dir": "build-vulkan", "targets": ["llama-server"],
                             "cache_snapshot": "tools/arifi-sync/recipe/snap.txt",
                             "live_cache": "build-vulkan/CMakeCache.txt"}
        self.cfg["judge"] = {"binary": "llama-server", "model": self.model,
                             "args": ["--no-host"], "port": 8199}
        self.args = type("A", (), {"i_have_read_bench_purity": False})()

    def tearDown(self):
        self.r.close()

    @staticmethod
    def _put(path, text):
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)

    def _receipt(self, doc=None, **over):
        """A receipt shaped exactly as `cmd_build` publishes one: source identity, canonical
        roots, and the artifact hashes. Anything less is now refused by name, which is the point."""
        if doc is None:
            doc = A._tree_identity(self.r.path, self.bdir)
            doc.update({"built_utc": "2026-09-11T00:00:00+00:00",
                        "build_dir": "build-vulkan",
                        "build_dir_abs": A._canon(self.bdir),
                        "cmake_home": A._canon(self.r.path),
                        "config": A.BUILD_CONFIG,
                        "judge_binary": A.artifact_key(self.bdir, self.binary),
                        "artifacts": A.artifact_digest(self.bdir, self.binary)})
        doc.update(over)
        with open(os.path.join(self.bdir, A.BUILD_RECEIPT), "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        return doc

    def _judge(self):
        return A.cmd_judge(self.r.path, self.cfg, self.args)

    def test_judge_refuses_a_binary_with_no_receipt(self):
        with self.assertRaises(A.Loud) as cm:
            self._judge()
        self.assertIn("no build-identity receipt", str(cm.exception))
        self.assertIn("arifi_sync.py build", str(cm.exception))

    def test_judge_refuses_a_binary_built_from_another_tree(self):
        self._receipt()
        self.r.commit("cuda: land something after the build",
                      {"ggml/after.c": "int a(void){return 1;}\n"}, effect="correctness only")
        with self.assertRaises(A.Loud) as cm:
            self._judge()
        self.assertIn("WRONG-TREE REFUSAL", str(cm.exception))

    def test_judge_refuses_when_the_tree_went_dirty_after_the_build(self):
        self._receipt()
        self.r.write("ggml/kernel.c", "int win(void) { return 2; }\n")
        with self.assertRaises(A.Loud):
            self._judge()

    def test_a_matching_receipt_reaches_the_bench_purity_gate(self):
        """The must-not-bite half. With the identity intact, judge proceeds to exactly where it
        always stopped - the bench-purity confirmation - and still launches nothing."""
        self._receipt()
        self.assertEqual(1, self._judge())

    # -- CHECK-UPDATE-ROOT finding 1: the bytes, not the listing -------------------------------
    def test_dirty_bytes_CHANGING_changes_the_identity(self):
        """`DIRTY_BYTES_CHANGED_IDENTITY_MATCHES True` was the review's reproduction: HEAD, the
        HEAD tree and the `status` LISTING are all unchanged when an already-modified file goes
        from v1 to v2, so the old identity certified a binary of the wrong source."""
        self.r.write("ggml/kernel.c", "int win(void) { return 1; }\n")
        first = A._tree_identity(self.r.path, self.bdir)
        self.r.write("ggml/kernel.c", "int win(void) { return 2; }\n")
        second = A._tree_identity(self.r.path, self.bdir)
        self.assertEqual(first["head"], second["head"])
        self.assertEqual(first["tree"], second["tree"])
        self.assertEqual(first["status_sha256"], second["status_sha256"],
                         "the listing is unchanged - which is exactly why it was not enough")
        self.assertNotEqual(first["content_sha256"], second["content_sha256"])
        self.assertFalse(A._identity_matches(first, second))

    def test_judge_refuses_after_a_dirty_file_changed_to_DIFFERENT_dirty_content(self):
        self.r.write("ggml/kernel.c", "int win(void) { return 1; }\n")
        self._receipt()
        self.assertEqual(1, self._judge(), "same dirty bytes must still be judgeable")
        self.r.write("ggml/kernel.c", "int win(void) { return 2; }\n")
        with self.assertRaises(A.Loud) as cm:
            self._judge()
        self.assertIn("WRONG-TREE REFUSAL", str(cm.exception))

    def test_a_staged_rename_does_not_desynchronize_the_content_parse(self):
        """`status --porcelain -z` emits a rename as TWO NUL fields. Consume only the first and
        the next entry is read as the rename's origin, so every path after a rename drops out of
        the digest - a stable hash that stops seeing edits."""
        self.r.commit("feat: two files", {"a.txt": "1\n", "z.txt": "1\n"})
        git(self.r.path, "mv", "a.txt", "b.txt")
        self.r.write("z.txt", "v1\n")
        first = A._tree_identity(self.r.path, self.bdir)
        self.r.write("z.txt", "v2\n")
        second = A._tree_identity(self.r.path, self.bdir)
        self.assertNotEqual(first["content_sha256"], second["content_sha256"])

    # -- CHECK-UPDATE-ROOT finding 3: the artifacts ---------------------------------------------
    def test_judge_refuses_a_REPLACED_binary(self):
        """`CHANGED_BINARY_REACHES_PURITY_GATE True`: the receipt described source only, so a
        hand-replaced llama-server.exe kept the certification of the one it overwrote."""
        self._receipt()
        self._put(self.binary, "SERVER-TAMPERED")
        with self.assertRaises(A.Loud) as cm:
            self._judge()
        self.assertIn("BINARY REFUSAL", str(cm.exception))
        self.assertIn("DIFFERENT BYTES", str(cm.exception))

    def test_judge_refuses_a_replaced_runtime_LIBRARY(self):
        self._receipt()
        self._put(self.lib, "LIB-TAMPERED")
        with self.assertRaises(A.Loud) as cm:
            self._judge()
        self.assertIn("ggml-vulkan.dll", str(cm.exception))

    def test_judge_refuses_a_PARTIAL_build_that_lost_a_recorded_artifact(self):
        self._receipt()
        os.remove(self.lib)
        with self.assertRaises(A.Loud) as cm:
            self._judge()
        self.assertIn("MISSING", str(cm.exception))

    def test_judge_refuses_a_receipt_that_records_no_artifacts_at_all(self):
        """The old receipt shape, and any hand-written one: source metadata with no executable
        bytes certifies nothing about what would run."""
        self._receipt(artifacts={})
        with self.assertRaises(A.Loud) as cm:
            self._judge()
        self.assertIn("NO artifact hashes", str(cm.exception))

    def test_judge_refuses_a_receipt_written_for_another_build_root(self):
        self._receipt(cmake_home=A._canon(os.path.join(self.r.home, "other-checkout")))
        with self.assertRaises(A.Loud) as cm:
            self._judge()
        self.assertIn("cmake_home", str(cm.exception))

    # -- R2 finding 4: the selected binary, and the adjacent runtime set ------------------------
    def test_an_EXTENSIONLESS_server_binary_is_HASHED_and_judgeable(self):
        """`EXTENSIONLESS_SERVER_HASHED False`: `bin/llama-server` is a supported candidate and
        carries no suffix, so the suffix filter dropped it from the receipt. A build that selected
        it produced a receipt with no hash for its own judge binary, which `judge` then refused
        forever - the guard bricked the configuration it was meant to certify."""
        os.remove(self.binary)
        ext = os.path.join(self.bindir, "llama-server")
        self._put(ext, "SERVER-V1")
        self.assertEqual(ext, A._judge_binary(self.bdir))
        self.assertIn("bin/llama-server", A.artifact_digest(self.bdir, ext))
        self.binary = ext
        self._receipt()
        self.assertEqual(1, self._judge(), "the extensionless binary must reach the purity gate")

    def test_a_replaced_EXTENSIONLESS_binary_is_still_caught(self):
        os.remove(self.binary)
        ext = os.path.join(self.bindir, "llama-server")
        self._put(ext, "SERVER-V1")
        self.binary = ext
        self._receipt()
        self._put(ext, "SERVER-TAMPERED")
        with self.assertRaises(A.Loud) as cm:
            self._judge()
        self.assertIn("DIFFERENT BYTES", str(cm.exception))

    def test_an_ADDED_runtime_library_invalidates_the_receipt(self):
        """`NEW_RUNTIME_REACHES_PURITY True`: only recorded paths were checked, so a DLL dropped
        into the judge binary's own directory - which the loader searches first - changed what the
        process would load without changing one recorded byte."""
        self._receipt()
        self._put(os.path.join(self.bindir, "ggml-extra.dll"), "INJECTED")
        with self.assertRaises(A.Loud) as cm:
            self._judge()
        self.assertIn("ADDED runtime library", str(cm.exception))
        self.assertIn("ggml-extra.dll", str(cm.exception))

    def test_an_added_shared_object_is_caught_on_posix_names_too(self):
        self._receipt()
        self._put(os.path.join(self.bindir, "libggml-extra.so.1"), "INJECTED")
        with self.assertRaises(A.Loud) as cm:
            self._judge()
        self.assertIn("ADDED runtime library", str(cm.exception))

    def test_judge_refuses_when_it_resolves_a_DIFFERENT_binary_than_the_build_selected(self):
        """`_judge_binary` walks a fixed candidate list. A build that selected the extensionless
        candidate and a judge run that resolves a newly appeared `.exe` are each internally
        consistent and are about different files, so the selection is recorded and re-checked."""
        os.remove(self.binary)
        ext = os.path.join(self.bindir, "llama-server")
        self._put(ext, "SERVER-V1")
        self.binary = ext
        self._receipt()
        self._put(os.path.join(self.bindir, "llama-server.exe"), "OTHER-SERVER")
        with self.assertRaises(A.Loud) as cm:
            self._judge()
        self.assertIn("judge_binary", str(cm.exception))

    def test_an_UNRECORDED_new_file_in_the_build_dir_is_not_a_refusal(self):
        """The must-not-fire half: building a second target later adds files beside the server.
        Only the RECORDED artifacts have to still be present and unchanged."""
        self._receipt()
        self._put(os.path.join(self.bindir, "llama-cli.exe"), "CLI")
        self.assertEqual(1, self._judge())


class FakeCompiler:
    """Intercepts `cmake` ONLY. Every other subprocess - all of git - runs for real, so nothing
    the guards are about is stubbed away; the hardware execution is."""

    def __init__(self, bindir, rc=0, before_exit=None):
        self.real = subprocess.run
        self.bindir = bindir
        self.rc = rc
        self.before_exit = before_exit
        self.calls = []

    def __call__(self, cmd, *a, **k):
        if cmd and str(cmd[0]) == "cmake":
            self.calls.append(list(cmd))
            if self.before_exit:
                self.before_exit()
            if self.rc == 0:
                os.makedirs(self.bindir, exist_ok=True)
                for name, body in (("llama-server.exe", "SERVER-BUILT"),
                                   ("ggml-vulkan.dll", "LIB-BUILT")):
                    with open(os.path.join(self.bindir, name), "w", encoding="utf-8") as fh:
                        fh.write(body)
            return subprocess.CompletedProcess(cmd, self.rc)
        return self.real(cmd, *a, **k)


class BuildGateTest(unittest.TestCase):
    """CHECK-UPDATE-ROOT findings 2, 3 and 6: what `build` must refuse BEFORE it compiles, and
    what it must never leave behind when it refuses.

    Only the compiler is stubbed. The repository, its status, the CMake cache, the recipe snapshot
    and the receipt file are all real."""

    def setUp(self):
        self.r = Repo()
        self.r.commit("vulkan: our line", dict(PATHS))
        self.bdir = os.path.join(self.r.path, "build-vulkan")
        self.bindir = os.path.join(self.bdir, "bin")
        os.makedirs(self.bdir)
        self.cache = os.path.join(self.bdir, "CMakeCache.txt")
        self._cache(self.r.path)
        self.snap = os.path.join(self.r.path, "tools", "arifi-sync", "recipe", "snap.txt")
        os.makedirs(os.path.dirname(self.snap), exist_ok=True)
        self._snapshot_matches_live()
        self.cfg = dict(self.r.cfg)
        self.cfg["build"] = {"build_dir": "build-vulkan", "targets": ["llama-server"],
                             "cache_snapshot": "tools/arifi-sync/recipe/snap.txt",
                             "live_cache": "build-vulkan/CMakeCache.txt"}
        self.rpath = os.path.join(self.bdir, A.BUILD_RECEIPT)
        self._saved_run = subprocess.run

    def tearDown(self):
        A.subprocess.run = self._saved_run
        self.r.close()

    def _cache(self, home, extra=""):
        with open(self.cache, "w", encoding="utf-8") as fh:
            fh.write(CACHE_HEAD % str(home).replace("\\", "/") + extra)

    def _snapshot_matches_live(self):
        entries = A.parse_cache(self.cache)
        with open(self.snap, "w", encoding="utf-8", newline="\n") as fh:
            for k in sorted(entries):
                fh.write("%s:%s=%s\n" % (k, entries[k][0], entries[k][1]))

    def _stale_receipt(self):
        with open(self.rpath, "w", encoding="utf-8") as fh:
            json.dump({"head": "0" * 40, "tree": "0" * 40, "artifacts": {}}, fh)

    def _build(self, compiler):
        A.subprocess.run = compiler
        return A.cmd_build(self.r.path, self.cfg, type("A", (), {})())

    # -- finding 2: the compiler must be pointed at THIS checkout -------------------------------
    def test_a_cache_naming_another_source_root_refuses_before_the_compiler(self):
        """`WRONG_CMAKE_ROOT_AND_RECIPE_FAILURE_BUILD_RC 0`: `CMAKE_HOME_DIRECTORY` is an INTERNAL
        entry, which `parse_cache` drops, so nothing ever read the source root cmake would build.
        A cache naming another checkout compiled that checkout and wrote THIS one's receipt."""
        self._stale_receipt()
        self._cache(os.path.join(self.r.home, "some", "OTHER", "checkout"))
        self._snapshot_matches_live()
        c = FakeCompiler(self.bindir)
        self.assertEqual(1, self._build(c))
        self.assertEqual([], c.calls, "the compiler ran despite a foreign CMake source root")
        self.assertFalse(os.path.exists(self.rpath), "a refused build left a receipt behind")

    def test_a_build_dir_outside_the_checkout_refuses(self):
        outside = os.path.join(self.r.home, "elsewhere-build")
        os.makedirs(os.path.join(outside, "bin"))
        with open(os.path.join(outside, "CMakeCache.txt"), "w", encoding="utf-8") as fh:
            fh.write(CACHE_HEAD % self.r.path.replace("\\", "/"))
        self.cfg["build"] = dict(self.cfg["build"], build_dir=outside,
                                 live_cache=os.path.join(outside, "CMakeCache.txt"))
        c = FakeCompiler(os.path.join(outside, "bin"))
        self.assertEqual(1, self._build(c))
        self.assertEqual([], c.calls)

    def test_a_cache_with_no_source_root_at_all_refuses(self):
        with open(self.cache, "w", encoding="utf-8") as fh:
            fh.write("LLAMA_VULKAN:BOOL=ON\n")
        self._snapshot_matches_live()
        c = FakeCompiler(self.bindir)
        self.assertEqual(1, self._build(c))
        self.assertEqual([], c.calls)

    # -- finding 6: the recipe's verdict must stop the compiler ---------------------------------
    def test_a_LOST_recipe_entry_stops_before_the_compiler_and_removes_the_receipt(self):
        """`cmd_recipe_diff`'s return code was discarded, so a lost build flag reached the
        compiler and then received a fresh identity receipt blessing it."""
        self._stale_receipt()
        with open(self.snap, "a", encoding="utf-8", newline="\n") as fh:
            fh.write("LLAMA_ARIFI_TURBO_KV:BOOL=ON\n")   # recorded, absent from the live cache
        c = FakeCompiler(self.bindir)
        self.assertEqual(1, self._build(c))
        self.assertEqual([], c.calls, "the compiler ran despite a lost recipe entry")
        self.assertFalse(os.path.exists(self.rpath))

    # -- finding 3: receipts and failed / mid-flight builds -------------------------------------
    def test_a_FAILED_build_removes_the_previous_receipt(self):
        """`FAILED_BUILD_RC 1 OLD_RECEIPT_SURVIVES True`: the old receipt was left untouched, so a
        half-updated build directory kept certification from the build before it."""
        self._stale_receipt()
        c = FakeCompiler(self.bindir, rc=1)
        self.assertEqual(1, self._build(c))
        self.assertEqual(1, len(c.calls))
        self.assertFalse(os.path.exists(self.rpath))

    def test_an_edit_DURING_the_build_is_not_certified(self):
        def edit():
            self.r.write("ggml/kernel.c", "int win(void) { return 999; }\n")
        c = FakeCompiler(self.bindir, before_exit=edit)
        self.assertEqual(1, self._build(c))
        self.assertEqual(1, len(c.calls), "the compiler should have run - the edit races it")
        self.assertFalse(os.path.exists(self.rpath))

    def test_a_build_that_produces_no_server_gets_no_receipt(self):
        c = FakeCompiler(self.bindir)
        c.rc = 0
        A.subprocess.run = lambda cmd, *a, **k: (
            subprocess.CompletedProcess(cmd, 0) if cmd and str(cmd[0]) == "cmake"
            else self._saved_run(cmd, *a, **k))
        self.assertEqual(1, A.cmd_build(self.r.path, self.cfg, type("A", (), {})()))
        self.assertFalse(os.path.exists(self.rpath))

    def test_a_successful_build_records_the_artifact_bytes_and_judge_accepts_them(self):
        """The must-not-bite half, end to end: build writes a receipt carrying the produced bytes,
        and judge verifies them and proceeds to the bench-purity gate, launching nothing."""
        c = FakeCompiler(self.bindir)
        self.assertEqual(0, self._build(c))
        A.subprocess.run = self._saved_run
        with open(self.rpath, encoding="utf-8") as fh:
            receipt = json.load(fh)
        self.assertIn("bin/llama-server.exe", receipt["artifacts"])
        self.assertIn("bin/ggml-vulkan.dll", receipt["artifacts"])
        self.assertEqual(A._canon(self.r.path), receipt["cmake_home"])
        self.assertEqual(A._canon(self.bdir), receipt["build_dir_abs"])
        # OUTSIDE the checkout: a model dropped into the repo is an untracked file, i.e. a source
        # change after the build, and judge would refuse it - correctly.
        model = os.path.join(self.r.home, "model.gguf")
        open(model, "w").close()
        cfg = dict(self.cfg)
        cfg["judge"] = {"binary": "llama-server", "model": model, "args": ["--no-host"],
                        "port": 8199}
        self.assertEqual(1, A.cmd_judge(self.r.path, cfg,
                                        type("A", (), {"i_have_read_bench_purity": False})()))

    def test_a_rebuild_that_fails_after_a_GOOD_build_leaves_no_certification(self):
        """The sequence the review reproduced: build green, then a failing rebuild. The receipt
        for the binaries that are now half-overwritten must be gone."""
        self.assertEqual(0, self._build(FakeCompiler(self.bindir)))
        self.assertTrue(os.path.exists(self.rpath))
        self.assertEqual(1, self._build(FakeCompiler(self.bindir, rc=1)))
        self.assertFalse(os.path.exists(self.rpath))

    # -- R2 finding 1: an in-source build switched the dirty-source guard off -------------------
    def test_an_IN_SOURCE_build_directory_is_REFUSED_before_the_compiler(self):
        """`IN_SOURCE_ROOT_PROBLEMS []` was the review's reproduction. `build_dir: "."` is `_under`
        the checkout and its cache names the right source root, so every root check passed - and
        then the identity excluded the entire tree as "the build directory", reporting
        `dirty: False` over edited sources."""
        self.cfg["build"] = dict(self.cfg["build"], build_dir=".",
                                 live_cache="CMakeCache.txt")
        shutil.copy(self.cache, os.path.join(self.r.path, "CMakeCache.txt"))
        problems = A.build_root_problems(self.r.path, self.cfg)
        self.assertTrue(any("IS the checkout" in p for p in problems), problems)
        c = FakeCompiler(self.bindir)
        self.assertEqual(1, self._build(c))
        self.assertEqual([], c.calls, "the compiler ran on an in-source build directory")

    def test_excluding_the_checkout_from_the_identity_is_REFUSED_at_the_point_of_use(self):
        """The other half: `cmd_judge` samples the identity before it reaches the root check, so
        the refusal also lives where the exclusion is applied. Without it a dirty source file is
        simply invisible."""
        self.r.write("ggml/kernel.c", "int win(void) { return 999; }\n")
        self.assertTrue(A._tree_identity(self.r.path, self.bdir)["dirty"],
                        "a real build dir must not hide a dirty source file")
        with self.assertRaises(A.Loud) as cm:
            A._tree_identity(self.r.path, self.r.path)
        self.assertIn("not a strict descendant", str(cm.exception))

    def test_a_build_directory_holding_TRACKED_files_is_refused(self):
        """A build directory is excluded from the identity, so one carrying source removes that
        source from the identity just as an in-source build would."""
        self.r.commit("feat: source that lives under the build dir",
                      {"build-vulkan/src.c": "int s(void){return 1;}\n"}, effect="tooling only")
        problems = A.build_root_problems(self.r.path, self.cfg)
        self.assertTrue(any("TRACKED file" in p for p in problems), problems)
        c = FakeCompiler(self.bindir)
        self.assertEqual(1, self._build(c))
        self.assertEqual([], c.calls)

    def test_the_ACCEPTED_configuration_builds_and_the_REJECTED_one_does_not(self):
        """Both arms of the same fixture, end to end: a strict subdirectory builds green and
        publishes a receipt; the same repository with `build_dir: "."` compiles nothing."""
        c = FakeCompiler(self.bindir)
        self.assertEqual(0, self._build(c))
        self.assertEqual(1, len(c.calls))
        self.assertTrue(os.path.exists(self.rpath))

        shutil.copy(self.cache, os.path.join(self.r.path, "CMakeCache.txt"))
        self.cfg["build"] = dict(self.cfg["build"], build_dir=".", live_cache="CMakeCache.txt")
        c2 = FakeCompiler(self.bindir)
        self.assertEqual(1, self._build(c2))
        self.assertEqual([], c2.calls)

    # -- R2 finding 2: the recipe could certify a cache the compiler never reads -----------------
    def test_a_DECOY_live_cache_refuses_before_the_compiler_and_leaves_no_receipt(self):
        """The review's fixture, reproduced: the recorded recipe requires a flag, a DECOY cache at
        `build.live_cache` carries it, and the cache in the build directory - the one cmake is
        handed - does not. `DECOY_RECIPE_BUILD_RC 0 COMPILER_CALLS 1 RECEIPT True` was the result;
        the recipe diff compared the decoy and passed."""
        self._stale_receipt()
        decoy_dir = os.path.join(self.r.path, "decoy")
        os.makedirs(decoy_dir, exist_ok=True)
        decoy = os.path.join(decoy_dir, "CMakeCache.txt")
        with open(decoy, "w", encoding="utf-8") as fh:
            fh.write(CACHE_HEAD % self.r.path.replace("\\", "/") + "REQUIRED_FLAG:BOOL=ON\n")
        # the recorded recipe matches the DECOY, not the real build cache
        entries = A.parse_cache(decoy)
        with open(self.snap, "w", encoding="utf-8", newline="\n") as fh:
            for k in sorted(entries):
                fh.write("%s:%s=%s\n" % (k, entries[k][0], entries[k][1]))
        self.assertNotIn("REQUIRED_FLAG", A.parse_cache(self.cache))

        self.cfg["build"] = dict(self.cfg["build"], live_cache="decoy/CMakeCache.txt")
        c = FakeCompiler(self.bindir)
        self.assertEqual(1, self._build(c))
        self.assertEqual([], c.calls, "the compiler ran against a cache nobody compared")
        self.assertFalse(os.path.exists(self.rpath))

    def test_the_recipe_is_compared_against_the_cache_the_compiler_is_handed(self):
        """Even with `live_cache` correctly configured, the comparison names the canonical cache
        explicitly rather than resolving a second config key: a flag lost from THAT file stops the
        build."""
        with open(self.cache, "a", encoding="utf-8") as fh:
            fh.write("EXTRA_FLAG:BOOL=ON\n")
        self._snapshot_matches_live()
        with open(self.cache, "w", encoding="utf-8") as fh:      # the flag is lost again
            fh.write(CACHE_HEAD % self.r.path.replace("\\", "/"))
        c = FakeCompiler(self.bindir)
        self.assertEqual(1, self._build(c))
        self.assertEqual([], c.calls)

    # -- R2 finding 3: a refusal that left the receipt behind ------------------------------------
    def test_a_MISSING_cmake_cache_ALSO_drops_the_previous_receipt(self):
        """The residual the review named: this refusal raised before reaching the drop, so a
        receipt survived a build directory that is no longer configured at all."""
        self._stale_receipt()
        os.remove(self.cache)
        c = FakeCompiler(self.bindir)
        self.assertEqual(1, self._build(c))
        self.assertEqual([], c.calls)
        self.assertFalse(os.path.exists(self.rpath), "a refused build left a receipt behind")

    def test_the_missing_cache_message_still_names_the_repair(self):
        """A build directory with no cache at all reports what to do about it. The refusal now
        arrives through the root check (an absent cache states no `CMAKE_HOME_DIRECTORY`), and the
        receipt is dropped on that path too - which is the property the review asked for."""
        self._stale_receipt()
        os.remove(self.cache)
        import contextlib
        import io as _io
        buf = _io.StringIO()
        with contextlib.redirect_stdout(buf):
            self._build(FakeCompiler(self.bindir))
        self.assertIn("Reconfigure the build dir from the recorded recipe", buf.getvalue())
        self.assertFalse(os.path.exists(self.rpath))


def _object_exists(repo, sha):
    r = subprocess.run(["git", "-C", repo, "cat-file", "-e", sha],
                       capture_output=True, text=True)
    return r.returncode == 0


class DraftSnapshotSafetyTest(unittest.TestCase):
    """CHECK-UPDATE-ROOT-R2 finding 7: `--draft` promised to publish nothing and then wrote the
    bytes of every untracked file in the checkout into `.git/objects` via `git add -A`.

    A dangling commit is unreachable, not absent. `DRAFT_SILENT_UNTRACKED_SECRET True OUTPUT ''`
    was the review's reproduction: private content stored, with no output at all. The default is a
    refusal now, taken before any object is written, and a genuinely new candidate file is included
    only when it is named."""

    SECRET = "BEGIN PRIVATE KEY\nnot-part-of-any-candidate\nEND PRIVATE KEY\n"

    def setUp(self):
        self.r = Repo()
        self.r.commit("feat: tracked source", dict(PATHS), effect="tooling only")

    def tearDown(self):
        self.r.close()

    def test_an_untracked_SENSITIVE_file_refuses_and_is_never_written_to_the_object_store(self):
        self.r.write("private/secret.key", self.SECRET)
        sha = git(self.r.path, "hash-object", "private/secret.key")   # computed, NOT written
        self.assertFalse(_object_exists(self.r.path, sha), "fixture precondition")
        with self.assertRaises(A.Loud) as cm:
            A.draft_snapshot(self.r.path)
        self.assertIn("DRAFT REFUSED", str(cm.exception))
        self.assertIn("private/secret.key", str(cm.exception))
        self.assertFalse(_object_exists(self.r.path, sha),
                         "the secret's bytes were written into .git/objects anyway")

    def test_an_IGNORED_file_is_not_a_refusal_and_stays_out_of_the_snapshot(self):
        self.r.commit(".gitignore", {".gitignore": "build-vulkan/\n"}, effect="tooling only")
        self.r.write("build-vulkan/blob.bin", "generated output\n")
        sha = git(self.r.path, "hash-object", "build-vulkan/blob.bin")
        snap = A.draft_snapshot(self.r.path)
        self.assertNotIn("build-vulkan", git(self.r.path, "ls-tree", "-r", "--name-only", snap))
        self.assertFalse(_object_exists(self.r.path, sha))

    def test_an_explicitly_ALLOWED_new_file_is_included_and_nothing_else_is(self):
        self.r.write("tools/arifi-sync/protected-wins.json", '{"version": 1, "wins": []}\n')
        snap = A.draft_snapshot(self.r.path, ["tools/arifi-sync/protected-wins.json"])
        listed = git(self.r.path, "ls-tree", "-r", "--name-only", snap).split()
        self.assertIn("tools/arifi-sync/protected-wins.json", listed)
        self.assertIn("ggml/kernel.c", listed)

    def test_an_allowance_does_not_open_the_door_to_the_REST_of_the_untracked_set(self):
        self.r.write("tools/arifi-sync/protected-wins.json", '{"version": 1, "wins": []}\n')
        self.r.write("private/secret.key", self.SECRET)
        with self.assertRaises(A.Loud) as cm:
            A.draft_snapshot(self.r.path, ["tools/arifi-sync/protected-wins.json"])
        self.assertIn("private/secret.key", str(cm.exception))

    def test_an_OVERSIZE_allowance_is_refused(self):
        self.r.write("big.bin", "x" * (A.DRAFT_MAX_NEW_BYTES + 1))
        with self.assertRaises(A.Loud) as cm:
            A.draft_snapshot(self.r.path, ["big.bin"])
        self.assertIn("too large", str(cm.exception))

    def test_an_allowance_naming_a_TRACKED_file_is_refused(self):
        """An allowance authorises a NEW file. Pointed at something already tracked it is not an
        authorisation at all, and silently accepting it would make the flag look load-bearing when
        it did nothing."""
        with self.assertRaises(A.Loud) as cm:
            A.draft_snapshot(self.r.path, ["ggml/kernel.c"])
        self.assertIn("not untracked", str(cm.exception))

    def test_a_PARTIALLY_STAGED_tree_keeps_its_index_bytes_refs_worktree_and_status(self):
        """The snapshot uses a throwaway `GIT_INDEX_FILE`, so the operator's own staging - the
        half-built commit they are in the middle of - has to come through untouched."""
        self.r.commit("feat: two more files",
                      {"a.txt": "a1\n", "b.txt": "b1\n"}, effect="tooling only")
        self.r.write("a.txt", "a2\n")
        git(self.r.path, "add", "--", "a.txt")        # staged
        self.r.write("b.txt", "b2\n")                 # unstaged
        self.r.write("ggml/kernel.c", "int win(void) { return ARIFI_FAST_PATH + 1; }\n")

        idx = os.path.join(self.r.path, ".git", "index")
        refs_before = git(self.r.path, "for-each-ref", "--format=%(refname) %(objectname)")
        status_before = git(self.r.path, "status", "--porcelain")
        head_before = git(self.r.path, "rev-parse", "HEAD")
        with open(os.path.join(self.r.path, "b.txt"), "rb") as fh:
            worktree_before = fh.read()
        # LAST baseline capture, and it has to stay last. `git status` legitimately rewrites the
        # real index to refresh its stat cache - a racily-clean entry gets its size zeroed - so an
        # index read taken BEFORE the baseline commands compares against bytes git itself changed,
        # for reasons that have nothing to do with the snapshot. Read here and the only writer that
        # could possibly run before the assertion below is `draft_snapshot` itself.
        with open(idx, "rb") as fh:
            index_before = fh.read()

        snap = A.draft_snapshot(self.r.path)

        with open(idx, "rb") as fh:
            self.assertEqual(index_before, fh.read(), "the real index was modified")
        self.assertEqual(refs_before,
                         git(self.r.path, "for-each-ref", "--format=%(refname) %(objectname)"))
        self.assertEqual(status_before, git(self.r.path, "status", "--porcelain"))
        self.assertEqual(head_before, git(self.r.path, "rev-parse", "HEAD"))
        with open(os.path.join(self.r.path, "b.txt"), "rb") as fh:
            self.assertEqual(worktree_before, fh.read())
        # and the snapshot carries the WORKTREE bytes, staged or not - it is the tree in front of
        # you, not the index
        self.assertEqual("b2", git(self.r.path, "show", "%s:b.txt" % snap))
        self.assertEqual("a2", git(self.r.path, "show", "%s:a.txt" % snap))

    def test_a_tracked_DELETION_reaches_the_snapshot(self):
        """`git add -u` stages deletions as well as edits; a draft that silently resurrected a
        deleted file would validate a tree nobody has."""
        os.remove(os.path.join(self.r.path, "ggml", "kernel.c"))
        snap = A.draft_snapshot(self.r.path)
        self.assertNotIn("ggml/kernel.c", git(self.r.path, "ls-tree", "-r", "--name-only", snap))

    def test_the_command_refuses_the_draft_and_names_the_allowance_flag(self):
        """Through the subcommand, not just the helper: the operator has to be told what to do."""
        commit_json(self.r.path, "tools/arifi-sync/protected-wins.json", {"version": 1, "wins": [
            {"id": "w", "status": "active", "mechanism": "m", "commit": "HEAD",
             "protected_paths": ["ggml/kernel.c"], "anchors": ["ARIFI_FAST_PATH"],
             "evidence": ["commit:HEAD"], "quality_gate": "q", "workload": "w",
             "measured_effect": "-1% per dispatch"}]})
        self.r.write("private/secret.key", self.SECRET)
        with self.assertRaises(A.Loud) as cm:
            A.cmd_protected_win_validate(
                self.r.path, self.r.cfg,
                type("A", (), {"ref": "main", "draft": True, "ref_explicit": False,
                               "allow_new": []})())
        self.assertIn("--allow-new", str(cm.exception))


class ReplayPatchIdLimitTest(unittest.TestCase):
    """The BOUNDARY of replay provenance, pinned so nobody discovers it during an update.

    `git patch-id --stable` canonicalises away line numbers, whitespace and the blob hashes in the
    `index` lines - all of which change legitimately on a replay. It does NOT canonicalise away
    CONTEXT lines. So a replay whose context shifted, because upstream edited the lines next to
    ours, has a different patch id even though the rebase applied without a conflict.

    That is left fail-closed on purpose. The mapping's whole claim is that the replayed commit is
    the change that was measured; once the surrounding code moved, that claim needs a human, not a
    looser hash. The alternative - matching on subject, or on a fuzzy diff - is the exact laundering
    the mandate forbids."""

    def setUp(self):
        self.r = Repo()

    def tearDown(self):
        self.r.close()

    def _lines(self, *body):
        return "".join(l + "\n" for l in body)

    def test_a_DISJOINT_replay_keeps_its_patch_id(self):
        """The case that maps: upstream touches another file entirely, so our patch is unchanged."""
        self.r.commit("ours: the win", {"f.c": self._lines("l1", "l2", "WIN", "l3")},
                      effect="-3% per dispatch")
        ours = git(self.r.path, "rev-parse", "HEAD")
        git(self.r.path, "checkout", "-q", "-b", "up", self.r.base)
        self.r.commit("upstream: an unrelated file", {"other.c": "int o(void){return 0;}\n"},
                      effect="tooling only")
        git(self.r.path, "checkout", "-q", "main")
        before = A._patch_id(self.r.path, ours)
        git(self.r.path, "rebase", "-q", "--onto", "up", self.r.base, "main")
        self.assertEqual(before, A._patch_id(self.r.path, git(self.r.path, "rev-parse", "HEAD")))

    def test_an_ADJACENT_upstream_edit_changes_the_patch_id_even_on_a_clean_rebase(self):
        """The case that does NOT map, and must not be made to. Measured on this box 2026-09-12:
        the rebase reports no conflict and the patch id still differs, so `replay-map` leaves the
        commit UNMATCHED and names manual registration."""
        # The upstream edit has to land INSIDE our hunk's three-line context window, which is what
        # "adjacent" means to patch-id. An edit further away leaves the context untouched and the
        # ids match - that case is the disjoint test above.
        plain = ("l1", "l2", "l3", "l4", "l5", "l6", "l7", "l8")
        shared = self.r.commit("base: the file", {"f.c": self._lines(*plain)},
                               effect="tooling only")
        ours = self.r.commit(
            "ours: the win",
            {"f.c": self._lines("l1", "l2", "l3", "l4", "WIN", "l5", "l6", "l7", "l8")},
            effect="-3% per dispatch")
        git(self.r.path, "checkout", "-q", "-b", "up", shared)
        self.r.commit(
            "upstream: edit the lines next to ours",
            {"f.c": self._lines("l1", "l2", "UPSTREAM", "l3", "l4", "l5", "l6", "l7", "l8")},
            effect="tooling only")
        git(self.r.path, "checkout", "-q", "main")
        before = A._patch_id(self.r.path, ours)
        git(self.r.path, "rebase", "-q", "--onto", "up", shared, "main")
        after = A._patch_id(self.r.path, git(self.r.path, "rev-parse", "HEAD"))
        self.assertTrue(before and after)
        self.assertNotEqual(before, after,
                            "if this ever goes green, patch-id started ignoring context and the "
                            "documented limitation needs re-measuring - do not delete the test")


class BumpCandidateIdentityTest(unittest.TestCase):
    """UPDATE-RUNBOOK 2.1's standing caveat, closed: `bump` replayed into a throwaway worktree and
    then built and judged the checkout it was standing in, which is still on the OLD base. Legs 3-4
    certified the tree the bump was supposed to replace.

    The replay is REAL in every case here, on a real miniature fork with a real committed series
    and a real `git am`. It used to be stubbed, and the stub is what let the previous pass ship a
    positive test that invented the tree equality it was supposed to prove (CHECK-UPDATE-ROOT
    finding 4). Only `build` and `judge` - the hardware legs - are replaced, and the protected-win
    preflight, which has its own tests above."""

    def setUp(self):
        self.r = Repo()
        # EFFECTS ARE EXPLICIT, and that is load-bearing now that the preservation preflight is no
        # longer stubbed. `Repo.commit`'s default effect ("test fixture") states no quantity and
        # matches no NO_EFFECT prefix, so `_measured_effect_is_real` calls it REAL - every fixture
        # commit was a measured win, which is what made the unstubbed preflight unusable in R2.
        # Exactly one commit here is a real measured win, and it is the one that is registered.
        self.win_sha = self.r.commit("vulkan: our win", dict(PATHS),
                                     effect="-12% per dispatch")
        self.r.commit("feat: a second patch", {"src/two.c": "int two(void){return 2;}\n"},
                      effect="tooling only")
        # upstream moves on its own line, off the shared base - an ordinary upstream bump
        git(self.r.path, "checkout", "-q", "-b", "upstream", self.r.base)
        self.r.commit("upstream: land a new file", {"upstream.c": "int u(void){return 0;}\n"},
                      effect="tooling only")
        self.onto = git(self.r.path, "rev-parse", "HEAD")
        # The upstream tip is TAGGED, because a real upstream bump pins a tag (b10825, ...) and
        # `set-base-pin` DERIVES the tag with `git describe --tags --exact-match`. An untagged
        # fixture tip would be testing the refusal path, not the ordinary update.
        git(self.r.path, "tag", "b-upstream", self.onto)
        git(self.r.path, "checkout", "-q", "main")
        self._write_manifest()
        self._write_sources()
        self.r.regen_and_commit()
        self.called = []
        # ONLY the hardware legs are replaced. `cmd_protected_win_check` runs for real: R2 stubbed
        # it, and the stub is exactly what hid the rebased registered win becoming UNREGISTERED.
        self._saved = (A.cmd_build, A.cmd_judge)
        A.cmd_build = self._forbidden("build")
        A.cmd_judge = self._forbidden("judge")

    def tearDown(self):
        (A.cmd_build, A.cmd_judge) = self._saved
        git(self.r.path, "branch", "-D", A.REPLAY_BRANCH, check=False)
        self.r.close()

    def test_the_fixture_effects_are_what_the_classifier_actually_says(self):
        """Pinned, because the whole suite's meaning depends on it: exactly one fixture commit is a
        real measured win. If `Repo.commit`'s default leaked back in here, every commit would be one
        and the unstubbed preflight below would be measuring noise."""
        self.assertTrue(A._measured_effect_is_real("-12% per dispatch"))
        self.assertFalse(A._measured_effect_is_real("tooling only"))
        self.assertFalse(A._measured_effect_is_real("generated artifact refresh"))
        self.assertFalse(A._measured_effect_is_real("tooling only - fixture"))

    def _manifest_entry(self, commit=None):
        return {"id": "test-win", "status": "active", "mechanism": "the fast path",
                "commit": commit or self.win_sha, "protected_paths": ["ggml/kernel.c"],
                "anchors": ["ARIFI_FAST_PATH"], "evidence": ["commit:" + self.win_sha],
                "quality_gate": "byte-identical output", "workload": "the seat serve line",
                "measured_effect": "-12% per dispatch"}

    def _write_manifest(self):
        commit_json(self.r.path, "tools/arifi-sync/protected-wins.json",
                    {"version": 1, "wins": [self._manifest_entry()]}, trailers=True)

    def _rebased(self, subject):
        """The post-rebase sha of the commit with this subject, found by walking the line - never
        assumed to be the old one."""
        for sha in git(self.r.path, "rev-list", "HEAD").split():
            if git(self.r.path, "log", "-1", "--format=%s", sha) == subject:
                return sha
        raise AssertionError("no commit with subject %r on HEAD" % subject)

    def _write_sources(self):
        """The config as a COMMITTED file, because the base pin is one of the things the documented
        repair sequence has to move - and a test that only moved an in-memory dict would prove the
        gate passes without proving the printed commands reach it."""
        doc = dict(self.r.cfg)
        doc["build"] = {"build_dir": "build-vulkan", "targets": ["llama-server"],
                        "cache_snapshot": "tools/arifi-sync/recipe/snap.txt",
                        "live_cache": "build-vulkan/CMakeCache.txt"}
        commit_json(self.r.path, "tools/arifi-sync/sources.json", doc, trailers=True)

    def _cfg(self):
        """Reload the config from disk, so the pin the repair sequence wrote is the pin used."""
        return A.load_config(os.path.join(self.r.path, "tools", "arifi-sync", "sources.json"))

    def _forbidden(self, name):
        def fn(repo, cfg, args):
            self.called.append(name)
            raise AssertionError("%s ran on the wrong tree - the refusal did not come first" % name)
        return fn

    def _bump(self, capture=False):
        args = type("A", (), {"onto": "upstream", "ref": "main",
                              "i_have_read_bench_purity": False})()
        if not capture:
            return A.cmd_bump(self.r.path, self._cfg(), args)
        import contextlib
        import io as _io
        buf = _io.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = A.cmd_bump(self.r.path, self._cfg(), args)
        self.printed = buf.getvalue()
        return rc

    def _ran(self, *argv):
        """Run one git command AND assert the refusal actually printed it.

        This is the durable close of CHECK-UPDATE-ROOT-R2 finding 6. The previous helper performed
        `git add -- patches/series` privately, so the test proved the convergence of a sequence the
        operator was never shown, and the literal printed sequence did not converge. Transcribing
        the commands into the test by hand would have the same failure mode one edit later, so the
        assertion is DERIVED: every command this test runs must appear verbatim in the captured
        refusal text. A step dropped from the print is a RED here."""
        printed = getattr(self, "printed", "")
        line = "git -C %s %s" % (self.r.path, " ".join(argv))
        self.assertIn(line, printed,
                      "the refusal never printed this step, so the test would be running a repair "
                      "no operator was given:\n    %s" % line)
        return git(self.r.path, *argv)

    def _ran_commit(self, pathspec, message):
        """`git commit -- <pathspec>`, asserted to be printed. The printed line carries no `-m`
        because it expects an editor; the test supplies the message non-interactively and the
        trailer the printed comment demands. The OPERATION - a pathspec commit of exactly this
        path - is the thing under test and it is asserted verbatim."""
        line = "git -C %s commit -- %s" % (self.r.path, pathspec)
        self.assertIn(line, getattr(self, "printed", ""),
                      "the refusal never printed this commit step:\n    %s" % line)
        git(self.r.path, "commit", "-q", "-m", message, "--", pathspec)

    def _run_printed(self, needle):
        """Run a printed `python tools/arifi-sync/arifi_sync.py ...` step through the PUBLIC CLI,
        with the argv taken from the printed line itself.

        CHECK-UPDATE-ROOT-R3 finding 5: step 3 used to be the prose line `edit
        tools/arifi-sync/sources.json: ...`, and this test reached that state through its own
        private `_write_sources_file()`. Both are gone. The line is located in the captured
        refusal, split, and handed to `A.main` - so the argv under test is the operator's argv,
        character for character, and a printed flag that `main` does not accept is a RED here.

        `A.DEFAULT_CONFIG` is repointed at the fixture for the call, which is the module-level
        stand-in for the operator's `cd` into their own checkout. It is restored in `finally`:
        the real repository's sources.json is never the file under test.
        """
        prefix = A.BUMP_STEP_PREFIX + "python tools/arifi-sync/arifi_sync.py "
        lines = [l for l in getattr(self, "printed", "").splitlines()
                 if l.startswith(prefix) and needle in l]
        self.assertEqual(1, len(lines),
                         "expected exactly one printed step containing %r, got %r"
                         % (needle, lines))
        # The printed step is a SHELL line: it propagates its own failure (`|| exit $?`) and may
        # carry a trailing comment. `main` takes the command, so both are cut here - and only here,
        # so the argv under test is still the printed one, character for character.
        cmd = lines[0][len(prefix):].split(" || ")[0].split("   # ")[0].rstrip()
        argv = shlex.split(cmd, posix=True)
        saved = A.DEFAULT_CONFIG
        A.DEFAULT_CONFIG = os.path.join(self.r.path, "tools", "arifi-sync", "sources.json")
        try:
            return A.main(argv)
        finally:
            A.DEFAULT_CONFIG = saved

    def _materialise(self):
        """The sequence `bump`'s refusal prints, in the order it prints it, with every command
        asserted to be one the refusal actually printed.

        Not every line is EXECUTED as printed, and the difference is deliberate:
          - `replay-map` and `set-base-pin` go through `_run_printed`: the printed argv, split and
            handed to the public `main`, character for character.
          - the `git commit -- <path>` lines are asserted verbatim by `_ran_commit` and then
            performed with `-q -m <message>` added - the printed line expects an editor.
          - `series regen` is asserted verbatim and then performed through `self.r.regen("main")`.
        `RunbookRecordedByBashTest` covers the one surface this class does not touch: the runbook's
        `bash` block, executed in a real shell.
        """
        if not getattr(self, "printed", ""):
            self._bump(capture=True)

        # 0. retain the pre-rebase tip, so the replayed commits can be mapped back to it. The ref
        #    is named after the tip it retains (R9 finding 1), and the printed gate verifies that
        #    it resolves to exactly that object before anything is rewritten.
        tip = git(self.r.path, "rev-parse", "main")
        retained = A.retained_ref_name(tip)
        self._ran("switch", "main")
        self.assertIn('rev-parse --verify --quiet "%s^{commit}")" = %s' % (retained, tip),
                      self.printed, "the printed sequence never verifies the RETAINED ref")
        self._ran("tag", retained, tip)
        # 1. rebase our line onto the new upstream - every fork commit gets a NEW sha here
        self._ran("rebase", "--onto", "upstream", self.r.base, "main")
        # 2. map the replayed commits to their registered originals, and COMMIT the ledger
        #    CHECK-UPDATE-ROOT-R4 finding 3: this step used to run the private
        #    `cmd_protected_win_replay_map()` while the printed line carried `--recorded-by <you>`,
        #    so the one command that was NOT executable as printed was also the one command this
        #    test did not execute. It goes through the public CLI now, argv taken from the print.
        self.assertIn("protected-win replay-map --ref main --retained-ref %s" % retained,
                      self.printed)
        self.assertEqual(0, self._run_printed("replay-map"))
        self._ran("add", "--", "tools/arifi-sync/%s" % A.REPLAY_PROVENANCE)
        self._ran_commit("tools/arifi-sync/%s" % A.REPLAY_PROVENANCE,
                         "chore: replay provenance\n\nOrigin: ArifiLabs (native)\n"
                         "Measured-effect: tooling only\n")
        # 3. the base pin moves BEFORE the regen - by running the PRINTED command, not by this
        #    test writing the file itself.
        self.assertEqual(0, self._run_printed("set-base-pin"))
        self.r.cfg = self._cfg()          # the pin on disk is now the pin the fixture speaks for
        self.assertEqual(self.onto, self.r.cfg["base"]["upstream_sha"])
        self.assertEqual("b-upstream", self.r.cfg["base"]["upstream_tag"])
        self._ran("add", "--", "tools/arifi-sync/sources.json")
        self._ran_commit("tools/arifi-sync/sources.json",
                         "chore: move the base pin\n\nOrigin: ArifiLabs (native)\n"
                         "Measured-effect: tooling only - fixture\n")
        # 4. regen against the NEW pin, STAGE, then commit. The `add` is the step R2's helper hid.
        self.assertIn("arifi_sync.py series regen --ref main", self.printed)
        self.assertEqual(0, self.r.regen("main"))
        self._ran("add", "--", "patches/series")
        self._ran_commit("patches/series",
                         "regen: bank the series\n\nOrigin: ArifiLabs (native)\n"
                         "Measured-effect: generated artifact refresh\n")

    def _replay_map(self, write=False, recorded_by="arifi-sync test"):
        args = type("A", (), {"ref": "main", "retained_ref": "arifi-pre-rebase",
                              "recorded_by": recorded_by, "write": write})()
        return A.cmd_protected_win_replay_map(self.r.path, self._cfg(), args)

    # -- the gate bites --------------------------------------------------------------------
    def test_bump_refuses_when_the_checkout_is_still_on_the_old_base(self):
        self.assertEqual(1, self._bump())
        self.assertEqual([], self.called, "build or judge ran despite a wrong-tree checkout")

    def test_bump_refuses_a_dirty_checkout(self):
        """A dirty tree has no identity a receipt can name, so it cannot be the candidate."""
        self._materialise()
        self.r.write("ggml/kernel.c", "int win(void) { return 3; }\n")
        self.assertEqual(1, self._bump())
        self.assertEqual([], self.called)

    def test_bump_refuses_a_HAND_EDITED_patch_the_series_exclusion_would_otherwise_hide(self):
        """The exclusion is what makes the comparison possible and is also the hole it could open:
        the candidate and its replay legitimately differ inside `patches/series`, so a smuggled
        byte there is invisible to the tree diff. It is not invisible to the integrity proof."""
        self._materialise()
        patches = sorted(f for f in os.listdir(self.r.series_dir) if f.endswith(".patch"))
        target = os.path.join(self.r.series_dir, patches[0])
        with open(target, "a", encoding="utf-8", newline="\n") as fh:
            fh.write("\n# smuggled\n")
        git(self.r.path, "add", "--", "patches/series")
        git(self.r.path, "commit", "-q", "-m",
            "tamper: hand-edit a patch\n\nOrigin: ArifiLabs (native)\n"
            "Measured-effect: tooling only\n", "--", "patches/series")
        self.assertEqual(1, self._bump())
        self.assertEqual([], self.called)

    def test_bump_refuses_a_STALE_series_that_no_longer_regenerates(self):
        self._materialise()
        # a non-measured effect on purpose: this case is about the STALE SERIES, and a commit that
        # classifies as a measured win would refuse one gate earlier, for a different reason.
        self.r.commit("feat: a third patch nobody regenerated",
                      {"src/three.c": "int three(void){return 3;}\n"}, effect="tooling only")
        self.assertEqual(1, self._bump())
        self.assertEqual([], self.called)

    def test_bump_refuses_an_UNCOMMITTED_patch_file(self):
        self._materialise()
        self.r.commit("feat: a third patch", {"src/three.c": "int three(void){return 3;}\n"},
                      effect="tooling only")
        self.assertEqual(0, self.r.regen())          # regenerated on disk, deliberately NOT committed
        self.assertEqual(1, self._bump())
        self.assertEqual([], self.called)

    def test_no_printed_COMMAND_carries_shell_metasyntax_or_a_placeholder(self):
        """The defect class behind CHECK-UPDATE-ROOT-R4 finding 3, not just its one instance.

        `--recorded-by <you>` was not merely unhelpful: in every shell that block is pasted into,
        `<you>` is input redirection, so the line reads a file named `you` and never supplies the
        required value. Any `<`/`>` in an INDENTED line is that same bug, so none is allowed.
        Comment lines are exempt on purpose - a `#` line may legitimately write `<sha>`.

        The two DELIBERATE redirections the retention gate writes - `>/dev/null` on the probe and
        `>&2` on its refusal message - are removed before the check rather than weakening it: they
        come from `retention_gate_lines()`, carry no placeholder, and a `<you>` or any other `<`/`>`
        still trips this test.
        """
        self._bump(capture=True)
        commands = [l for l in self.printed.splitlines()
                    if l.startswith(A.BUMP_STEP_PREFIX) and l.strip()]
        self.assertTrue(commands, "the refusal printed no command lines at all")
        for line in commands:
            bare = line.replace(">/dev/null", "").replace(">&2", "")
            self.assertNotIn("<", bare, "redirection metasyntax in a line printed as a command")
            self.assertNotIn(">", bare, "redirection metasyntax in a line printed as a command")

    def test_the_printed_replay_map_carries_a_CONCRETE_recorded_by_in_ONE_argv_token(self):
        """The printed identity must survive the split that runs it, as one token, resolved.

        A shell EXPANSION would pass this file's `<`/`>` check and still be wrong: the convergence
        test runs the line through `shlex.split` and `main`, with no shell, so `$(...)` would be
        recorded verbatim as the operator. The assertion is a round-trip against the resolver, not
        against a hardcoded address - the identity is whatever THIS checkout's git config names.
        """
        self._bump(capture=True)
        prefix = A.BUMP_STEP_PREFIX + "python tools/arifi-sync/arifi_sync.py "
        lines = [l for l in self.printed.splitlines()
                 if l.startswith(prefix) and "replay-map" in l]
        self.assertEqual(1, len(lines), lines)
        argv = shlex.split(lines[0][len(prefix):], posix=True)
        value = argv[argv.index("--recorded-by") + 1]
        self.assertEqual(A.recorded_by_identity(self.r.path), value)
        self.assertEqual("test@arifilabs.invalid", value)   # the fixture's git config, resolved
        for bad in ("$", "%", "`", "<", ">"):
            self.assertNotIn(bad, value, "an unexpanded expansion would be recorded as a person")

    def test_with_NO_resolvable_operator_the_printed_sequence_carries_NO_PLACEHOLDER(self):
        """CHECK-UPDATE-ROOT-R6 finding 2. There used to be a `replay-map` line here carrying the
        constant `unknown-operator`: executable, concrete-looking, and a registration statement made
        by nobody. What prints now is the one step that IS true then - set a real identity and
        re-run - and it is still a command, not prose."""
        saved = A.recorded_by_identity
        A.recorded_by_identity = lambda repo: ""
        self.addCleanup(lambda: setattr(A, "recorded_by_identity", saved))
        self._bump(capture=True)
        commands = [l for l in self.printed.splitlines()
                    if l.startswith(A.BUMP_STEP_PREFIX) and l.strip()]
        self.assertTrue(commands)
        self.assertNotIn("unknown-operator", self.printed)
        for line in commands:
            bare = line.replace(">/dev/null", "").replace(">&2", "")   # the retention gate's own
            self.assertNotIn("replay-map", line, "a replay-map line with no operator to record")
            self.assertNotIn("<", bare)
            self.assertNotIn(">", bare)
        self.assertIn("git config user.email", self.printed,
                      "the operator is not told how to supply the one thing only they know")
        self.assertTrue(any("arifi_sync.py bump --onto" in l and "--ref main" in l
                            for l in commands),
                        "the sequence must end at the step that is runnable now: re-run the bump")

    def test_a_PLACEHOLDER_operator_is_refused_at_WRITE_time_and_on_every_READ(self):
        dest = os.path.join(self.r.path, "tools", "arifi-sync", A.REPLAY_PROVENANCE)
        for bad in ("unknown-operator", "", "Someone <someone@host>"):
            with self.assertRaises(A.Loud):
                self._replay_map(write=True, recorded_by=bad)
        self.assertFalse(os.path.exists(dest), "a refused identity still reached the ledger")
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        row = {"rebased": "a" * 40, "original": "b" * 40, "patch_id": "c" * 40,
               "subject": "vulkan: our win", "trailers": "Origin: ArifiLabs (native)",
               "retained_ref": "arifi-pre-rebase", "recorded_utc": "2026-09-13T00:00:00Z",
               "recorded_by": "unknown-operator"}
        with open(dest, "w", encoding="utf-8", newline="\n") as fh:
            json.dump({"version": 1, "mappings": [row]}, fh)
        problems, mapped = A.replay_provenance(self.r.path, self._cfg())
        self.assertEqual({}, mapped, "a row recording nobody mapped a commit anyway")
        self.assertTrue(any("placeholder" in p for p in problems), problems)

    # -- the gate must NOT bite ------------------------------------------------------------
    def test_the_documented_repair_sequence_CONVERGES_to_a_green_bump(self):
        """The whole point, and what the previous pass could not do: an ordinary upstream update,
        driven by the commands the refusal prints, must reach legs 3 and 4 on the candidate.

        The first attempt refuses because the checkout is still on the old base. After the printed
        sequence - retain, rebase, map the replay, move the base pin, regenerate, stage and commit
        each - the SAME command passes, with every gate still enforced and NOTHING stubbed except
        the two hardware legs. In particular the protected-win preflight runs for real, over a real
        registered measured win whose sha the rebase rewrote."""
        self.assertEqual(1, self._bump(capture=True))
        self.assertEqual([], self.called)
        # the refusal must have printed the staging step R2 found missing
        self.assertIn("git -C %s add -- patches/series" % self.r.path, self.printed)

        self._materialise()

        # the win's sha really was rewritten, so this is the case R2 could not converge
        rebased = self._rebased("vulkan: our win")
        self.assertNotEqual(self.win_sha, rebased)
        # and it is treated as registered only through the ledger
        rows = A._replay_rows(self.r.path)
        self.assertEqual(1, len(rows))
        self.assertEqual(rebased, rows[0]["rebased"])
        self.assertEqual(self.win_sha, rows[0]["original"])
        problems, mapped = A.replay_provenance(self.r.path, self._cfg())
        self.assertEqual([], problems)
        self.assertEqual({rebased: self.win_sha}, mapped)

        ran = []
        A.cmd_build = lambda repo, cfg, args: ran.append("build") or 0
        A.cmd_judge = lambda repo, cfg, args: ran.append("judge") or 0
        self.assertEqual(0, self._bump())
        self.assertEqual(["build", "judge"], ran)

    def test_without_the_replay_ledger_the_rebased_win_REFUSES_as_unregistered(self):
        """The RED the ledger exists for, and the exact failure R2 reproduced. Same repair, minus
        step 2: the rebase rewrites the registered win's sha, the manifest still names the old one,
        and the fail-closed preflight refuses the update it is supposed to protect."""
        self._bump(capture=True)
        self._ran("switch", "main")
        self._ran("tag", A.retained_ref_name(git(self.r.path, "rev-parse", "main")),
                  git(self.r.path, "rev-parse", "main"))
        self._ran("rebase", "--onto", "upstream", self.r.base, "main")
        self.assertEqual(0, self._run_printed("set-base-pin"))
        self.r.cfg = self._cfg()
        self._ran("add", "--", "tools/arifi-sync/sources.json")
        self._ran_commit("tools/arifi-sync/sources.json",
                         "chore: move the base pin\n\nOrigin: ArifiLabs (native)\n"
                         "Measured-effect: tooling only - fixture\n")
        self.assertEqual(0, self.r.regen("main"))
        self._ran("add", "--", "patches/series")
        self._ran_commit("patches/series",
                         "regen: bank the series\n\nOrigin: ArifiLabs (native)\n"
                         "Measured-effect: generated artifact refresh\n")

        problems = A.validate_protected_wins(self.r.path, "main", self._cfg())
        self.assertTrue(any("UNREGISTERED measured win" in p for p in problems), problems)
        self.assertEqual(1, self._bump())
        self.assertEqual([], self.called, "build or judge ran despite an unregistered measured win")

    # -- replay provenance: the ledger is re-derived, never trusted ------------------------------
    def _ledger(self, rows):
        p = os.path.join(self.r.path, "tools", "arifi-sync", A.REPLAY_PROVENANCE)
        with open(p, "w", encoding="utf-8", newline="\n") as fh:
            json.dump({"version": 1, "mappings": rows}, fh)
            fh.write("\n")
        git(self.r.path, "add", "--", "tools/arifi-sync/%s" % A.REPLAY_PROVENANCE)
        git(self.r.path, "commit", "-q", "-m",
            "chore: ledger\n\nOrigin: ArifiLabs (native)\nMeasured-effect: tooling only\n",
            "--", "tools/arifi-sync/%s" % A.REPLAY_PROVENANCE)

    def _row(self, rebased, original, **over):
        row = {"rebased": rebased, "original": original,
               "patch_id": A._patch_id(self.r.path, rebased),
               "subject": git(self.r.path, "log", "-1", "--format=%s", rebased),
               "trailers": A.commit_trailers(self.r.path, rebased),
               "retained_ref": "arifi-pre-rebase", "recorded_utc": "2026-09-12T00:00:00+00:00",
               "recorded_by": "arifi-sync test"}
        row.update(over)
        return row

    def _rebase_only(self):
        self._bump(capture=True)
        git(self.r.path, "tag", "arifi-pre-rebase", "main")
        git(self.r.path, "rebase", "--onto", "upstream", self.r.base, "main")

    def test_a_NON_EQUIVALENT_patch_is_refused_by_the_ledger(self):
        """A row may only map commits whose patches are equivalent. Here the ledger claims the
        SECOND patch is the replay of the registered win - same shape of lie a conflict-edited
        replay tells, and refused for the same reason: the diffs are not the same change."""
        self._rebase_only()
        self._ledger([self._row(self._rebased("feat: a second patch"), self.win_sha)])
        problems, mapped = A.replay_provenance(self.r.path, self._cfg())
        self.assertEqual({}, mapped)
        self.assertTrue(any("NOT EQUIVALENT" in p for p in problems), problems)

    def test_an_AMBIGUOUS_ledger_maps_nothing(self):
        self._rebase_only()
        rebased = self._rebased("vulkan: our win")
        other = self._rebased("feat: a second patch")
        self._ledger([self._row(rebased, self.win_sha), self._row(other, self.win_sha)])
        problems, mapped = A.replay_provenance(self.r.path, self._cfg())
        self.assertEqual({}, mapped)
        self.assertTrue(any("AMBIGUOUS" in p for p in problems), problems)

    def test_a_ledger_row_whose_ORIGINAL_is_unregistered_is_refused(self):
        """Replay provenance carries an existing registration across a rewrite. It can never make
        one, so an original nobody registered maps nothing."""
        self._rebase_only()
        self._ledger([self._row(self._rebased("feat: a second patch"),
                                git(self.r.path, "rev-parse", "arifi-pre-rebase"))])
        problems, mapped = A.replay_provenance(self.r.path, self._cfg())
        self.assertEqual({}, mapped)
        self.assertTrue(any("ORIGINAL is itself unregistered" in p or "NOT EQUIVALENT" in p
                            for p in problems), problems)

    def test_a_ledger_row_with_no_RETAINED_ref_is_refused(self):
        """"It was in the reflog" is not evidence. The original has to be reachable from a ref the
        row names, or the mapping cannot be checked by anyone else."""
        self._rebase_only()
        rebased = self._rebased("vulkan: our win")
        self._ledger([self._row(rebased, self.win_sha, retained_ref="no/such/tag")])
        problems, mapped = A.replay_provenance(self.r.path, self._cfg())
        self.assertEqual({}, mapped)
        self.assertTrue(any("retained_ref" in p for p in problems), problems)

    def test_a_HAND_EDITED_patch_id_in_the_ledger_is_refused(self):
        self._rebase_only()
        rebased = self._rebased("vulkan: our win")
        self._ledger([self._row(rebased, self.win_sha, patch_id="0" * 40)])
        problems, mapped = A.replay_provenance(self.r.path, self._cfg())
        self.assertEqual({}, mapped)
        self.assertTrue(any("is not the patch id these commits have now" in p
                            for p in problems), problems)

    def test_replay_map_REFUSES_to_guess_when_nothing_matches(self):
        """The generator never invents a mapping: an unmatched measured win is named, and the
        command exits non-zero rather than writing a row."""
        self._rebase_only()
        self.r.commit("feat: a brand new measured win",
                      {"src/new.c": "int n(void){return 1;}\n"}, effect="-4.0% per dispatch")
        rc = self._replay_map(write=True)
        self.assertEqual(1, rc)
        rows = A._replay_rows(self.r.path)
        self.assertEqual(["vulkan: our win"], [r["subject"] for r in rows],
                         "the unmatched win was mapped anyway")

    def test_a_ledger_that_exists_only_ON_DISK_cannot_certify_the_ref(self):
        """The ledger decides whether a commit counts as registered, so a copy that is not the
        ref's own copy is the wrong-tree trap with a new file name."""
        self._rebase_only()
        p = os.path.join(self.r.path, "tools", "arifi-sync", A.REPLAY_PROVENANCE)
        with open(p, "w", encoding="utf-8", newline="\n") as fh:
            json.dump({"version": 1,
                       "mappings": [self._row(self._rebased("vulkan: our win"), self.win_sha)]}, fh)
        problems = A.candidate_identity_problems(self.r.path, "main")
        self.assertTrue(any(A.REPLAY_PROVENANCE in x for x in problems), problems)

    def test_the_replayed_candidate_is_RETAINED_and_holds_the_bumped_tree(self):
        """The replay worktree is still destroyed; the candidate it produced is not. It has to
        survive, because it is the only tree a bump is actually about."""
        self._materialise()
        A.cmd_build = lambda repo, cfg, args: 0
        A.cmd_judge = lambda repo, cfg, args: 0
        self.assertEqual(0, self._bump())
        tip = git(self.r.path, "rev-parse", A.REPLAY_BRANCH)
        self.assertTrue(tip)
        # the retained candidate carries upstream's file AND ours, and matches the checkout
        # everywhere outside the generated series
        self.assertIn("upstream.c", git(self.r.path, "ls-tree", "-r", "--name-only", tip))
        self.assertEqual("", git(self.r.path, "diff", "--name-only", tip, "HEAD",
                                 "--", ".", ":(exclude)patches/series"))

    def test_a_failing_build_still_refuses_the_bump(self):
        """Failure propagation: the identity guard must not swallow a leg-3 failure."""
        self._materialise()
        A.cmd_build = lambda repo, cfg, args: 1
        A.cmd_judge = self._forbidden("judge")
        self.assertEqual(1, self._bump())
        self.assertEqual([], self.called)

    def test_a_failing_judge_still_refuses_the_bump(self):
        self._materialise()
        A.cmd_build = lambda repo, cfg, args: 0
        A.cmd_judge = lambda repo, cfg, args: 1
        self.assertEqual(1, self._bump())

    def test_bump_refuses_without_an_explicit_ref(self):
        """--ref used to default out of sources.json - the very file whose staleness is the thing
        under suspicion. The candidate is named by the operator now."""
        import contextlib
        import io as _io
        with self.assertRaises(SystemExit) as cm:
            with contextlib.redirect_stderr(_io.StringIO()):
                A.main(["bump", "--onto", "b10481"])
        self.assertEqual(2, cm.exception.code)


class ConfigRepoPairingTest(unittest.TestCase):
    """`repo` is derived from the config's directory. A --repo pointing elsewhere means one
    checkout's configuration drives another checkout's tree - the wrong-tree trap at its source."""

    def test_a_config_from_another_checkout_is_refused(self):
        a, b = Repo(), Repo()
        try:
            commit_json(a.path, "tools/arifi-sync/sources.json",
                        {"base": {"upstream_sha": a.base, "ref": "main"}, "series_dir": "patches/series"})
            import contextlib
            import io as _io
            buf = _io.StringIO()
            with contextlib.redirect_stdout(buf):
                rc = A.main(["--config", os.path.join(a.path, "tools", "arifi-sync", "sources.json"),
                             "--repo", b.path, "status"])
            self.assertEqual(2, rc)
            self.assertIn("two different checkouts", buf.getvalue())
        finally:
            a.close()
            b.close()


class SetBasePinTest(unittest.TestCase):
    """`set-base-pin`, the command that replaced the printed prose line `edit
    tools/arifi-sync/sources.json: ...` (CHECK-UPDATE-ROOT-R3 finding 5).

    The pin is the single value that decides what `series regen` emits, so this command is allowed
    to write sources.json and nothing else is. Every case below asserts the file's BYTES, because
    "atomic" and "refuses" are claims about what is on disk afterwards, not about a return code."""

    def setUp(self):
        self.r = Repo()
        self.onto = self.r.commit("upstream: land a new file",
                                  {"upstream.c": "int u(void){return 0;}\n"}, effect="tooling only")
        git(self.r.path, "tag", "b-next", self.onto)
        self.cfg_path = os.path.join(self.r.path, "tools", "arifi-sync", "sources.json")
        self._write({"base": {"upstream_sha": self.r.base, "upstream_tag": "b-test", "ref": "main"},
                     "series_dir": "patches/series", "remotes": [], "local_checkouts": []})

    def tearDown(self):
        self.r.close()

    def _write(self, doc, eol="\r\n"):
        """CRLF by default: this repository is checked out CRLF, and a pin move that normalised
        line endings would turn a two-value edit into a whole-file diff."""
        text = (json.dumps(doc, indent=2) + "\n") if isinstance(doc, dict) else doc
        with open(self.cfg_path, "w", encoding="utf-8", newline=eol) as fh:
            fh.write(text)

    def _bytes(self):
        with open(self.cfg_path, "rb") as fh:
            return fh.read()

    def _run(self, *argv):
        import contextlib
        import io as _io
        buf = _io.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = A.main(["--config", self.cfg_path, "set-base-pin", *argv])
        self.printed = buf.getvalue()
        return rc

    def _refuses(self, *argv):
        """A refusal must leave the file byte-identical - that is the whole claim of the word
        'atomic' here, and a return code alone would not measure it."""
        before = self._bytes()
        rc = self._run(*argv)
        self.assertEqual(2, rc, self.printed)
        self.assertEqual(before, self._bytes(), "a REFUSED pin move still rewrote sources.json")
        return self.printed

    # -- it moves the pin, and only the pin ------------------------------------------------
    def test_it_moves_BOTH_halves_of_the_pin_and_derives_the_tag(self):
        self.assertEqual(0, self._run("--onto", "b-next", "--expect-sha", self.r.base))
        doc = A.load_config(self.cfg_path)
        self.assertEqual(self.onto, doc["base"]["upstream_sha"])
        self.assertEqual("b-next", doc["base"]["upstream_tag"],
                         "the tag must be DERIVED from the ref, not left at the old one")
        self.assertEqual("main", doc["base"]["ref"])
        self.assertEqual(["patches/series", [], []],
                         [doc["series_dir"], doc["remotes"], doc["local_checkouts"]],
                         "the rewrite touched a key that is none of its business")

    def test_the_rewrite_keeps_the_file_s_own_line_endings(self):
        self.assertEqual(0, self._run("--onto", "b-next", "--expect-sha", self.r.base))
        raw = self._bytes()
        self.assertEqual(raw.count(b"\n"), raw.count(b"\r\n"),
                         "a CRLF config came back with LF lines - a whole-file diff at integration")

    def test_a_branch_tip_can_be_pinned_when_the_tag_is_NAMED(self):
        """The derived tag is the default, not the only way: `--upstream-tag` is accepted when it
        resolves to the same commit."""
        self.assertEqual(0, self._run("--onto", "main", "--expect-sha", self.r.base,
                                      "--upstream-tag", "b-next"))
        self.assertEqual(self.onto, A.load_config(self.cfg_path)["base"]["upstream_sha"])

    def test_an_UPPERCASE_expect_sha_is_the_same_sha(self):
        """Git prints lowercase and so does this tool, but an operator pasting from elsewhere may
        not. A case difference is not a different pin, and refusing it would name the wrong
        reason."""
        self.assertEqual(0, self._run("--onto", "b-next", "--expect-sha", self.r.base.upper()))
        self.assertEqual(self.onto, A.load_config(self.cfg_path)["base"]["upstream_sha"])

    # -- and refuses every state it cannot vouch for ---------------------------------------
    def test_an_UNRESOLVABLE_onto_refuses(self):
        self.assertIn("cannot resolve",
                      self._refuses("--onto", "no-such-ref", "--expect-sha", self.r.base))

    def test_a_pin_that_is_NOT_the_one_expected_refuses(self):
        """Somebody else moved it, or this is another checkout. Either way the operator's mental
        model is wrong and overwriting it would be silent."""
        self.assertIn("not the one you expected",
                      self._refuses("--onto", "b-next", "--expect-sha", self.onto))

    def test_a_TRUNCATED_or_non_hex_expect_sha_refuses(self):
        self.assertIn("not a sha prefix",
                      self._refuses("--onto", "b-next", "--expect-sha", self.r.base[:6]))
        self.assertIn("not a sha prefix",
                      self._refuses("--onto", "b-next", "--expect-sha", "the-old-one"))

    def test_moving_the_pin_to_where_it_ALREADY_is_refuses(self):
        self.assertIn("already",
                      self._refuses("--onto", self.r.base, "--expect-sha", self.r.base))

    def test_an_UNPARSEABLE_config_refuses_before_it_is_overwritten(self):
        self._write('{"base": {"upstream_sha": "x",\n')
        self.assertIn("not parseable JSON",
                      self._refuses("--onto", "b-next", "--expect-sha", self.r.base))

    def test_a_config_with_no_base_object_refuses(self):
        self._write({"series_dir": "patches/series"})
        self.assertIn("no `base` object",
                      self._refuses("--onto", "b-next", "--expect-sha", self.r.base))

    def test_a_HALF_pin_refuses_rather_than_completing_itself(self):
        self._write({"base": {"upstream_sha": self.r.base, "ref": "main"}})
        self.assertIn("base.upstream_tag is missing",
                      self._refuses("--onto", "b-next", "--expect-sha", self.r.base))

    def test_an_UNTAGGED_target_refuses_and_names_the_flag(self):
        git(self.r.path, "tag", "-d", "b-next")
        out = self._refuses("--onto", "main", "--expect-sha", self.r.base)
        self.assertIn("carries no exact tag", out)
        self.assertIn("--upstream-tag", out)

    def test_a_tag_that_names_a_DIFFERENT_commit_refuses(self):
        git(self.r.path, "tag", "b-elsewhere", self.r.base)
        self.assertIn("must", self._refuses("--onto", "b-next", "--expect-sha", self.r.base,
                                            "--upstream-tag", "b-elsewhere"))


class RecordedByIdentityTest(unittest.TestCase):
    """The resolver behind the printed `--recorded-by`, tested as a function: fast, no bump.

    It exists because the printed line must be runnable in a shell AND executable by
    `shlex.split` + `main`, which rules out both the old `<you>` placeholder and any `$(...)`
    expansion. The shell-parsing cases live here; the proof that the PRINTED line is the one the
    convergence test runs lives in `BumpCandidateIdentityTest`.
    """

    def setUp(self):
        self.home = tempfile.mkdtemp(prefix="arifi-ident-")
        self.repo = os.path.join(self.home, "repo")
        os.makedirs(self.repo)
        # The box's own global/system git config would otherwise decide these cases. Cleanup is
        # registered BEFORE the mutation: a raising setUp skips tearDown, and a leaked
        # GIT_CONFIG_GLOBAL would follow every class that sorts after this one.
        self.saved = {k: os.environ.get(k) for k in ("GIT_CONFIG_GLOBAL", "GIT_CONFIG_SYSTEM")}
        self.addCleanup(self._restore_env)
        for k in self.saved:
            os.environ[k] = os.path.join(self.home, "no-such-gitconfig")
        git(self.repo, "init", "-q", "-b", "main")

    def _restore_env(self):
        for k, v in self.saved.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v

    def tearDown(self):
        shutil.rmtree(self.home, ignore_errors=True)

    def _split(self, value):
        """What a shell - and `shlex.split` - makes of the printed word for `value`."""
        return shlex.split("--recorded-by %s --write" % A._shell_word(value), posix=True)

    def test_the_email_is_preferred_and_printed_UNQUOTED(self):
        git(self.repo, "config", "user.email", "op@arifilabs.invalid")
        self.assertEqual("op@arifilabs.invalid", A.recorded_by_identity(self.repo))
        self.assertEqual("op@arifilabs.invalid", A._shell_word("op@arifilabs.invalid"))

    def test_a_name_with_SPACES_stays_ONE_argv_token(self):
        git(self.repo, "config", "user.name", "Angelo Arifi")
        value = A.recorded_by_identity(self.repo)
        self.assertEqual("Angelo Arifi", value)
        self.assertEqual(["--recorded-by", "Angelo Arifi", "--write"], self._split(value))
        self.assertEqual('"Angelo Arifi"', A._shell_word(value),
                         "single quotes do not group in cmd.exe; the block is pasted there too")

    def test_an_identity_carrying_SHELL_METASYNTAX_is_refused_and_falls_through(self):
        """`Someone <someone@host>` is a normal git ident and is redirection in a shell."""
        git(self.repo, "config", "user.email", "Someone <someone@host>")
        git(self.repo, "config", "user.name", "fallback-name")
        self.assertEqual("fallback-name", A.recorded_by_identity(self.repo))

    def _os_user(self, value):
        """Drive the OS-user rung, the one this box would otherwise decide."""
        saved = A.getpass.getuser
        A.getpass.getuser = lambda: value
        self.addCleanup(lambda: setattr(A.getpass, "getuser", saved))

    def _no_os_user(self):
        self._os_user("")

    def test_an_UNSAFE_OS_user_resolves_to_NOTHING_rather_than_being_printed(self):
        self._os_user("Someone <someone@host>")
        self.assertEqual("", A.recorded_by_identity(self.repo))

    def test_the_OS_user_is_the_LAST_rung_and_is_taken_when_git_names_nobody(self):
        self._os_user("angelo")
        self.assertEqual("angelo", A.recorded_by_identity(self.repo))

    def test_with_NO_git_identity_and_NO_OS_user_the_resolver_returns_NOTHING(self):
        """CHECK-UPDATE-ROOT-R6 finding 2. This assertion is the INVERSE of the R6 one
        (`assertTrue(value, "the step must print an identity, never an empty flag value")`), and it
        is not a relaxation: the value that used to satisfy it was the constant `unknown-operator`,
        a shell-safe token naming nobody, which strict provenance then accepted as an operator. No
        identity is now an EMPTY resolution that the printer, `replay-map --write` and every
        provenance read all refuse - which is stricter than printing a placeholder, not weaker."""
        self._no_os_user()
        self.assertEqual("", A.recorded_by_identity(self.repo))
        with open(A.__file__, encoding="utf-8") as fh:
            source = fh.read()
        self.assertNotIn('"unknown-operator"', source,
                         "the placeholder constant is back in the tool")

    def test_the_placeholder_rule_rejects_nobody_and_accepts_a_real_identity(self):
        for placeholder in ("unknown-operator", "unknown operator", "unknown", "operator", "you",
                            "your-name", "n/a", "TBD", "nobody", "you@example.invalid",
                            "someone@example.com"):
            self.assertTrue(A.recorded_by_problem(placeholder), placeholder)
        for real in ("test@arifilabs.invalid", "op@arifilabs.invalid", "Angelo Arifi",
                     "fallback-name"):
            self.assertEqual("", A.recorded_by_problem(real), real)
        self.assertTrue(A.recorded_by_problem(""))
        self.assertTrue(A.recorded_by_problem("Someone <someone@host>"))

    def test_a_configured_PLACEHOLDER_email_falls_through_to_a_real_name(self):
        git(self.repo, "config", "user.email", "unknown-operator")
        git(self.repo, "config", "user.name", "fallback-name")
        self.assertEqual("fallback-name", A.recorded_by_identity(self.repo))


RUNBOOK = os.path.normpath(os.path.join(HERE, "..", "..", "UPDATE-RUNBOOK.md"))


def _runbook_text():
    """The runbook, read with universal newlines: the file on this box is CRLF."""
    with open(RUNBOOK, encoding="utf-8") as fh:
        return fh.read()


def _runbook_bash_block(needle):
    """The one ```bash fence in the runbook that contains `needle`, as LF text."""
    blocks, cur = [], None
    for line in _runbook_text().splitlines():
        if cur is None:
            if line.strip() == "```bash":
                cur = []
        elif line.strip() == "```":
            blocks.append("\n".join(cur) + "\n")
            cur = None
        else:
            cur.append(line)
    hits = [b for b in blocks if needle in b]
    if len(hits) != 1:
        raise AssertionError("expected exactly one ```bash block containing %r, got %d"
                             % (needle, len(hits)))
    return hits[0]


def _git_bash():
    """The bash to run the runbook block with.

    NOT plain `shutil.which("bash")` on this box: that finds `C:\\Windows\\System32\\bash.exe`, the
    WSL launcher, which prints "Windows Subsystem for Linux has no installed distributions." and
    exits 1 - a failure that looks exactly like the block refusing. Git for Windows' bash is derived
    from `git` itself (`...\\Git\\cmd\\git.exe` -> `...\\Git\\bin\\bash.exe`); elsewhere the PATH
    bash is correct.
    """
    g = shutil.which("git")
    if g:
        cand = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(g))), "bin", "bash.exe")
        if os.path.exists(cand):
            return cand
    b = shutil.which("bash")
    return b if b and "system32" not in os.path.normcase(b) else None


@unittest.skipUnless(_git_bash(), "no non-WSL bash found")
class RunbookRecordedByBashTest(unittest.TestCase):
    """CHECK-UPDATE-ROOT-R5 finding 1: the runbook's own block, RUN, in a real shell.

    `--recorded-by "$(git config user.email)"` is executable and still wrong: with `user.email`
    unset the substitution supplies an argv token whose value is the empty string, the flag parses,
    and the PUBLIC boundary then refuses it - `cmd_protected_win_replay_map()` applies
    `recorded_by_problem()` before it resolves a ref or writes anything, so no ledger row is
    written at all. The refusal lands mid-update, after the rewrite, when the operator wanted a
    ledger. The block must therefore fall back, and REFUSE before it ever reaches that boundary.

    The block is read out of UPDATE-RUNBOOK.md and executed as written - no copy lives here, so
    editing the runbook without editing the ladder is a RED test. `python` is shadowed by a shell
    FUNCTION (a function beats PATH on the command word), which records the argv the block would
    have handed the tool; `git` still resolves to real git.
    """

    TIP = "0" * 40               # stands for the pre-rebase tip the block above captured
    RECORDER = 'python() { printf "%s\\n" "$@" > "$ARGV_OUT"; }\n'
    # The same recorder, standing for the PUBLIC CLI refusing (an unmappable commit, a stale
    # ledger, a placeholder the ladder somehow let through): argv is still recorded, the status is
    # nonzero. `3` rather than `1` so the assertion discriminates a propagated status from any
    # other failure.
    REFUSING_RECORDER = 'python() { printf "%s\\n" "$@" > "$ARGV_OUT"; return 3; }\n'

    def setUp(self):
        self.home = tempfile.mkdtemp(prefix="arifi-runbook-")
        self.repo = os.path.join(self.home, "repo")
        os.makedirs(self.repo)
        self.argv_out = os.path.join(self.home, "argv.txt").replace("\\", "/")
        git(self.repo, "init", "-q", "-b", "main")

    def tearDown(self):
        shutil.rmtree(self.home, ignore_errors=True)

    def _run_block(self, env=None, recorder=None):
        """Run the runbook block in bash. Returns (exit code, argv list or None, stderr).

        A marker command is appended AFTER the block, standing for the `add`/`commit` an operator
        pastes next, and the harness deliberately does NOT run `set -e`: that is the caller state
        CHECK-UPDATE-ROOT-R6 finding 3 is about. `self.walked_on()` then answers the actual
        question - did the sequence continue past a refusal?"""
        script = os.path.join(self.home, "block.sh")
        self.walk_out = os.path.join(self.home, "walked-on.txt")
        with open(script, "w", encoding="utf-8", newline="\n") as fh:
            # The retained ref this block hands `replay-map` is named after the pre-rebase tip the
            # block ABOVE it captured (R9 finding 1), and reaches this one as the same operator
            # placeholder as `<repo>`. Substituted here so the block runs as an operator's paste
            # does; `RunbookRebasePrerequisiteBashTest` is where the real sha flows through it.
            fh.write((recorder or self.RECORDER)
                     + _runbook_bash_block("--recorded-by").replace("<pre-rebase-tip-sha>", self.TIP)
                     + '\nprintf WALKED_ON > "%s"\n' % self.walk_out.replace("\\", "/"))
        extra = env or {}
        env = dict(os.environ)
        # This box's own git config and OS identity must not decide these cases.
        env["GIT_CONFIG_GLOBAL"] = env["GIT_CONFIG_SYSTEM"] = os.path.join(self.home, "no-such")
        for k in ("USER", "USERNAME", "LOGNAME"):
            env.pop(k, None)
        env.update(extra)
        env["LC_ALL"] = "C"               # so grep -E's A-Za-z0-9 ranges are locale-deterministic
        env["ARGV_OUT"] = self.argv_out
        r = subprocess.run([_git_bash(), "--noprofile", "--norc", script],
                           cwd=self.repo, env=env, capture_output=True, text=True)
        argv = None
        if os.path.exists(self.argv_out):
            with open(self.argv_out, encoding="utf-8") as fh:
                argv = fh.read().splitlines()
        return r.returncode, argv, r.stderr

    def walked_on(self):
        """Did the sequence continue past the block? (The `add`/`commit` an operator pastes next.)"""
        return os.path.exists(self.walk_out)

    def _recorded(self, argv):
        self.assertIsNotNone(argv, "the block refused where it should have run replay-map")
        i = argv.index("--recorded-by")
        return argv[i:i + 3]

    def test_an_UNSET_email_falls_back_to_the_name_as_ONE_argv_token(self):
        """The case the old `$(git config user.email)` line got wrong, end to end."""
        git(self.repo, "config", "user.name", "Angelo Arifi")
        rc, argv, _ = self._run_block()
        self.assertEqual(0, rc)
        self.assertEqual(["--recorded-by", "Angelo Arifi", "--write"], self._recorded(argv),
                         "the identity must be ONE nonempty argv token; an empty one is the "
                         "INCOMPLETE provenance row this finding is about")
        self.assertTrue(self.walked_on(), "a RESOLVED identity must let the sequence continue - "
                                          "otherwise the refusal test below proves nothing")

    def test_an_UNSAFE_email_falls_THROUGH_to_a_safe_name(self):
        """CHECK-UPDATE-ROOT-R6 finding 1, in the shell it is pasted into.

        `Someone <someone@host>` is a normal git ident and is redirection in a shell. The old ladder
        tested each rung for NONEMPTY only and validated once at the end, so this unsafe email
        blocked the safe `user.name` below it and the block refused with a usable identity on disk.
        """
        git(self.repo, "config", "user.email", "Someone <someone@host>")
        git(self.repo, "config", "user.name", "fallback-name")
        rc, argv, err = self._run_block()
        self.assertEqual(0, rc, "the unsafe email must not refuse a checkout that names a safe "
                                "identity one rung down: %s" % err)
        self.assertEqual(["--recorded-by", "fallback-name", "--write"], self._recorded(argv))

    def test_an_UNSAFE_USER_falls_THROUGH_to_a_safe_USERNAME(self):
        """The same defect on the OS rung: `USER` unsafe must not shadow a safe `USERNAME`."""
        rc, argv, err = self._run_block(env={"USER": "bad$(id)", "USERNAME": "angelo"})
        self.assertEqual(0, rc, err)
        self.assertEqual(["--recorded-by", "angelo", "--write"], self._recorded(argv))

    def test_NO_identity_at_all_REFUSES_before_replay_map_AND_STOPS_THE_SEQUENCE(self):
        rc, argv, err = self._run_block()
        self.assertNotEqual(0, rc, "a refusal that exits 0 lets the pasted sequence walk on")
        self.assertIsNone(argv, "replay-map ran with no resolvable operator")
        self.assertIn("REFUSE", err)
        # CHECK-UPDATE-ROOT-R6 finding 3: the harness runs NO `set -e`, so this is the caller state
        # in which the old `false` let the next pasted command run anyway.
        self.assertFalse(self.walked_on(),
                         "the sequence continued past the refusal: the stop must not depend on the "
                         "caller having run `set -e`")

    def test_a_PLACEHOLDER_email_never_reaches_argv_AND_STOPS_THE_SEQUENCE(self):
        """CHECK-UPDATE-ROOT-R7 finding 1, in the shell it is pasted into.

        `unknown-operator` is nonempty and shell-safe, so the old ladder selected it and handed it
        to the public `replay-map`, which correctly refused - and the block neither checked that
        status nor ran under `set -e`, so the paste WALKED ON into the `add`/`commit` and could
        commit stale provenance after a refusal. Three things are proved here at once: the
        placeholder never reaches argv, the block's own status is nonzero, and nothing below it
        runs."""
        git(self.repo, "config", "user.email", "unknown-operator")
        rc, argv, err = self._run_block()
        self.assertNotEqual(0, rc, "a refusal that exits 0 lets the pasted sequence walk on")
        self.assertIsNone(argv, "the placeholder reached the public CLI's argv: the ladder must "
                                "refuse a value that names nobody BEFORE using it")
        self.assertIn("REFUSE", err)
        self.assertFalse(self.walked_on(),
                         "the sequence continued past the refusal: the stop must not depend on "
                         "the caller having run `set -e`")

    def test_a_PLACEHOLDER_email_falls_THROUGH_to_a_safe_real_name(self):
        """The placeholder half of the rule is a FALL-THROUGH, exactly like the unsafe half: a
        checkout that names a real person one rung down is not refused. This mirrors
        `test_a_configured_PLACEHOLDER_email_falls_through_to_a_real_name` on the Python rung."""
        git(self.repo, "config", "user.email", "unknown-operator")
        git(self.repo, "config", "user.name", "fallback-name")
        rc, argv, err = self._run_block()
        self.assertEqual(0, rc, err)
        self.assertEqual(["--recorded-by", "fallback-name", "--write"], self._recorded(argv))
        self.assertTrue(self.walked_on())

    def test_a_REFUSING_replay_map_STOPS_THE_SEQUENCE_carrying_ITS_status(self):
        """The public boundary is the authority: when `replay-map` itself refuses - for any reason
        the local ladder cannot see - `|| exit $?` ends the paste with the tool's OWN status,
        without ambient `set -e`."""
        git(self.repo, "config", "user.email", "op@arifilabs.invalid")
        rc, argv, _ = self._run_block(recorder=self.REFUSING_RECORDER)
        self.assertEqual(3, rc, "the public CLI's status must be PROPAGATED, not swallowed or "
                                "flattened to a generic 1")
        self.assertEqual(["--recorded-by", "op@arifilabs.invalid", "--write"], self._recorded(argv))
        self.assertFalse(self.walked_on(),
                         "the `add`/`commit` ran after `replay-map` refused: it would carry stale "
                         "provenance")

    def test_the_runbook_identity_rule_IS_the_python_one(self):
        """ONE definition, mirrored - and pinned, so the mirror cannot drift.

        The bash ladder cannot import `recorded_by_problem()`, so it greps the SAME two pattern
        strings. Both patterns and both grep flag sets are asserted here: a `(?i)` added inline to
        the Python pattern (which `grep -E` would read literally) or an `-i` dropped from the
        placeholder grep is a RED test rather than a silent divergence."""
        block = _runbook_bash_block("--recorded-by")
        for var, rx in (("safe", A._RECORDED_BY_SAFE),
                        ("names_nobody", A._RECORDED_BY_PLACEHOLDER)):
            m = re.search(r"^%s='(.*)'$" % var, block, re.M)
            self.assertIsNotNone(m, "the runbook block lost its `%s=` pattern" % var)
            self.assertEqual(rx.pattern, m.group(1),
                             "the runbook's `%s` pattern drifted from the tool's" % var)
            self.assertNotIn("(?", rx.pattern, "an inline regex flag/group is not POSIX ERE: "
                                               "`grep -E` would read it literally")
        self.assertTrue(A._RECORDED_BY_PLACEHOLDER.flags & re.IGNORECASE,
                        "the placeholder rule is case-insensitive in the FLAG; the block mirrors "
                        "that with `grep -Ei`")
        self.assertFalse(A._RECORDED_BY_SAFE.flags & re.IGNORECASE,
                         "the safe-character rule is case-SENSITIVE; the block mirrors that with "
                         "`grep -E`")
        self.assertIn('grep -Eq "$safe"', block)
        self.assertIn('grep -Eiq "$names_nobody"', block)

    def test_the_runbook_retention_block_IS_the_generated_one(self):
        """CHECK-UPDATE-ROOT-R9 finding 2: the runbook grew a fence the printed repair never had,
        while still telling the operator that `bump` "prints the same list".

        `retention_gate_lines()` is the ONE source now - `bump_repair_sequence()` emits it resolved
        against the live tip, and this page publishes it with the operator placeholders. The
        equality is asserted line for line, so a fix applied to one and not the other is RED here
        rather than a divergence an operator discovers mid-update.
        """
        block = _runbook_bash_block("rebase --onto <new-upstream-sha>")
        lines = [l for l in block.splitlines() if l.strip() and not l.strip().startswith("#")]
        expect = A.retention_gate_lines("<repo>", "arifi/main", "<pre-rebase-tip-sha>")
        expect.append("git -C <repo> rebase --onto <new-upstream-sha> <old-base-sha> "
                      "arifi/main || exit $?")
        self.assertEqual(expect, lines,
                         "the runbook's retention block is no longer what the tool generates")
        # and the ref that was retained is the ref handed to the boundary, in the NEXT block
        self.assertIn("--retained-ref %s " % A.retained_ref_name("<pre-rebase-tip-sha>"),
                      _runbook_bash_block("--recorded-by"),
                      "the identity block maps against a different ref than the one retained")

    def test_the_runbook_does_not_CLAIM_more_than_is_tested(self):
        """CHECK-UPDATE-ROOT-R5 finding 2, as a guard: the two retired sentences stay retired, and
        the claims that replaced them stay present."""
        text = _runbook_text()
        for overclaim in ("executes the printed lines verbatim", "prints the same identity",
                          # CHECK-UPDATE-ROOT-R8 finding 2: an empty identity does NOT reach disk.
                          # The public boundary refuses it before any row is written; the strict
                          # read is the second fence, not the first.
                          "the row is written",
                          # CHECK-UPDATE-ROOT-R8 finding 1: switch and rebase are fail-stop now.
                          "These three lines deliberately carry NO",
                          # CHECK-UPDATE-ROOT-R9 finding 1: "it resolves to a commit" was exactly
                          # the check a STALE tag passed. Create-if-absent + verify-EXACT subsumes
                          # the old exception, so its reasoning must not survive on the page.
                          "rev-parse --verify --quiet arifi-pre-rebase^{commit} >/dev/null",
                          "`git tag` is the ONE exception, and only for the already-retained case",
                          "MUST exist and resolve to a commit before the",
                          # R9 finding 2: what this page publishes is what the tool generates.
                          "runs this block in a real shell"):
            self.assertNotIn(overclaim, text, "retired overclaim is back in the runbook")
        self.assertNotIn("unknown-operator", text,
                         "the retired placeholder constant is back in the runbook")
        for present in ('for candidate in "$(git config --get user.email)" '
                        '"$(git config --get user.name)" \\',
                        '"${USER:-}" "${USERNAME:-}"; do',
                        "REFUSE: no usable identity for --recorded-by",
                        "The refusal branch ends in `exit 1`, not in `false`",
                        "`|| exit $?` for the same reason, and it propagates the tool's OWN status",
                        "a semantic placeholder — a value like",
                        "closes THAT shell instead",
                        # R8 finding 1: the fail-stop, its ONE exception, and what makes the
                        # exception safe rather than a swallowed failure.
                        "git -C <repo> switch arifi/main || exit $?",
                        "git -C <repo> rebase --onto <new-upstream-sha> <old-base-sha> "
                        "arifi/main || exit $?",
                        # R9 finding 1: WHY the name carries the sha, in the one sentence that
                        # discriminates it from "just delete the stale tag".
                        "every prior cycle's ledger rows name that ref",
                        "exists at another object the sequence fails closed",
                        "`RunbookRebasePrerequisiteBashTest`",
                        "test_the_runbook_retention_block_IS_the_generated_one",
                        # R8 finding 2: the boundary, not the readback, is what refuses.
                        "NO ledger row is written at all",
                        "The `bash` block above is NOT executed by that test."):
            self.assertIn(present, text, "the runbook lost a statement this test speaks for")


@unittest.skipUnless(_git_bash(), "no non-WSL bash found")
class RunbookRebasePrerequisiteBashTest(unittest.TestCase):
    """CHECK-UPDATE-ROOT-R8 finding 1: the switch/tag/rebase block, RUN, against a real repository.

    These three commands are the PREREQUISITES of everything below them. A `switch` that refuses
    leaves the operator on the wrong checkout, so the retained tag names the wrong tip; a `rebase`
    that stops on a conflict leaves the series half replayed, so `replay-map` would map a line that
    does not exist. Before R9 neither carried `|| exit $?`, and the harness runs NO ambient
    `set -e`, so a paste walked straight on into the ledger, the pin and the commits.

    R9 finding 1 is the other half, and the harder one: the gate used to ask only "does
    `arifi-pre-rebase` resolve?", which a STALE tag from an earlier cycle answers yes. The retained
    ref is named after the tip it retains now, so this class also exercises the stale, the matching,
    the colliding and the moved-tip cases.

    What runs is the sequence `bump_repair_sequence()` GENERATES (R9 finding 2 - the runbook block
    was the only thing ever executed before, and the printed repair had neither fence); the IDENTITY
    block from the runbook is appended after it (the operator's next paste) and a marker after that
    (the `add`/`commit`). `python` is shadowed by the same recorder `RunbookRecordedByBashTest` uses,
    so `argv is None` proves the updater never ran.
    """

    def setUp(self):
        self.home = tempfile.mkdtemp(prefix="arifi-prereq-")
        self.argv_out = os.path.join(self.home, "argv.txt").replace("\\", "/")
        self.walk_out = os.path.join(self.home, "walked-on.txt").replace("\\", "/")

    def tearDown(self):
        shutil.rmtree(self.home, ignore_errors=True)

    def _build(self, conflict=False):
        """A real repo: `main` carries the new upstream commit, `arifi/main` carries the win.

        `conflict=True` makes the win and the upstream commit touch the SAME line, so the real
        `git rebase --onto` really conflicts - no faked status anywhere in that case.
        """
        repo = tempfile.mkdtemp(prefix="repo-", dir=self.home)
        git(repo, "init", "-q", "-b", "main")
        git(repo, "config", "user.email", "op@arifilabs.invalid")   # also the identity ladder's win
        git(repo, "config", "user.name", "Op")
        # LOCAL, so it survives into the bash run, which hides this box's global config: with
        # `core.autocrlf=true` only here, the block's `switch` sees every LF file as modified and
        # refuses for a reason that has nothing to do with what these cases measure.
        git(repo, "config", "core.autocrlf", "false")
        self._write(repo, "f.txt", "base\n")
        git(repo, "add", "-A")
        git(repo, "commit", "-qm", "base")
        base = git(repo, "rev-parse", "HEAD")
        git(repo, "branch", "arifi/main")
        self._write(repo, "f.txt", "upstream\n" if conflict else "base\nupstream\n")
        git(repo, "add", "-A")
        git(repo, "commit", "-qm", "upstream")
        upstream = git(repo, "rev-parse", "HEAD")
        git(repo, "switch", "-q", "arifi/main")
        self._write(repo, "f.txt" if conflict else "win.txt", "win\n")
        git(repo, "add", "-A")
        git(repo, "commit", "-qm", "win")
        git(repo, "switch", "-q", "main")       # the block's own `switch` must do the work
        return repo, base, upstream

    @staticmethod
    def _write(repo, rel, text):
        with open(os.path.join(repo, rel), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)

    def _generated_prerequisites(self, repo, base, upstream):
        """The PUBLIC generated sequence for this repo, cut after its rebase line.

        CHECK-UPDATE-ROOT-R9 finding 2: this class used to extract the runbook's block, so the
        thing an operator actually receives - the sequence the TOOL prints - was executed by
        nothing. It comes out of `bump_repair_sequence()` now, and
        `test_the_runbook_retention_block_IS_the_generated_one` proves the runbook publishes these
        same lines. The slice is by CONTENT, not by index: a gate line added later cannot silently
        drift out of what this harness runs.
        """
        cfg = {"series_dir": "patches/series"}
        args = type("A", (), {"ref": "arifi/main", "onto": upstream,
                              "i_have_read_bench_purity": False})()
        out = []
        for line in A.bump_repair_sequence(repo.replace("\\", "/"), cfg, args, base):
            if not line.startswith(A.BUMP_STEP_PREFIX):
                continue                        # a `#` comment line, not a command
            out.append(line[len(A.BUMP_STEP_PREFIX):])
            if " rebase --onto " in line:
                return "\n".join(out) + "\n"
        raise AssertionError("the generated sequence never reached a rebase line: %r" % out)

    @staticmethod
    def _flag(argv, name):
        """The value the recorded argv gave `name`."""
        return argv[argv.index(name) + 1] if name in argv else None

    def _run(self, repo, base, upstream, git_shim="", mutate=None):
        """Run the generated prerequisites + the runbook identity block + a marker.

        -> (rc, argv, stderr). `mutate` runs AFTER the script is generated and BEFORE it executes,
        which is how the "the tip moved since `bump` printed" case is staged.
        """
        tip = git(repo, "rev-parse", "arifi/main")
        block = self._generated_prerequisites(repo, base, upstream)
        # The retained ref carries the captured sha, and the identity block hands that same ref to
        # `replay-map`. Substituting it here is what the `--retained-ref` assertions below check.
        identity = _runbook_bash_block("--recorded-by").replace("<pre-rebase-tip-sha>", tip)
        script = os.path.join(self.home, "prereq.sh")
        with open(script, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(RunbookRecordedByBashTest.RECORDER + git_shim + block + identity
                     + '\nprintf WALKED_ON > "%s"\n' % self.walk_out)
        if mutate:
            mutate()
        env = dict(os.environ)
        env["GIT_CONFIG_GLOBAL"] = env["GIT_CONFIG_SYSTEM"] = os.path.join(self.home, "no-such")
        env["LC_ALL"] = "C"
        env["ARGV_OUT"] = self.argv_out
        r = subprocess.run([_git_bash(), "--noprofile", "--norc", script],
                           cwd=repo, env=env, capture_output=True, text=True)
        argv = None
        if os.path.exists(self.argv_out):
            with open(self.argv_out, encoding="utf-8") as fh:
                argv = fh.read().splitlines()
        return r.returncode, argv, r.stderr

    def walked_on(self):
        """Did the sequence continue past the block? (The `add`/`commit` pasted next.)"""
        return os.path.exists(self.walk_out)

    @staticmethod
    def _probe(repo, *args):
        """The status the same command really returns - measured in a THROWAWAY twin of the repo.

        Never in the repo the block will run in: a probe `rebase` leaves it mid-rebase and the
        block would then be measuring the probe's wreckage instead of its own subject.
        """
        return subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True).returncode

    def test_the_prerequisite_block_RUNS_CLEAN_and_lets_the_sequence_continue(self):
        """The control: without it, every refusal case below proves nothing."""
        repo, base, upstream = self._build()
        before = git(repo, "rev-parse", "arifi/main")
        rc, argv, err = self._run(repo, base, upstream)
        self.assertEqual(0, rc, err)
        self.assertEqual(upstream, git(repo, "rev-parse", "arifi/main~1"), "the rebase did not run")
        self.assertEqual(before, git(repo, "rev-parse", A.retained_ref_name(before)),
                         "the PRE-rebase tip is what the manifest entries name; a tag on the "
                         "rewritten tip retains nothing")
        self.assertIsNotNone(argv, "replay-map did not run after a clean prerequisite block")
        self.assertEqual(A.retained_ref_name(before), self._flag(argv, "--retained-ref"),
                         "the ref `replay-map` was handed is not the one that was RETAINED, so the "
                         "captured tip never reached the boundary that checks reachability")
        self.assertTrue(self.walked_on())

    def test_a_FAILED_switch_STOPS_THE_SEQUENCE_carrying_ITS_status(self):
        """The stale lane worktree of §5.2, which is the real-world case: `arifi/main` resolves
        (so the sequence prints in full) but another worktree holds it, so `switch` refuses. The
        paste must stop with git's OWN status, and must not tag, rebase, map or commit."""
        repo, base, upstream = self._build()
        before = git(repo, "rev-parse", "arifi/main")
        git(repo, "worktree", "add", "-q", os.path.join(self.home, "lane"), "arifi/main")
        twin, _, _ = self._build()
        git(twin, "worktree", "add", "-q", os.path.join(self.home, "lane-twin"), "arifi/main")
        expect = self._probe(twin, "switch", "arifi/main")
        self.assertNotEqual(0, expect, "the probe did not fail; this case tests nothing")
        rc, argv, _ = self._run(repo, base, upstream)
        self.assertEqual(expect, rc, "the failed `switch` status must be PROPAGATED, not swallowed")
        self.assertEqual("", git(repo, "tag", "-l", A.retained_ref_name(before)),
                         "a tag was retained over a checkout the switch never reached")
        self.assertIsNone(argv, "replay-map ran after the switch failed")
        self.assertFalse(self.walked_on(), "the sequence walked on past a failed `switch` - the "
                                           "stop must not depend on the caller having run `set -e`")

    def test_a_CONFLICTED_rebase_STOPS_THE_SEQUENCE_carrying_ITS_status(self):
        """A real conflict, not a faked status: the half-replayed line must not reach `replay-map`,
        which would otherwise map commits that do not exist yet."""
        repo, base, upstream = self._build(conflict=True)
        twin, tbase, tup = self._build(conflict=True)
        git(twin, "switch", "-q", "arifi/main")
        expect = self._probe(twin, "rebase", "--onto", tup, tbase, "arifi/main")
        self.assertNotEqual(0, expect, "the probe rebase did not conflict; this case tests nothing")
        rc, argv, _ = self._run(repo, base, upstream)
        self.assertEqual(expect, rc, "the conflicted `rebase` status must be PROPAGATED")
        self.assertIsNone(argv, "replay-map ran on a half-replayed line")
        self.assertFalse(self.walked_on(), "the sequence walked on into `add`/`commit` mid-rebase")

    def test_an_ALREADY_RETAINED_tag_does_NOT_stop_the_sequence(self):
        """The ONE justified exception, still intact: `git tag` failing because THIS cycle's ref
        already exists from an earlier partial run must not strand the operator over a tip that is
        already retained."""
        repo, base, upstream = self._build()
        before = git(repo, "rev-parse", "arifi/main")
        git(repo, "tag", A.retained_ref_name(before), "arifi/main")
        rc, argv, err = self._run(repo, base, upstream)
        self.assertEqual(0, rc, "an already-existing retained tag must not stop the sequence: %s"
                                % err)
        self.assertEqual(before, git(repo, "rev-parse", A.retained_ref_name(before)))
        self.assertEqual(upstream, git(repo, "rev-parse", "arifi/main~1"), "the rebase did not run")
        self.assertIsNotNone(argv)
        self.assertEqual(A.retained_ref_name(before), self._flag(argv, "--retained-ref"))
        self.assertTrue(self.walked_on())

    def test_a_STALE_retained_ref_from_an_EARLIER_cycle_cannot_satisfy_THIS_gate(self):
        """CHECK-UPDATE-ROOT-R9 finding 1, the unrepairable one.

        An earlier cycle's retained refs are lying around, pointing at objects that have nothing to
        do with this tip. Under the old fixed name they ANSWERED the gate - `git tag` failed because
        the name was taken, `rev-parse` succeeded on the stale object, and the rebase then rewrote
        this cycle's tip with nothing retaining it. The name carries the sha now, so the stale refs
        are simply other names: they are neither satisfied by, nor deleted for, nor overwritten by
        this cycle, and THIS tip really is retained before the rewrite.
        """
        repo, base, upstream = self._build()
        before = git(repo, "rev-parse", "arifi/main")
        git(repo, "tag", "arifi-pre-rebase", base)                    # the old fixed name
        git(repo, "tag", A.retained_ref_name(base), base)             # an earlier cycle's derived
        rc, argv, err = self._run(repo, base, upstream)
        self.assertEqual(0, rc, err)
        self.assertEqual(before, git(repo, "rev-parse", A.retained_ref_name(before)),
                         "THIS cycle's tip was not retained - the stale ref answered for it")
        self.assertEqual(upstream, git(repo, "rev-parse", "arifi/main~1"), "the rebase did not run")
        self.assertEqual(base, git(repo, "rev-parse", "arifi-pre-rebase"),
                         "an unrelated ref was moved")
        self.assertEqual(base, git(repo, "rev-parse", A.retained_ref_name(base)),
                         "an earlier cycle's retained ref was moved - its ledger rows read it")
        self.assertEqual(A.retained_ref_name(before), self._flag(argv, "--retained-ref"))
        self.assertTrue(self.walked_on())

    def test_a_COLLIDING_retained_name_FAILS_CLOSED_and_overwrites_NOTHING(self):
        """The name is taken, by another object. Deriving it from the tip makes this astronomically
        unlikely rather than impossible - a fabricated tag reaches it - and the answer is a stop,
        never a `tag -f`: the sequence must not rewrite history it is not retaining, and must not
        destroy whatever that ref was evidence for."""
        repo, base, upstream = self._build()
        before = git(repo, "rev-parse", "arifi/main")
        git(repo, "tag", A.retained_ref_name(before), base)     # the right NAME, the wrong OBJECT
        rc, argv, err = self._run(repo, base, upstream)
        self.assertEqual(1, rc, "a colliding retained name must fail closed: %s" % err)
        self.assertIn("exists at another object", err)
        self.assertEqual(before, git(repo, "rev-parse", "arifi/main"),
                         "the rebase rewrote a tip the retained ref does not name")
        self.assertEqual(base, git(repo, "rev-parse", A.retained_ref_name(before)),
                         "the colliding ref was overwritten - `tag -f` is never the repair")
        self.assertIsNone(argv, "replay-map ran against a ref that does not name the tip")
        self.assertFalse(self.walked_on())

    def test_a_MOVED_tip_STOPS_THE_SEQUENCE_before_anything_is_tagged(self):
        """`bump` captures the tip when it PRINTS. If the branch moves before the paste runs, the
        captured sha is not the tip that would be rewritten, and retaining it would retain the
        wrong line. The fence sits before the tag, so nothing is created either."""
        repo, base, upstream = self._build()
        before = git(repo, "rev-parse", "arifi/main")

        def move():
            git(repo, "branch", "-f", "arifi/main", base)

        rc, argv, err = self._run(repo, base, upstream, mutate=move)
        self.assertEqual(1, rc, err)
        self.assertIn("no longer", err)
        self.assertEqual(base, git(repo, "rev-parse", "arifi/main"), "the rebase ran anyway")
        self.assertEqual("", git(repo, "tag", "-l", A.retained_ref_name(before)),
                         "a ref was retained for a tip this checkout no longer has")
        self.assertIsNone(argv)
        self.assertFalse(self.walked_on())

    def test_a_FAILING_tag_STOPS_THE_SEQUENCE_carrying_ITS_status(self):
        """`git tag` may only be skipped when the ref is ALREADY there. When it is asked to create
        the ref and refuses, that status is the sequence's status - nothing below it runs."""
        repo, base, upstream = self._build()
        before = git(repo, "rev-parse", "arifi/main")
        shim = 'git() { for a in "$@"; do [ "$a" = tag ] && return 7; done; command git "$@"; }\n'
        rc, argv, _ = self._run(repo, base, upstream, git_shim=shim)
        self.assertEqual(7, rc, "the failed `tag` status must be PROPAGATED, not swallowed")
        self.assertEqual(before, git(repo, "rev-parse", "arifi/main"),
                         "the rebase rewrote the branch with no retained ref to carry the "
                         "registrations across - exactly the unrepairable case")
        self.assertIsNone(argv, "replay-map ran with no retained ref")
        self.assertFalse(self.walked_on())

    def test_a_LYING_tag_STOPS_THE_SEQUENCE_before_the_rebase(self):
        """What keeps the tag step from being trusted at all: a `tag` that reports success and
        creates nothing. The verify fence does not care WHY the ref is absent, or what any command
        claimed - only that the ref must resolve to the captured tip before anything is rewritten.
        This is the case a status check alone cannot catch."""
        repo, base, upstream = self._build()
        before = git(repo, "rev-parse", "arifi/main")
        shim = 'git() { for a in "$@"; do [ "$a" = tag ] && return 0; done; command git "$@"; }\n'
        rc, argv, err = self._run(repo, base, upstream, git_shim=shim)
        self.assertEqual(1, rc, "the verify fence refuses with 1 and nothing below it runs")
        self.assertIn("exists at another object", err)
        self.assertEqual("", git(repo, "tag", "-l", A.retained_ref_name(before)),
                         "the shim was supposed to create nothing; this case tests nothing")
        self.assertEqual(before, git(repo, "rev-parse", "arifi/main"), "the rebase ran anyway")
        self.assertIsNone(argv, "replay-map ran with no retained ref")
        self.assertFalse(self.walked_on())

    def test_an_UNRESOLVABLE_ref_PRINTS_NO_rewrite_at_all(self):
        """The other end of the same rule, and the reason it is the PRINTER's job: the retained ref
        is named after the tip, so a ref that names no commit could only produce a tag name with
        nothing on the end of it. The generator stops at the one step that is runnable instead -
        no switch, no tag, no rebase, nothing for an operator to paste."""
        repo, base, upstream = self._build()
        cfg = {"series_dir": "patches/series"}
        args = type("A", (), {"ref": "arifi/nope", "onto": upstream,
                              "i_have_read_bench_purity": False})()
        lines = A.bump_repair_sequence(repo.replace("\\", "/"), cfg, args, base)
        cmds = [l.strip() for l in lines if l.startswith(A.BUMP_STEP_PREFIX) and l.strip()]
        self.assertEqual(["python tools/arifi-sync/arifi_sync.py bump --onto %s --ref arifi/nope "
                          "|| exit $?" % upstream], cmds,
                         "an unresolvable ref printed a rewrite, or a tag named after nothing")
        self.assertNotIn(A.RETAINED_REF_PREFIX + "-\n", "\n".join(lines) + "\n")
        self.assertIn("names no commit", "\n".join(lines))


if __name__ == "__main__":
    unittest.main(verbosity=2)
