"""Reword the post-x1i commits of integ so each carries Origin: + Measured-effect: (B1c, HQ82).

Run by lane 298 AT MERGE FREEZE. It never touches the source repo's refs except one new branch:
  1. clone --no-local --single-branch <branch> into <work> (a scratch copy; the fork is only read)
  2. replay c23bd8cc5e..tip with `git commit-tree`: same tree, author, committer and dates; the
     message of every row in post-x1i-trailers.tsv gets the two trailers via `git interpret-trailers`
     (merged into an existing trailer block, never duplicating a key that already parses)
  3. re-key every full 40-hex sha of a rewritten commit in tools/arifi-sync/**/*.json at the new tip,
     and commit that as one ledger commit (commits under c23bd8cc5e keep their shas: none is touched)
  4. write <work>/reword-commit-map.tsv and fetch the result into <src> as refs/heads/<out-branch>
Then lane 298, in its integ worktree:  git reset --keep <out-branch>   and run series regen.
Usage: python reword.py <src-repo> <branch> <work-dir> [--out-branch release-prep/reword-out] [--no-fetch (dry run)]
Refuses: a merge in range, a table sha outside the range, a dirty or existing work dir."""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TSV = os.path.join(HERE, "post-x1i-trailers.tsv")
X1I_PREFIX = "c23bd8cc5e"
LEDGER_DIR = "tools/arifi-sync"
NOTE_SUBJECT = "arifi-sync: re-key the sha-pinned ledgers after the post-x1i trailer reword"


def run(repo, *a, env=None, inp=None):
    r = subprocess.run(["git", "-C", repo, *a], capture_output=True, text=True, encoding="utf-8",
                       env=env, input=inp)
    if r.returncode:
        sys.exit("git %s failed:\n%s" % (" ".join(a), r.stderr))
    return r.stdout


def load_table():
    rows = {}
    for line in open(TSV, encoding="utf-8").read().splitlines()[1:]:
        c = line.split("\t")
        rows[c[0]] = (c[3], c[4])
    return rows


def reword(work, msg, origin, effect, tr):
    args = ["interpret-trailers", "--if-exists", "doNothing"]
    if not (tr.get("Origin") or tr.get("Taken-from")):
        args += ["--trailer", "Origin: " + origin]
    if not tr.get("Measured-effect"):
        args += ["--trailer", "Measured-effect: " + effect]
    return run(work, *args, inp=msg)


def trailers(work, sha):
    return {k: run(work, "log", "-1", "--format=%%(trailers:key=%s,valueonly)" % k, sha).strip()
            for k in ("Origin", "Taken-from", "Measured-effect")}


