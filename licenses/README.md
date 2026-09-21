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
| `thecodacus-MIT.txt` | `https://github.com/thecodacus/llama.cpp` `LICENSE` at its public head — no remote is configured here, and none is needed (see the correction below) | thecodacus MIT. **Retained 2026-08-18 on the President's correction, in `19fd8719b`. Row added 2026-08-29 (gap G10)** — its provenance-of-copy had been recorded only in the prose below, never in this table. |
| `zuijdwijk-MIT.txt` | `git show refs/remotes/zuijdwijk/master:LICENSE` (LaurentZuijdwijk/llama.cpp) | zuijdwijk MIT (llama.cpp lineage, "The ggml authors"). **Retained 2026-08-29 (gap G11)** — the remote was configured and fetched but registered nowhere. **CODE IS CARRIED** (`e146c1175`, `c1440b85d` on lane-166 branches, both trailered `Taken-from: LaurentZuijdwijk/llama.cpp`); tracked source in `sources.json` `remotes`, pinned `f97c0e6fe`. |
| `buun-MIT.txt` | `git show refs/remotes/buun/master:LICENSE` (spiritbuun/buun-llama-cpp) | buun MIT. **Retained 2026-08-27; this was a GAP** — series patch `0394` ingested buun's DFlash2 controller on 2026-08-21 with no notice row, so the notice lagged the code by six days. Byte-identical to the upstream MIT text apart from LF termination. |
| `turboq-mtp-MIT.txt` | `LICENSE` of `https://github.com/jtrefon/llama.cpp-turboq-mtp` at `6a02d0494` (shallow clone `cache/r18-turboq-src`, 1099 bytes, "The ggml authors") | jtrefon MIT (llama.cpp lineage). **Retained 2026-09-07 (lane-212, WI-1694) in the same commit that carries the code**: the TBQ3_0/TBQ4_0 codec (`ggml/src/ggml-tbq-quant.c`, tables) at ids 58/59 is ported from this tree. Tracked source in `sources.json` `remotes` as `turboq`, pinned `6a02d0494`. |

| `S-X8-MarlaLabs-LICENSE.txt` | `https://huggingface.co/marlalabsAI/Qwen3.8-27B-SX8/resolve/main/LICENSE`, fetched 2026-09-21 | Apache-2.0 as published by the author, carrying his own copyright line *"Copyright 2026 Martí Vidal Leandro"*. Retained for **S-X8 v4.3** (MarlaLabs; DOI 10.5281/zenodo.21922640, v1.0 2026-08-13). **This was the one real licence gap at `94461e770`; closed 2026-09-21.** 11,350 bytes, sha256 `ad4aa936adb43842a3f34d16797acdd39ba4f523e5b02b9ea4fe48b1962d3c74`. |
| `S-X8-MarlaLabs-NOTICE.txt` | `https://huggingface.co/marlalabsAI/Qwen3.8-27B-SX8/resolve/main/NOTICE`, fetched 2026-09-21 | The author's own NOTICE, retained verbatim because **Apache-2.0 §4(d) requires it to travel**. It also carries the base model's attribution (Qwen3.8-27B, Copyright (C) 2026 Qwen Team, Alibaba Group, Apache-2.0). 1,278 bytes, sha256 `20ea964a519ef0b04ce51beb2ae0cf601a82775cd7e4022c15ec0434a94ca13a`. **Not scrubbed** — it contains the author's contact address and a required NOTICE is not ours to edit. |

`../LICENSE` is also the verbatim copy of upstream llama.cpp’s MIT license.
`../NOTICE` names every component above with its licence and source; it was added 2026-09-21 in
the same release-prep commit as `Apache-2.0.txt`.

### S-X8 v4.3 — Apache-2.0, retained 2026-09-21 (release prep)

The tree implements `GGML_TYPE_SX8` (57) across 37 source files. The **licence is established, not
assumed**: `research/local-inference/lane-evidence/2026-09-15-sx8-paper-docs/PROVENANCE.md:3` reads
*"All files Apache-2.0 (author Martí Vidal Leandro, MarlaLabs)"* over ten files, each banked with a
source URL and a sha256 — the papers, `SX8_FLASH_V4_3_SPEC.md`, the container scripts, the reference
CUDA kernels (`sx8_decode1_v3.cu`, `sx8_embed_v44.cu`) and the author's own `llama-cpp-sx8.patch`.
Commit `948aab9c6` carries `Taken-from: MarlaLabsAI/sx8-quantization docs/S-X-METHODOLOGY.md section
2 (per-8 affine sub-block decomposition), Apache-2.0`.

**CLOSED 2026-09-21 — the licensor's own files are now retained.** This row previously read OWED,
because the 2026-09-15 capture took the papers, the scripts, the kernels and the patch but no
LICENSE blob, and the stand-in was the canonical Apache-2.0 text. That gap is closed by fetching
the author's own files:

| File | Source | Fetched | Bytes | sha256 |
|---|---|---|---|---|
| `S-X8-MarlaLabs-LICENSE.txt` | `https://huggingface.co/marlalabsAI/Qwen3.8-27B-SX8/resolve/main/LICENSE` | 2026-09-21 | 11,350 | `ad4aa936adb43842a3f34d16797acdd39ba4f523e5b02b9ea4fe48b1962d3c74` |
| `S-X8-MarlaLabs-NOTICE.txt` | `https://huggingface.co/marlalabsAI/Qwen3.8-27B-SX8/resolve/main/NOTICE` | 2026-09-21 | 1,278 | `20ea964a519ef0b04ce51beb2ae0cf601a82775cd7e4022c15ec0434a94ca13a` |

