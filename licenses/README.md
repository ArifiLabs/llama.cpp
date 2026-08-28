# Retained license and notice inventory

This directory retains verbatim license or notice text copied from the audited
source tree. Do not edit, normalize, shorten, or regenerate these files.

| Destination | Copy verbatim from | Audit disposition |
|---|---|---|
| `llama.cpp-MIT.txt` | `<FORK-WORKSPACE>/src/llama-upstream-b10068/LICENSE` | llama.cpp MIT; retain copyright and permission notice |
| `powerinfer-MIT.txt` | `<FORK-WORKSPACE>/src/powerinfer/LICENSE` | PowerInfer root MIT |
| `smallthinker-MIT.txt` | `<FORK-WORKSPACE>/src/powerinfer/smallthinker/LICENSE` | PowerInfer `smallthinker` MIT |
| `rocmfpx-MIT.txt` | `<FORK-WORKSPACE>/src/rocmfpx-fork/LICENSE` | charlie12345/ROCmFPX MIT |
| `llama-cpp-turboquant-MIT.txt` | `<FORK-WORKSPACE>/src/llama-cpp-turboquant/LICENSE` | llama-cpp-turboquant MIT |
| `turboquant-plus-Apache-2.0.txt` | `<FORK-WORKSPACE>/src/turboquant_plus/LICENSE` | turboquant_plus Apache-2.0 |
| `turboquant-plus-NOTICE.txt` | `<FORK-WORKSPACE>/src/turboquant_plus/NOTICE` | turboquant_plus retained NOTICE |
| `tq3-MIT.txt` | `git show refs/remotes/tq3/main:LICENSE` (turbo-tan/llama.cpp-tq3) | tq3 MIT — the `TQ3_4S` family at ids 48-51 is ported from this tree. **Retained 2026-08-18 (lane-151); this was the one real gap.** |
| `prisml-MIT.txt` | `git show refs/remotes/prisml/prism:LICENSE` (PrismML-Eng/llama.cpp) | PrismML MIT. **Retained 2026-08-18 (lane-151) — see the correction below.** |
| `ciru-MIT.txt` | `git show refs/remotes/ciru/main:LICENSE` (ciru-ai/ROCmFPX) | ciru MIT. Registered source, no code carried yet; retained ahead of any ingest so the notice can never lag the code. |
| `buun-MIT.txt` | `git show refs/remotes/buun/master:LICENSE` (spiritbuun/buun-llama-cpp) | buun MIT. **Retained 2026-08-27; this was a GAP** — series patch `0394` ingested buun's DFlash2 controller on 2026-08-21 with no notice row, so the notice lagged the code by six days. Byte-identical to the upstream MIT text apart from LF termination. |

`../LICENSE` is also the verbatim copy of upstream llama.cpp’s MIT license.

The three files added on 2026-08-18 are extracted with `git show` from the fetched remotes of this
very repository, so their provenance is a ref, not a local checkout. They are 1078 bytes against the
1099 of the older copies: the same MIT text, LF-terminated as git stores it rather than CRLF. Verbatim
means verbatim, so they are not re-normalized.

## Audit, 2026-08-18 (lane-151) — every source this fork carries code or credit from

Derived from the distinct `Taken-from:` values across every fork commit on `arifi/main`, then widened
by hand. **The widening is not cosmetic and is stated so the table is not read as a purely mechanical
derivation:** two rows below are NOT in git's parsed trailer set. `turbo-tan/llama.cpp-tq3` appears in
commit `01b3065fe`'s *subject* as "Taken-from turbo-tan/llama.cpp-tq3@58ad80ffb" — no colon, so git
does not parse it as a trailer, and a tool that trusted the trailer block alone would have missed the
one source with a real license GAP. `turboquant_plus` appears in no trailer at all; its notice is
retained anyway because its Apache-2.0 NOTICE requires it. A trailer sweep is a starting point, not
the audit.

