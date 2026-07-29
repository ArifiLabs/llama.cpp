# Commit trailers

Every non-trivial commit in this fork carries provenance in its message. `arifi-sync provenance`
audits it, and `--strict` uses **git's own trailer parser** — the same one CI and any downstream
tooling sees. This page exists because there is one rule that is easy to break and gives no
warning when you do.

## The three trailers

| Trailer | Required on | Value |
|---|---|---|
| `Taken-from:` | anything ported from another project | repository and commit, e.g. `PrismML-Eng/llama.cpp@984bf9723` |
| `Origin:` | everything | which piece of work produced it |
| `Measured-effect:` | anything that could change behaviour | what was measured, on what hardware — or the literal word `UNMEASURED` |

`UNMEASURED` is a legal value and preferred over a guess. A *missing* trailer is the problem; an
honest one that says nothing was measured is not.

## The rule that bites: wrapped values

Git ends the trailer block at the **first line that is not trailer-shaped**. A continuation line
starting in column 0 therefore does not merely break its own trailer — it invalidates **every
trailer in the block, including the ones above it**. The commit still reads perfectly to a human,
which is exactly why this goes unnoticed.

```
WRONG — the block ends at "reference machine", so NOTHING here parses

    Taken-from: ggml-org/llama.cpp@e9fa0781f
    Origin: some work unit
    Measured-effect: 12.5 tok/s on the
    reference machine, 3 rolls, ranges disjoint.
```

```
RIGHT — the continuation is indented, so the block is intact

    Taken-from: ggml-org/llama.cpp@e9fa0781f
    Origin: some work unit
    Measured-effect: 12.5 tok/s on the
      reference machine, 3 rolls, ranges disjoint.
```

Two habits keep this correct:

1. **Indent continuation lines** by at least one space.
2. **Put the trailer block last.** Prose *after* the trailers ends the block just as effectively
   as an unindented continuation.

## Checking before you commit

```bash
python tools/arifi-sync/arifi_sync.py provenance --strict
```

`provenance` (without `--strict`) accepts a trailer token anywhere in the message, so it passes
on commits that machines cannot read. `--strict` is the honest check. If a commit shows up under
`LOOSE-ONLY`, the message is fine for a human and invisible to tooling — reword the trailer block
per the rule above before it lands.

A `commit-msg` hook is available that performs the same check locally:

```bash
git config core.hooksPath .githooks
```

## Known gap

Twelve commits already in history are `LOOSE-ONLY` — their provenance is present and correct for a
reader but does not parse. They are not being rewritten: the series is verified to replay to a
byte-identical tree, and rewriting messages to satisfy a parser would invalidate that verification
for no gain in the information itself. New commits are expected to parse.