The retained `LICENSE` carries the author's own copyright line — *"Copyright 2026 Martí Vidal
Leandro"* — which is exactly what a stand-in could not supply and why this directory refused to
invent one. **Retaining the author's NOTICE is not optional**: Apache-2.0 §4(d) requires the NOTICE
of a licensed work to travel with any derivative distribution. It also names the base model's own
licence (Qwen3.8-27B, Apache-2.0, Qwen Team / Alibaba Group), which now travels with this tree too.

Independently confirmed at the DOI: the Zenodo record for 10.5281/zenodo.21922640 carries
`"license": {"id": "apache2.0"}` in its own metadata and names Vidal Leandro, Martí (MarlaLabs) as
rights holder.

`Apache-2.0.txt` — the licensor-neutral stand-in this directory added on 2026-09-21 before the fetch
succeeded — is **removed**, because the author's own file now does that job and two copies of the
same terms invite the reader to wonder which one governs.

The author's NOTICE contains his contact address. It is **deliberately excluded** from this
repository's path/identity scrub: a NOTICE that Apache-2.0 requires to travel verbatim is not ours
to edit.

**No per-file headers were added.** Apache-2.0 §4(a)–(d) requires the licence, the NOTICE contents
and any retained attribution notices to travel with the derivative work; it does not require a
header on every file. The S-X8 sources captured carry no header of the author's to retain, and a
header composed here would be our text over his name.

### Inherited from upstream, not retained by this fork

| File | Origin | Disposition |
|---|---|---|
| `LICENSE-jsonhpp` | **upstream llama.cpp's own file**, present at `licenses/LICENSE-jsonhpp` on `upstream/master`; it arrived with the tree in `bd3f59f81` (`cmake : enable curl by default (#12761)`) | nlohmann/json MIT (Niels Lohmann), covering upstream's vendored `vendor/nlohmann/json.hpp` + `json_fwd.hpp`. **Not a fork retention** — no fork commit copied it and no fork take depends on it. Row added 2026-08-29 (gap G9), which flagged it as a retained notice with no row; the correction is that it is inherited, not retained, and the two tables above are for fork retentions only. |

The three files added on 2026-08-18 are extracted with `git show` from the fetched remotes of this
very repository, so their provenance is a ref, not a local checkout. They are 1078 bytes against the
1099 of the older copies: the same MIT text, LF-terminated as git stores it rather than CRLF. Verbatim
means verbatim, so they are not re-normalized.

## Audit, 2026-08-18 (lane-151) — every source this fork carries code or credit from

Derived from the distinct `Taken-from:` values across every fork commit on `arifi/main`, then widened
by hand. **The widening is not cosmetic and is stated so the table is not read as a purely mechanical
derivation:** two rows below are NOT in git's parsed trailer set. `turbo-tan/llama.cpp-tq3` appears in
commit `4c620f3d1`'s *subject* as "Taken-from turbo-tan/llama.cpp-tq3@58ad80ffb" — no colon, so git
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
| `thecodacus/llama.cpp` | **yes — `thecodacus-MIT.txt`** | **CLOSED 2026-08-18** (President's correction, retained in `19fd8719b`; row landed lane-152). The last open attribution. The old text of this row is preserved in the correction below because the error in it is instructive. |

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
original finding was true of the material examined and false of the project. (Both shas in this file
were re-keyed 2026-08-29 after the b10524→b10636→b10680 base moves rewrote every fork sha: the tq3
take was `01b3065fe`, the thecodacus retention `d40e42fae`. Neither pre-rebase object is on any
branch — a reader who cloned the published fork could not have verified either claim. Gap G5.)
The thecodacus half
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

### `zuijdwijk` — an unregistered source that turned out to be a live one (gap G11, closed same day)

`zuijdwijk` (`LaurentZuijdwijk/llama.cpp`) was a configured, fetched remote that appeared in **no**
governed artifact: not `sources.json`, not this file, not any `Taken-from:` value on `arifi/main`.
It was first registered here as "no code known to be carried". **That was wrong within hours, and
the way it was wrong is the lesson:** two lane-166 commits carry `Taken-from:
LaurentZuijdwijk/llama.cpp` in full — `e146c1175` (matvec `NUM_COLS>4` VGPR spill, from `211abf6a9`)
and `c1440b85d` (coopmat LDS stride pad, from `f97c0e6fe`). The sweep that said "no code carried"
read `arifi/main`, and **the takes were on unmerged branches.** A source-registration sweep that
only reads the mainline misses every take still in flight. It is now a tracked source in
`sources.json` `remotes`, pinned `f97c0e6fe`.

**The content diff the audit called owed has been run, and `arifi/main` is clean.** Patch-id
intersection of all 78 `zuijdwijk` non-upstream commits against all 478 commits of
`d7bd3bfca..arifi/main`: **zero** verbatim matches. A distinctive-symbol scan (every `GGML_*` /
`LLAMA_*` / `--long-opt` token those 78 commits add, minus everything already in `upstream/master`)
leaves 46 tokens present in our tree — and all 46 are **ROCmFPX** symbols, reaching us through
`charlie12345/ROCmFPX`, a licensed and registered source which is also `zuijdwijk`'s own upstream on
its `qwen4exp-rocmfpx` branch. Shared ancestry, not zuijdwijk-originated content. **Nothing
unattributed from this source is on the published branch**, and the notice was retained before the
takes ever merge.

`turbomerge` was the other unregistered remote and is a different thing entirely: it is a local path
inside our own `src/`, authored by this estate (`hq`, `m`), a fork-owned merge workspace. It is
registered in `sources.json` as a workspace and needs **no** third-party notice.
