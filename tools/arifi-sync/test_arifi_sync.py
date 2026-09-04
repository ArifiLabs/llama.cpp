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
import hashlib
import json
import os
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
        self.path = tempfile.mkdtemp(prefix="arifi-test-")
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

    def commit(self, subject, files, trailers=True, ref=None):
        for rel, text in files.items():
            self.write(rel, text)
        git(self.path, "add", "--", *files)
        msg = subject
        if trailers:
            msg += "\n\nOrigin: ArifiLabs (native)\nMeasured-effect: test fixture\n"
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

    def close(self):
        shutil.rmtree(self.path, ignore_errors=True)


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
        with open(self.manifest_path, "w", encoding="utf-8") as fh:
            json.dump({"version": 1, "wins": list(entries)}, fh)

    def _write_resolutions(self, *rows):
        p = os.path.join(self.r.path, "tools", "arifi-sync", "protected-win-resolutions.json")
        with open(p, "w", encoding="utf-8") as fh:
            json.dump({"resolutions": list(rows)}, fh)

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

    def test_incomplete_active_entry_fails_closed(self):
        self._write_manifest(self._entry(quality_gate=""))
        problems = A.validate_protected_wins(self.r.path, "main")
        self.assertTrue(any("INCOMPLETE" in p for p in problems), problems)
        self.assertEqual(1, self._check(self._incoming({"docs/x.md": "unrelated\n"})))

    def test_missing_manifest_fails_closed(self):
        os.remove(self.manifest_path)
        self.assertEqual(2, self._check(self._incoming({"docs/x.md": "unrelated\n"})))

    def test_stale_anchor_fails_validation(self):
        """A win whose anchor no longer exists is guarding nothing, and must say so."""
        self._write_manifest(self._entry(anchors=["ANCHOR_THAT_WAS_RENAMED"]))
        problems = A.validate_protected_wins(self.r.path, "main")
        self.assertTrue(any("anchor" in p for p in problems), problems)

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


if __name__ == "__main__":
    unittest.main(verbosity=2)
