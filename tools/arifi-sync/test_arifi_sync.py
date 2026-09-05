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
        os.remove(self.manifest_path)
        self.assertEqual(2, self._check(self._incoming({"docs/x.md": "unrelated\n"})))

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
        with open(self.manifest, "w", encoding="utf-8") as fh:
            json.dump({"version": 1, "wins": [{
                "id": "test-win", "status": "active", "mechanism": "the fast path",
                "commit": self.win_sha, "protected_paths": ["ggml/kernel.c"],
                "anchors": ["ARIFI_FAST_PATH"], "evidence": ["commit:" + self.win_sha],
                "quality_gate": "byte-identical output", "workload": "the seat serve line",
                "measured_effect": "-12% per dispatch"}]}, fh)

    def tearDown(self):
        self.r.close()

    def _baseline(self, sha, disposition="NOT A MECHANISM - fixture", tip="__main__"):
        """`tip` defaults to whatever `main` is RIGHT NOW, which is what a genuine baseline looks
        like: it was recorded at a tip, and every row it dispositions predates that tip. Pass an
        explicit tip (or `None`) to model the tampering shapes."""
        p = os.path.join(self.r.path, "tools", "arifi-sync", "protected-win-baseline.json")
        if tip == "__main__":
            tip = git(self.r.path, "rev-parse", "main").strip()
        doc = {"version": 1, "commits": [
            {"sha": sha, "subject": "fixture", "disposition": disposition}]}
        if tip is not None:
            doc["tip_when_recorded"] = tip
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)

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
        with open(self.manifest, "w", encoding="utf-8") as fh:
            json.dump({"version": 1, "wins": [{
                "id": "test-win", "status": "active", "mechanism": "the fast path",
                "commit": self.win_sha, "protected_paths": ["ggml/kernel.c"],
                "anchors": ["ARIFI_FAST_PATH"], "evidence": ["commit:" + self.win_sha],
                "quality_gate": "byte-identical output", "workload": "the seat serve line",
                "measured_effect": "-12% per dispatch"}]}, fh)
        self.bpath = os.path.join(self.r.path, "tools", "arifi-sync",
                                  "protected-win-baseline.json")

    def tearDown(self):
        self.r.close()

    def _write(self, rows, tip):
        doc = {"version": 1, "commits": rows}
        if tip is not None:
            doc["tip_when_recorded"] = tip
        with open(self.bpath, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)

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


if __name__ == "__main__":
    unittest.main(verbosity=2)