| source cited by a trailer | notice retained | disposition |
|---|---|---|
| `ggml-org/llama.cpp` (upstream) | yes — `llama.cpp-MIT.txt` + root `LICENSE` | complete |
| `Tiiny-AI/PowerInfer` (+ `smallthinker`) | yes — two MIT files | complete |
| `charlie12345/ROCmFPX` | yes — `rocmfpx-MIT.txt` | complete |
| `llama-cpp-turboquant` | yes — MIT | complete |
| `turboquant_plus` | yes — Apache-2.0 + NOTICE | complete |
| `PrismML-Eng/llama.cpp` | **yes — `prisml-MIT.txt`** | **CLOSED 2026-08-18.** See the correction below: the "Phase 0 observed no LICENSE" finding was about the retained *release material*, and it was carried forward for a year as though it were a fact about the project. `refs/remotes/prisml/prism:LICENSE` exists and is MIT. |
| `turbo-tan/llama.cpp-tq3` | **yes — `tq3-MIT.txt`** | **CLOSED 2026-08-18.** Code IS ported (`58ad80ffb`, the `TQ3_4S` family at ids 48-51), and the license is now retained from `refs/remotes/tq3/main:LICENSE`. |
| `ciru-ai/ROCmFPX` | **yes — `ciru-MIT.txt`** | registered source, no code carried yet; the notice is retained ahead of any ingest so it can never lag the code. |
| `thecodacus/llama.cpp` | **yes — `thecodacus-MIT.txt`** | **CLOSED 2026-08-18** (President's correction, retained in `d40e42fae`; row landed lane-152). The last open attribution. The old text of this row is preserved in the correction below because the error in it is instructive. |

### Correction, 2026-08-18 (lane-151): a false absence that stood for months

Two of these rows said "no license" for a year, and one of them said it in this very file's prose.
Both were wrong, and the reason they were wrong is worth more than the fix: **the absence was checked
against the wrong artifact.** Phase 0 looked at retained *release material* and local checkouts, found
no `LICENSE`, and wrote down "there is none". Meanwhile this repository has had `prisml`, `tq3`,
`ciru`, `rocmfpx`, `powerinfer`, `turboquant` and `tqplus` configured as git remotes with their
objects fetched the whole time — every one of those licenses was one `git show` away.

Lane-151 repeated the same mistake with tq3 in the same breath, writing "no clone remains on the
estate to look at" and filing it as an unclosable gap, when `git show refs/remotes/tq3/main:LICENSE`
answers in a second. **An absence claim is only as good as the place you looked, and "I could not
find it" is not the same statement as "it does not exist."**

Audited all seven registered sources at once, since the same command answers for each:
`prisml`, `ciru`, `rocmfpx`, `powerinfer`, `turboquant`, `tqplus` all carry a `LICENSE` at their
fetched head; `tqplus` also carries a `NOTICE`. Every one is now retained.

**SUPERSEDED IN PART, 2026-08-18 — kept because the reasoning still binds where it applies.** Phase 0
observed no source `LICENSE` or `NOTICE` file in the retained PrismML release material, and no license
grant inside the captured thecodacus patch artifacts. The PrismML half is now closed: `prisml-MIT.txt`
is retained from `refs/remotes/prisml/prism:LICENSE`, and the correction above explains why the
original finding was true of the material examined and false of the project. The thecodacus half
**FELL THE SAME DAY, to the same mistake one level out.** Its old text read: *"there is no
`thecodacus` remote on this repository — the three prefetch patches were captured as patch artifacts,
and those artifacts carry no license grant. There is nothing to `git show`. Documented, not
manufactured."* Every clause of that is true, and the conclusion drawn from it is still wrong: the
correction above says an absence claim is only as good as the place you looked, and this row then
looked in exactly one place — this repository — and stopped. **NO LOCAL REMOTE IS NOT NO SOURCE.**
The project is public; its LICENSE is a fetch away and needs no remote configured here. The President
made that correction and `thecodacus-MIT.txt` is retained (`d40e42fae`). All seven registered sources
plus thecodacus are now closed; this directory still contains no fabricated license text.

A 2026-07-22 project decision authorizes their inclusion with maximal attribution and provenance; it
does not authorize inventing a license text. Therefore this directory still contains no fabricated
`thecodacus-*` license file, and never will.
