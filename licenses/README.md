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

`../LICENSE` is also the verbatim copy of upstream llama.cpp’s MIT license.

## Audit, 2026-08-18 (lane-151) — every source cited by a `Taken-from:` trailer

Derived by taking the distinct `Taken-from:` trailer values across every fork commit on `arifi/main`
and asking, for each, whether a notice is retained above.

| source cited by a trailer | notice retained | disposition |
|---|---|---|
| `ggml-org/llama.cpp` (upstream) | yes — `llama.cpp-MIT.txt` + root `LICENSE` | complete |
| `Tiiny-AI/PowerInfer` (+ `smallthinker`) | yes — two MIT files | complete |
| `charlie12345/ROCmFPX` | yes — `rocmfpx-MIT.txt` | complete |
| `llama-cpp-turboquant` | yes — MIT | complete |
| `turboquant_plus` | yes — Apache-2.0 + NOTICE | complete |
| `PrismML-Eng/llama.cpp` | **no** | Phase 0 observed no `LICENSE`/`NOTICE` in the retained release material. Documented, not manufactured. |
| `thecodacus/llama.cpp` | **no** | no license grant inside the captured patch artifacts. Documented, not manufactured. |
| `turbo-tan/llama.cpp-tq3` | **no — GAP** | code WAS ported (`58ad80ffb`, the `TQ3_4S` family at ids 48-51). Unlike the two rows above this is not a "we looked and there was none" finding: no one looked, and no clone remains on the estate to look at. **Retaining tq3's license is a hard precondition of publishing this repository.** |

`ciru-ai/ROCmFPX` is a registered source but appears in no trailer and has contributed no code, so
no notice is owed yet; if it ever contributes, its license is retained first.

Phase 0 observed no source `LICENSE` or `NOTICE` file in the retained PrismML
release material and no license grant inside the captured thecodacus patch
artifacts. A 2026-07-22 project decision authorizes their inclusion with
maximal attribution and provenance; it does not authorize inventing a license
text. Therefore this directory contains no fabricated `prisml-*` or
`thecodacus-*` license file.