def main():
    a = sys.argv[1:]
    if len(a) < 3:
        sys.exit(__doc__)
    src, branch, work = a[0], a[1], os.path.abspath(a[2])
    out_branch = a[a.index("--out-branch") + 1] if "--out-branch" in a else "release-prep/reword-out"
    if os.path.exists(work):
        sys.exit("work dir exists: %s (use a fresh path)" % work)
    subprocess.run(["git", "clone", "-q", "--no-local", "--single-branch", "-b", branch, src, work], check=True)
    x1i = run(work, "rev-parse", "--verify", X1I_PREFIX + "^{commit}").strip()
    rng = "%s..HEAD" % x1i
    if run(work, "rev-list", "--merges", rng).strip():
        sys.exit("REFUSED: merge commit(s) in %s" % rng)
    shas = run(work, "rev-list", "--reverse", "--topo-order", rng).split()
    table = load_table()
    outside = sorted(set(table) - set(shas))
    if outside:
        sys.exit("REFUSED: %d table sha(s) not in %s: %s" % (len(outside), rng, outside[:5]))

    m, reworded, skipped = {}, 0, []
    tmp = os.path.join(work, ".git", "reword-msg.txt")
    for sha in shas:
        meta = run(work, "log", "-1", "--format=%an%x00%ae%x00%ad%x00%cn%x00%ce%x00%cd%x00%T%x00%P",
                   "--date=raw", sha).rstrip("\n").split("\x00")
        an, ae, ad, cn, ce, cd, tree, parents = meta
        new_parents = [m.get(p, p) for p in parents.split()]
        msg = run(work, "log", "-1", "--format=%B", sha)
        if sha in table:
            tr = trailers(work, sha)
            if (tr["Origin"] or tr["Taken-from"]) and tr["Measured-effect"]:
                skipped.append(sha)
            else:
                msg = reword(work, msg, *table[sha], tr)
                reworded += 1
        if new_parents == parents.split() and sha not in table:
            m[sha] = sha
            continue
        env = dict(os.environ, GIT_AUTHOR_NAME=an, GIT_AUTHOR_EMAIL=ae, GIT_AUTHOR_DATE=ad,
                   GIT_COMMITTER_NAME=cn, GIT_COMMITTER_EMAIL=ce, GIT_COMMITTER_DATE=cd)
        with open(tmp, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(msg)
        pargs = sum((["-p", p] for p in new_parents), [])
        m[sha] = run(work, "commit-tree", tree, *pargs, "-F", tmp, env=env).strip()
    run(work, "reset", "-q", "--hard", m[shas[-1]])

    # re-key every rewritten full sha in the ledgers at the new tip
    changed = {o: n for o, n in m.items() if o != n}
    touched = []
    for root, _, files in os.walk(os.path.join(work, LEDGER_DIR)):
        for f in files:
            if not f.endswith(".json"):
                continue
            p = os.path.join(root, f)
            s = open(p, encoding="utf-8").read()
            t = re.sub(r"\b[0-9a-f]{40}\b", lambda k: changed.get(k.group(0), k.group(0)), s)
            if t != s:
                with open(p, "w", encoding="utf-8", newline="") as fh:
                    fh.write(t)
                touched.append(os.path.relpath(p, work).replace("\\", "/"))
    if touched:
        run(work, "add", "--", *touched)
        body = ("%s\n\nThe trailer reword (reword.py, B1c) replayed %s..%s with the same trees and new\n"
                "messages, so %d commit shas changed. Every full sha of a rewritten commit in the ledgers\n"
                "below is replaced by its new sha (map: reword-commit-map.tsv). Abbreviated shas in prose\n"
                "fields are not rewritten. Commits under c23bd8cc5e are untouched.\n%s\n\n"
                "Origin: ArifiLabs (native) - release engineering for this fork; no upstream origin exists.\n"
                "Origin: release-prep (HQ82)\n"
                "Measured-effect: UNMEASURED - ledger keys only; no inference code path is touched.\n"
                % (NOTE_SUBJECT, x1i[:10], shas[-1][:10], len(changed),
                   "\n".join("  " + t for t in touched)))
        with open(tmp, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(body)
        who = run(work, "log", "-1", "--format=%an%x00%ae%x00%cn%x00%ce", "HEAD").strip().split("\x00")
        env = dict(os.environ, GIT_AUTHOR_NAME=who[0], GIT_AUTHOR_EMAIL=who[1],
                   GIT_COMMITTER_NAME=who[2], GIT_COMMITTER_EMAIL=who[3])
        run(work, "commit", "-q", "-F", tmp, "--", *touched, env=env)
    with open(os.path.join(work, "reword-commit-map.tsv"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("old\tnew\n")
        for o in shas:
            fh.write("%s\t%s\n" % (o, m[o]))
    tip = run(work, "rev-parse", "HEAD").strip()
    abbrev_left = []
    for t in touched or []:
        s = open(os.path.join(work, t), encoding="utf-8").read()
        abbrev_left += [o[:10] for o in changed if o[:10] in s]
    if "--no-fetch" not in a:
        run(src, "fetch", "-q", work, "+HEAD:refs/heads/" + out_branch)
    print("range %s..%s  commits %d  reworded %d  already-complete %d  sha-changed %d" %
          (x1i[:10], shas[-1][:10], len(shas), reworded, len(skipped), len(changed)))
    print("ledgers re-keyed: %s" % (touched or "none"))
    print("abbreviated (10-char) old shas still in re-keyed ledgers: %d %s" % (len(abbrev_left), sorted(set(abbrev_left))[:8]))
    print("new tip %s -> %s refs/heads/%s" % (tip, src, out_branch))


if __name__ == "__main__":
    main()
