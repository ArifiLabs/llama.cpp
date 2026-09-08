# GGML type-ID allocation — the fork-wide contract

This document owns every `enum ggml_type` value the ArifiLabs fork will ever define, and the rule
for deciding whether a new format needs a per-file gate, a retag tool, or neither.

It exists because four merge slots (ROCmFP weight formats, HIP backend deltas, NVFP4->ROCmFP4 remap,
TurboQuant weight quants) were blocked on a type-ID collision. **The collision, as previously stated,
does not exist.** Section 1 shows why, from source. The rest of the document is the contract that
replaces it.

Everything below was verified against the actual enums, block structs, `type_traits` tables and
GGUF writer registries of each fork on 2026-07-24. Source refs are named per claim so any of it can
be re-checked without re-reading the trees.

---

## 1. What the collision actually is

### 1.1 The premise that was wrong

`trial-evidence/110/SOL-TASK-gguf-type-id-allocation.md` states the problem as:

> two upstream forks assign **different type-IDs to the same wire format**, and both collide with ours

**This is false, and it is false twice.** Both halves were checked against the block structs.

**(a) ROCmFPX 105/106 are not TurboQuant's 43/44.** They share the names `TURBO3_0`/`TURBO4_0` and
nothing else. ROCmFPX ported an early revision; TurboQuant has since changed its block size from 32
to 128 and restructured the packing.

| | ROCmFPX `rocmfpx/main` (`3edc3d31e`) | TurboQuant `llama-cpp-turboquant` (`a33ef00b1`) |
|---|---|---|
| `TURBO3_0` id | 105 | 43 |
| block size | `TURBO3_BLOCK_SIZE 32` | `QK_TURBO3 128` |
| struct | `ggml_half d; uint8_t qs[12];` = **14 B / 32 values** | `ggml_half norm; uint8_t qs[32]; uint8_t signs[16];` = **50 B / 128 values** |
| packing | contiguous 3-bit indices | split bit-planes: low 2 bits in `qs`, high bit in `signs` |
| `TURBO4_0` id | 106 | 44 |
| struct | `ggml_half d; uint8_t qs[16];` = **18 B / 32 values** | `norm + rnorm + qs[64]` = **68 B / 128 values** |

Refs: `rocmfpx/main:ggml/src/ggml-common.h:219-237`; `llama-cpp-turboquant:ggml/src/ggml-common.h:284-345`.

Note that TurboQuant's own in-source comments (`// 14 bytes total`, `// 10 bytes total`) are **stale** —
they date from when `QK_TURBO3` was 32. The `static_assert` on the following line computes the real
size. Do not quote those comments as geometry.

**(b) ROCmFPX 107 is its own format, not an alias of anything.** Sol's partial finding was correct
and is confirmed: `GGML_TYPE_Q2_0_ROCMFPX = 107` maps to `block_rocmfp2` —
`uint8_t qs[QS_ROCMFP2]; uint8_t e[2];` with `QS_ROCMFP2 = (32*2)/8 = 8`, so **10 bytes over 32 values**,
2-bit codes with dual UE4M3 half-block scales. Refs: `rocmfpx/main:ggml/rocmfpx/rocmfpx.h:13-41`,
`ggml/src/ggml.c:711-718`.

### 1.2 The collision that is real, and its actual size

The only ids that genuinely overlap ours are TurboQuant's **42 / 43 / 44** against our
`Q2_0` / `Q2_0_G128` / `COUNT`. Those three TurboQuant types are `TURBO2_0`, `TURBO3_0`, `TURBO4_0` —
**KV-cache types that are never written to a GGUF file.**

The proof is each fork's own GGUF writer registry, `gguf-py/gguf/constants.py::GGMLQuantizationType`,
which is the exhaustive list of type-ids their tooling can emit into a file:

- **TurboQuant** lists `TQ3_1S = 45` and `TQ4_1S = 46`. It lists **no** `TURBO2_0`/`TURBO3_0`/`TURBO4_0`.
  Its `LLAMA_FTYPE` enum likewise adds only `MOSTLY_TQ3_1S = 43` and `MOSTLY_TQ4_1S = 44`.
- **ROCmFPX** lists `Q4_0_ROCMFP4 = 100`, `Q4_0_ROCMFP4_FAST = 101`, `Q6_0_ROCMFPX = 102`,
  `Q8_0_ROCMFPX = 103`, `Q3_0_ROCMFPX = 104`, `Q2_0_ROCMFPX = 107`. It lists **no** turbo entries.
  Its `LLAMA_FTYPE` enum covers 100-106 (ROCmFP4 recipes) and 110-119 (ROCmFPX), including
  `MOSTLY_Q2_0_ROCMFPX = 119`. No turbo FTYPE exists.

So **no GGUF anywhere carries type-id 42, 43 or 44 meaning a turbo type**, because neither fork's
tooling can write one. The blocking collision is between our weight ids and another fork's
*runtime-only* ids — two namespaces that never meet in a file.

This confirms the second half of Sol's lead, and generalises it: the runtime/weight split is not a
property of our PolarQuant branch, it is how **both** source forks already behave.

### 1.3 The consequence

The four blocked slots need **no new mechanism at all**. Every serialized format they bring lands on
an id that is free in our fork and free upstream:

- ROCmFP weight formats want 100, 101, 102, 103, 104, 107 — our `GGML_TYPE_COUNT` is 44, upstream's is 43.
- TurboQuant weight quants want 45, 46 — also free.

They can be adopted **verbatim**, which means files produced by those forks, and by their own convert
and quantize tooling, load natively. No gate. No retag. No ambiguity to resolve.

---

## 2. The real hazard, which is a different one

Upstream `555881ebc` currently defines:

```c
GGML_TYPE_Q1_0    = 41,
GGML_TYPE_Q2_0    = 42,
GGML_TYPE_COUNT   = 43,
```

`GGML_TYPE_Q2_0 = 42` is **upstream's own allocation**, not ours. Our fork adds exactly one id of its
own invention: `GGML_TYPE_Q2_0_G128 = 43` — which sits precisely on **the next value upstream will hand
out**. The first new quant type upstream merges takes 43 and collides with us head-on.

That is the allocation problem this fork actually has. It has nothing to do with TurboQuant or ROCmFPX.

Note the structural asymmetry that makes the answer easy: **ROCmFPX already solved this for itself**
by starting its block at 100. TurboQuant did not, and squats on 42-46 directly in upstream's path.
ROCmFPX is the model to copy.

---

## 3. The allocation table

Two blocks, because there are two genuinely different kinds of type.

### 3.1 Block W — serialized weight formats

Types that appear in a GGUF file. They must round-trip with files that already exist, so their ids
are **not ours to choose**: we adopt whatever the format's originator serialized.

| id | type | owner | block | bytes/block | values | status in this fork |
|---|---|---|---|---|---|---|
| 0-42 | upstream types | upstream | — | — | — | inherited, never touched |
| 42 | `GGML_TYPE_Q2_0` | **upstream** | `d + qs[16]` | 18 | 64 | shipped |
| 43 | `GGML_TYPE_Q2_0_G128` | **ArifiLabs** | `d + qs[32]` | 34 | 128 | shipped; **stays at 43**, see §5 |
| 45 | `GGML_TYPE_TQ3_1S` | TurboQuant | `d0 + d1 + qs[12]` | 16 | 32 | reserved for slot "TurboQuant weight quants" |
| 46 | `GGML_TYPE_TQ4_1S` | TurboQuant | `d0 + d1 + qs[16]` | 20 | 32 | reserved for same slot |
| 100 | `GGML_TYPE_Q4_0_ROCMFP4` | ROCmFPX | `qs[16] + e[2]` | 18 | 32 | reserved for slot "ROCmFP weight formats" |
| 101 | `GGML_TYPE_Q4_0_ROCMFP4_FAST` | ROCmFPX | `qs[16] + e` | 17 | 32 | reserved, same slot |
| 102 | `GGML_TYPE_Q6_0_ROCMFPX` | ROCmFPX | `qs[24] + e[2]` | 26 | 32 | reserved, same slot |
| 103 | `GGML_TYPE_Q8_0_ROCMFPX` | ROCmFPX | `qs[32] + e` | 33 | 32 | reserved, same slot |
| 104 | `GGML_TYPE_Q3_0_ROCMFPX` | ROCmFPX | `qs[12] + e[2]` | 14 | 32 | reserved, same slot |
| 107 | `GGML_TYPE_Q2_0_ROCMFPX` | ROCmFPX | `qs[8] + e[2]` | 10 | 32 | reserved, same slot |

| 48 | `GGML_TYPE_TQ3_4S` | tq3 (**retagged**) | `d(4) + qs[12]` | 16 | 32 | **implemented behind `GGML_ARIFI_TURBO_WEIGHT_QUANTS`** (lane-142) — see §3.1.1 |
| 49 | `GGML_TYPE_TQ3_0` | tq3 (**retagged**) | `d + qs[12]` | 14 | 32 | **implemented behind `GGML_ARIFI_TURBO_WEIGHT_QUANTS`** (lane-142) — see §3.1.1 |
| 50 | `GGML_TYPE_TQ3_4SE` | tq3 (**retagged**) | `d(6) + qs[12]` | 18 | 32 | **implemented behind `GGML_ARIFI_TURBO_WEIGHT_QUANTS`** (lane-142) — see §3.1.1 |
| 51 | `GGML_TYPE_TQ3_1S_SHIFT` | tq3 (**retagged**) | `3 x half + qs[12]` | 18 | 32 | **implemented behind `GGML_ARIFI_TURBO_WEIGHT_QUANTS`** (lane-142) — see §3.1.1 |
| 52 | `GGML_TYPE_TQ3_4SV` | tq3 (**retagged**, their 37) | NOT DERIVED | NOT DERIVED | 32 | **RESERVED — not implemented** (lane-151) — see §3.1.2 |
| 53 | `GGML_TYPE_TQ3_1S_AP1` | tq3 (**retagged**, their 31) | NOT DERIVED | NOT DERIVED | 32 | **RESERVED — not implemented** (lane-151) — see §3.1.2 |
| 54 | `GGML_TYPE_TQ3_1S_TQ3` | tq3 (**retagged**, their 44) | *hypothesis:* same wire as our 45 | *hypothesis:* 16 | 32 | **RESERVED — not implemented** (lane-151) — see §3.1.2 |
| 55 | `GGML_TYPE_ESCHA2` | **ArifiLabs** (lane-164) | packed EschaLabs cbA K=2 code tiles, `[in/16][out/16][32] i16 LE` | 64 | 256 (one 16x16 tile) | **implemented** — NOT row-separable; sole consumer `GGML_OP_ESCHA_MM`; f32 aux sidecar `<base>.escha_aux` required |
| 56 | `GGML_TYPE_ESCHA3` | **ArifiLabs** (lane-164) | packed EschaLabs cbA K=3 code tiles, `[in/16][out/16][48] i16 LE` | 96 | 256 | **implemented** — same rules as 55 |
| 57 | `GGML_TYPE_SX8` | MarlaLabs S-X8 v4.3 (**retagged**, their 41) | `dmin(f16) + dmax(f16) + config(u8) + qh[16] + ql[8] + coeff(u8)` | 30 | 32 | **implemented, CPU only** (lane-212 / WI-1692). Their 41 is upstream `Q1_0` in files that exist, so this is the second family we renumber (same reasoning as §3.1.1). Files carrying 41 go through `tools/gguf-retag-sx8/retag_sx8.py`, which refuses unless every id-41 tensor spans exactly 30 B / 32 values. No `LLAMA_FTYPE`: serialized-but-not-quantizable by our tooling, like `Q2_0_G128`. No Vulkan/CUDA kernel yet (the author's CUDA hunks are staged, not ported). |

`44` is left as a hole. It is the value TurboQuant uses for a runtime-only type and the value our
current `GGML_TYPE_COUNT` occupies; leaving it unassigned costs nothing and removes a whole class of
off-by-one confusion when reading either fork's diffs.

`47` is also left free: TurboQuant uses it for runtime-only `TURBO4_0`, and by the same argument as
`44` it costs nothing to skip. The tq3 family therefore starts at **48**.

#### 3.1.1 tq3: the one imported family we DO renumber, and why

This section is the exception to the rule three paragraphs above ("their ids are **not ours to
choose**"). It is an exception because obeying the rule is arithmetically impossible here.

`github.com/turbo-tan/llama.cpp-tq3` numbers **`TQ3_4S` at 46**. Our 46 is `TQ4_1S`. Both are
serialized weight types, so for the first time two formats we want claim the SAME id, and adopting
the originator's id is not available: one of them has to move, and it cannot be ours — 46 is already
in files we wrote. tq3's ids are therefore remapped into free space at 48+, and any tq3-authored
GGUF must be **retagged** before it can load here.

The size mismatch is what makes this survivable rather than silent:

| | our 46 | tq3's 46 |
|---|---|---|
| type | `GGML_TYPE_TQ4_1S` | `TQ3_4S` |
| block | `d0 + d1 + qs[16]` | `d(4 B) + qs[12]` |
| bytes / 32 values | **20** | **16** |

A file tagged 46 by tq3 and read as our `TQ4_1S` computes a row size 25% too large and fails at load.
That is **luck, not design** — it is exactly the class of hazard §2 is about, and it would be a
silent corruption instead of a loud failure if the two blocks happened to match in size.

Measured 2026-08-17 (lane-139), scanning all 67 GGUF headers under `<MODELS>`:
`models/hf/YTan2000/Qwen3.8-27B-TQ3_4S/Qwen3.8-27B-TQ3_4S-v2.gguf` carries **504 tensors at id 46**,
meaning tq3's `TQ3_4S`. It is the only such file on disk. No other file carries a tq3 id. Evidence:
`research/local-inference/lane-evidence/2026-08-17-lane-139-proofs/I-typeid-scan-BEFORE-enum-change.txt`
(the scan self-check plants a type-46 tensor and confirms it surfaces, so a scan that could not fire
is not what produced this number).

**RESERVED means reserved, and nothing more.** The ids above are written down so that the next lane
cannot pick them for something else and so the collision is discoverable without re-deriving it.
The codec is NOT ported: `TQ3_4S` is a rotated-domain format (`TQ3_0_CENTROIDS`, `TQ3_0_SIGNS`,
`tq3_0_rht_forward/inverse`, `tq3_4s_encode/decode_scale`) at roughly 496 lines in
`tq3:ggml/src/ggml-quants.c`, and a rotated-domain codec is only "ported" when its dequant output is
numerically correct — which nothing short of a coherent-text run on the YTan file demonstrates.

Whoever ports it: land the codec and the coherence proof FIRST, and only then the retag tool. A
retag tool shipped ahead of a working dequant converts a file that fails loudly at load into a file
that loads and emits garbage, which is strictly worse than the situation this section describes.

##### Status after lane-142 (2026-08-17)

The codec IS ported, from `turbo-tan/llama.cpp-tq3@58ad80ffb`, into
`ggml/src/ggml-arifi-turbo-weights.c` under the existing `GGML_ARIFI_TURBO_WEIGHT_QUANTS` flag —
the same file and flag as `TQ3_1S`/`TQ4_1S`, because the two families share the block size, the
Lloyd-Max centroid table and the randomized Hadamard rotation byte for byte, and because placing
the new case labels beside the existing `TQ3_1S` labels inherits its already-correct treatment of
the seven `ggml-cpu/ops.cpp` switch sites. The flag is **default OFF**, like both of its siblings;
"implemented" in the table above means implemented behind it, not compiled by default.

Proof logs: `research/local-inference/lane-evidence/2026-08-17-lane-142-tq3-proofs/`.

Three corrections this port forces on the paragraphs above, all measured at that sha:

1. tq3 does **not** number this family contiguously. Its enum is `TQ3_1S_AP1 = 31`,
   `TQ3_4SE = 36`, `TQ3_4SV = 37`, `TQ3_1S = 44`, `TQ3_4S = 46`, `TQ3_0 = 200`. In particular
   **tq3's `TQ3_0 = 200` is our `TURBO2_0`** — a second, independent collision, and the reason 49
   is not optional.
2. This table reserves **four** destinations but tq3 serializes at least **six** ids. `TQ3_4SV` (37)
   and `TQ3_1S_AP1` (31) have no destination, and tq3's `TQ3_1S` (44) is very likely the same wire
   format as our `TQ3_1S` (45) but has not been proven so. The retag tool **refuses** all three
   rather than guessing. Allocating them is an open decision, not a lane's to make.
   **Superseded in part by §3.1.2 (lane-151):** the three ids now have RESERVED destinations (52, 53,
   54) so nothing else can claim them. The refusals are unchanged and the codecs are still unported —
   reserving an id and porting a format are different acts.
3. The only tq3-id file on the estate carries **`token_embd.weight` as Q6_K**, not as a tq3 type
   (866 tensors = 360 f32 + 2 Q6_K + 504 at id 46). A coherent generation on it therefore exercises
   `mul_mat`/`vec_dot` and the loader, and says **nothing** about `get_rows` — which is precisely
   the switch site that aborted at runtime for g128 (§7). `get_rows` coverage for these ids comes
   from `tests/test-backend-ops.cpp`, where they are listed in `all_types[]`.

#### 3.1.2 The three tq3 ids that lane-142 left with no destination — now RESERVED (lane-151)

Correction 2 of §3.1.1 named a real gap and left it open: tq3 serializes at least **six** ids, this
table reserved **four**, and `TQ3_4SV` (their 37), `TQ3_1S_AP1` (their 31) and tq3's `TQ3_1S` (their
44) had **no destination at all**. An id with no destination is not neutral — it is exactly the
condition under which a later lane picks 52 for something else and the next retag is a collision
instead of a refusal. Lane-151 closes the bookkeeping half, and only the bookkeeping half.

| tq3 id | our reserved id | what is known | what is NOT known |
|---|---|---|---|
| 37 `TQ3_4SV` | **52** | it is a serialized weight type in tq3's enum | block layout, bytes/block, codec — nothing was derived; the name suggests a `TQ3_4S` variant, and a suggestion is not a geometry |
| 31 `TQ3_1S_AP1` | **53** | serialized weight type in tq3's enum | same — layout and codec undrived |
| 44 `TQ3_1S` | **54** | 32 values/block; §3.1.1 records the *hypothesis* that it is the same wire format as our 45 | that the hypothesis is TRUE. It has never been proven on a file. If it is proven, 54 is retired and 45 is the destination — that is a decision, not an inference a lane may make |

**RESERVED-not-implemented means exactly three things.** (1) No other type may claim 52, 53 or 54.
(2) Nothing is ported: there is no block struct, no `type_traits[]` row, no `type_traits_cpu[]` row,
no dispatch case, and `GGML_TYPE_TQ3_4SV`/`_AP1`/`_TQ3` do **not** exist in `ggml.h`. Reserving an id
in a document is not the same act as adding an enumerator, and this table has been wrong about that
distinction before. (3) **The retag tool's refusals STAY.** `tools/gguf-retag-tq3/retag_tq3.py`
`TQ3_ID_UNMAPPED` still refuses 37, 31 and 44, and must keep refusing them.

That last point is the whole safety argument, so it is worth being blunt about why: a reserved
destination id makes the *rewrite* expressible, and the rewrite is the dangerous half. §3.1.1 already
states the rule — "land the codec and the coherence proof FIRST, and only then the retag tool" —
because a retag ahead of a working dequant turns a file that fails loudly at load into a file that
loads and emits garbage. Wiring these three ids into the retag map before their codecs exist would do
precisely that. Whoever ports one of them: port the codec, prove coherent output on a real file,
THEN delete that id's line from `TQ3_ID_UNMAPPED` — in that order, and one id at a time.

### 3.2 Block R — ArifiLabs runtime-only types, **200-255**

Types that exist only as live tensors — KV-cache codecs, repack/interleave layouts, scratch formats.
They never enter a GGUF, so no external file constrains their ids, and we choose them freely.

| id | type | purpose | status |
|---|---|---|---|
| 200 | `GGML_TYPE_ARIFI_POLAR2` | PolarQuant 2-bit KV (was provisional 44) | **reassigned**, see §6 |
| 201 | `GGML_TYPE_ARIFI_POLAR3` | PolarQuant 3-bit KV (was provisional 45) | **reassigned** |
| 202 | `GGML_TYPE_ARIFI_POLAR4` | PolarQuant 4-bit KV (was provisional 46) | **reassigned** |
| 203-255 | reserved | future ArifiLabs runtime-only types | free |

`GGML_TYPE_COUNT` becomes **256** once block R is populated.

### 3.3 Why 200-255, and why a high block at all

The brief asks this explicitly, so here is the argument in full.

**A high block is right, for the runtime block only.** Its whole value is that no other party will ever
allocate there: upstream is at 43 after several years and grows by one or two ids a year; ROCmFPX
claimed 100-107; TurboQuant is stuck in 42-46. 200 clears all three with a margin larger than
upstream's entire history, and leaves 108-199 free for a fourth fork to do what ROCmFPX did.

**A high block is wrong for the weight block.** Moving a serialized format off its originator's id buys
nothing and costs a retag tool, a gate, and permanent divergence from that fork's own convert/quantize
tooling. Ids we did not mint are not ours to renumber.

**Why 256 and not 1024.** Every `[GGML_TYPE_COUNT]`-dimensioned array grows with the ceiling, and the
Vulkan backend has the largest concentration of them: twelve `bool[COUNT]`, five
`vk_matmul_pipeline2[COUNT]`, and — the dominant term — three
`vk_pipeline[DMMV_WG_SIZE_COUNT][COUNT][mul_mat_vec_max_cols]` arrays, which with
`DMMV_WG_SIZE_COUNT = 2`, `mul_mat_vec_max_cols = 8` and `sizeof(std::shared_ptr) = 16` come to
768 bytes per type-id each (`ggml/src/ggml-vulkan/ggml-vulkan.cpp:783-874`). Summing the declarations
gives roughly **1.5 KB of `vk_device_struct` per type-id**, so the ceiling costs about 70 KB at today's
44, ~160 KB at ROCmFPX's 108, **~400 KB at 256**, and ~1.6 MB at 1024 — per Vulkan device. None of
those are fatal; 256 is simply the smallest ceiling that satisfies the requirement, and it keeps the
two `for (i = 0; i < GGML_TYPE_COUNT; ++i)` pipeline sweeps
(`ggml-vulkan.cpp:4025`, `:6625`) cheap.

*Estimate, not a measurement: I did not compile `sizeof(vk_matmul_pipeline2)`. The 768-bytes-per-id
term is exact from the declarations; the 1.5 KB total is the order of magnitude.*

**Sparse ids are already safe.** Upstream's own enum has holes at 4, 5, 31, 32, 33, 36, 37 and 38
(removed types), and `type_traits[GGML_TYPE_COUNT]` is a designated-initializer array that zero-fills
them. Nothing in the tree requires the id space to be dense. A block at 200 introduces no new
category of risk — only more of an existing, exercised one.

---

## 4. When a gate is owed — the criterion

The g128 machinery is often described as "the pattern for adding a type". It is not. It is the pattern
for **resolving an ambiguity**, and it should only be replicated where the same ambiguity exists.

The g128 gate exists for one narrow reason: PrismML shipped 128-value-per-block data under type-id
**42**, an id that in our tree means the 64-value format. One id, two geometries, in files that exist.
`llama_gguf_q2_0_g128_gate` + `llama_verify_q2_0_g128_spans`
(`src/llama-model-loader.cpp:106-263`) resolve that by requiring a `GGML_Q2_0_G128` metadata key **and**
at least one tensor whose byte span matches g128 geometry and not legacy geometry — and refusing the
file outright when a span matches legacy-only, matches neither, or when every span is
alignment-ambiguous.

Generalising from that, the rule for this fork:

> **A metadata-key gate is owed if and only if a single type-id can denote two different block
> geometries in files that exist, AND those geometries can produce identical byte spans.**
>
> If the id is unambiguous, no gate is owed — `gguf_init` already validates every tensor's declared
> span against its type's computed row size, so a wrong-geometry file fails to parse. That is the
> universal fail-closed backstop, and it is why an unambiguous id needs nothing added.

Applying it:

| format | ambiguous id? | gate owed | retag owed |
|---|---|---|---|
| `Q2_0_G128` = 43 | **yes** — PrismML files carry g128 data at id 42 | **yes, shipped** | **yes, shipped** (`tools/gguf-retag-g128/`) |
| ROCmFP weight formats 100-104, 107 | no — nothing else has ever used those ids | **no** | **no** |
| TurboQuant `TQ3_1S`/`TQ4_1S` = 45/46 | no — free in our tree and upstream's | **no** | **no** |
| PolarQuant KV 200-202 | not serialized at all | **no** | **no** |

**One generalized mechanism should not replace the g128 one.** The g128 gate is inseparable from the
specific pair of geometries it discriminates (18 B/64 vs 34 B/128) and from the specific metadata key
in files already on disk. A generic gate would be a config-driven reimplementation of a thing with
exactly one instance, and the current instance is proven in production. **g128 stays as-is; new formats
get nothing, because none of them need anything.** If a future format does hit the criterion above, it
gets its own instance modelled on g128 — which is cheap, because the hard part (span computation,
alignment-ambiguity handling) is already written and can be lifted.

### 4.1 Runtime-only types: the correction to Sol's reasoning

Sol's lead concluded that runtime-only types need no gate "because there is no on-disk file to
disambiguate". **The conclusion is right; the reason is wrong, and the wrong reason is dangerous** because
it would let a future executor skip a check that does exist.

Runtime-only KV types **are** persisted to disk — not in GGUFs, but in session/state files.
`llama_kv_cache::state_write_data` writes the raw numeric type-id for every layer:

```c
const int32_t k_type_i = (int32_t) k->type;
io.write(&k_type_i, sizeof(k_type_i));
const uint64_t k_size_row = ggml_row_size(k->type, n_embd_k_gqa);
io.write(&k_size_row, sizeof(k_size_row));
```
(`src/llama-kv-cache.cpp:2119-2127`, and the `v` equivalents at `:2179`.)

The correct reason no gate is owed is that **the read path is already fail-closed on both fields**:
`state_read_data` compares the persisted id against the live type and errors on mismatch, then
independently compares the persisted row size and errors on mismatch
(`src/llama-kv-cache.cpp:2354-2372`, `:2397-2404`, `:2440-2447`). Renumbering a KV type therefore
cannot silently misread an old session file — it is refused, with a logged reason, by upstream code we
do not have to write.

This is a strictly better foundation than "no file exists": it is true, and it names the code that
provides the guarantee.

It also supplies the argument for putting PolarQuant in a **far** block rather than next door to our
weight ids. TurboQuant's `block_turbo2_0` is 34 bytes over 128 values — **byte-identical geometry to our
`block_q2_0_g128`**. Geometry alone cannot separate those two, so their ids must never be adjacent or
confusable. 200-202 guarantees that by construction.

---

## 5. `Q2_0_G128` stays at 43

Moving it is the one migration this table could have demanded. It should not.

**What is actually on disk** (probed 2026-07-24, `<MODELS>/gguf/ternary/`, reading the GGUF
metadata and tensor-info tables directly):

| file | `GGML_Q2_0_G128` key | tensor type-ids |
|---|---|---|
| `Ternary-Bonsai-27B-...-Q2_0-g128key.gguf` | `u8 = 1` | 498 x **42**, 353 x f32 |
| `Ternary-Bonsai-27B-...-Q2_0.g128.gguf` | `u8 = 1` | 498 x **43**, 353 x f32 |
| `Ternary-Bonsai-27B-...-Q2_0.gguf` | absent | 498 x **42**, 353 x f32 |
| `Ternary-Bonsai-8B-Q2_0-g128key.gguf` | `u8 = 1` | 254 x **42**, 145 x f32 |

Two things follow, and the first corrects the brief.

**The `-g128key.gguf` files do not carry id 43 — they carry id 42 plus the marker key.** Only
`...Q2_0.g128.gguf` is native-43. So the migration exposure of moving 43 is one file, not the set.

**Every native-43 file also carries the marker key.** That closes the residual hole: there is no
key-less native-43 artifact in the estate for a future upstream id-43 to be confused with.

**The future upstream collision is already fail-closed.** If upstream mints its own type 43 and we
rebase, `Q2_0_G128` moves to a new id and existing native-43 files become "upstream type 43" to the
parser. The g128 gate catches this: the key is present so the gate returns true, but the span loop sees
tensors of neither `GGML_TYPE_Q2_0` nor the new `GGML_TYPE_Q2_0_G128`, so `confirmed` stays false and the
loader throws *"GGML_Q2_0_G128=1 was declared, but no Q2_0 tensor span..."*. The file is **refused, not
guessed at** (`src/llama-model-loader.cpp:212-262`, `:850-857`). If the new type's geometry also differs,
`gguf_init` rejects on offsets before that. Two independent fail-closed layers.

So the cost of staying is zero today and a clean refusal later; the cost of moving is a migration for
files that work. **Stay at 43.** Revisit only if upstream actually allocates 43, at which point the
migration is a single `retag_g128.py` invocation with new constants against one file.

---

## 6. PolarQuant KV: reassigned 44/45/46 -> 200/201/202

The `topic/turboquant-kv` branch provisionally took 44/45/46 and flagged it PROVISIONAL pending this
table. It is **reassigned**, for a reason stronger than tidiness:

**45 and 46 are TurboQuant's own serialized weight ids** (`TQ3_1S`, `TQ4_1S`). Holding them for a
runtime-only type would collide with one of the four slots this table exists to unblock — the same
project, the same source fork. That is a real conflict, and it is the only one in the whole problem.

The move is free: these types are never serialized to a GGUF (§1.2), and the session-state path is
fail-closed on id and row size (§4.1), so no file, tool or artifact has to change. The branch's own
executor already verified no `LLAMA_FTYPE` maps to them.

The branch's shipping verdict (measured: do not ship; plain `q4_0` is smaller and faster) is unaffected
and out of scope here. This decides only the ids it uses if and when it lands.

---

## 7. `GGML_TYPE_COUNT` and the dispatch surface

Raising the ceiling to 256 is a one-line change. Adding a type is not, and the failure mode is
silent: the g128 port compiled clean and **aborted at runtime** in
`ggml_compute_forward_get_rows`'s default case. Commit `a54f6cae1` fixed it by adding cases at seven
`ggml/src/ggml-cpu/ops.cpp` switch sites (add, add1, acc, out_prod, set, get_rows, clamp).

The checklist below is the enumerated surface, taken from where `GGML_TYPE_Q2_0_G128` actually had to
appear in this tree (164 occurrences across 26 files). Any slot adding a weight type walks all of it.

**Core (mandatory — a type does not exist without these)**
1. `ggml/include/ggml.h` — the enum value, inside the block this document reserves.
2. `ggml/src/ggml.c` — `type_traits[]` entry: `blck_size`, `type_size`, `is_quantized`, `to_float`,
   `from_float_ref`. A missing entry is a zero-filled row and divides by zero in `ggml_row_size`.
3. `ggml/src/ggml-common.h` (or a carve-out header — ROCmFPX's `ggml/rocmfpx/rocmfpx.h` is the better
   pattern for an imported family) — the block struct plus its `static_assert`.
4. `ggml/src/ggml-quants.c` — quantize/dequantize reference implementations.
5. `ggml/src/ggml-cpu/ggml-cpu.c` — CPU `type_traits_cpu[]` entry (`from_float`, `vec_dot`,
   `vec_dot_type`, `nrows`).

**CPU dispatch (the silent-abort surface — check every one)**

6. `ggml/src/ggml-cpu/ops.cpp` — **seven** switch sites. Traits-generic ops (add, add1, out_prod,
   get_rows) need a real case; ops with no meaning for the type (acc, set, clamp) need an **explicit
   reject** that matches the sibling type's `GGML_ABORT`, never a silent fallthrough.
7. `ggml/src/ggml-cpu/repack.cpp` — `ggml_repack_get_optimal_repack_type()` must return `nullptr` for
   any type without a genuine repack path. The g128 early return at `:5164` is the model; getting this
   wrong dispatches into a wrongly-sized `x4` struct and silently computes garbage (this is exactly
   how the VNNI slot's stride defect manifested).

**Backends (per backend actually built)**

8. `ggml/src/ggml-vulkan/ggml-vulkan.cpp` — the largest surface by far: g128 needed 42 touch points.
   Pipeline arrays, `dequant` shader registration, DMMV/MMQ tables, `get_rows`, `cpy`, `set_rows`, plus
   the shader sources and `vulkan-shaders-gen`.
9. `ggml/src/ggml-cuda/` — ROCmFPX's own type-107 work names the set precisely and it is the best
   available checklist: `common.cuh`, `convert.cu`, `getrows.cu`, `ggml-cuda.cu`, `mmq.cu`, `mmq.cuh`,
   `mmvq.cu`, `vecdotq.cuh`, `dequantize.cuh`, plus a `template-instances/mmq-instance-*.cu` and a
   `generate_cu_files.py` entry.

**Serialization (weight types only — skip entirely for block R)**

10. `include/llama.h` — `LLAMA_FTYPE_MOSTLY_*`, only if the type is quantizable by our tooling.
11. `gguf-py/gguf/constants.py` — `GGMLQuantizationType` **and** `LlamaFileType`. This is the
    ecosystem-visible registry; omitting it is what makes a type invisible to every Python tool.
12. `src/llama-quant.cpp`, `tools/quantize/quantize.cpp` — quantizer wiring.
13. `tests/test-backend-ops.cpp`, `tests/test-quantize-fns.cpp` — coverage.

**Known gap in our own tree:** `GGML_TYPE_Q2_0_G128` is absent from steps 10 and 11 — there is no
`LLAMA_FTYPE_MOSTLY_Q2_0_G128` and no `Q2_0_G128` in `gguf-py/gguf/constants.py`. That is *consistent*,
not a defect: g128 files are produced by retagging, never by our quantizer, so the type is
serialized-but-not-quantizable. It is recorded here so nobody "fixes" it without adding a real
quantizer path first.

---

## 8. Slot compliance — what each blocked slot must do

Four statements, so no slot has to re-litigate any of this.

### Slot: ROCmFP weight formats — **UNBLOCKED, no mechanism owed**

Adopt ROCmFPX's ids **verbatim**: `Q4_0_ROCMFP4 = 100`, `Q4_0_ROCMFP4_FAST = 101`,
`Q6_0_ROCMFPX = 102`, `Q8_0_ROCMFPX = 103`, `Q3_0_ROCMFPX = 104`, `Q2_0_ROCMFPX = 107`. All six are free
in our tree and upstream's; there is no collision and never was one. **No metadata gate. No retag tool.**
ROCmFPX GGUFs load natively.

Raise `GGML_TYPE_COUNT` to 256 in the same commit (not to 108 — 108 would have to move again when
block R lands). Walk the §7 checklist; the CUDA list in item 9 is copied from ROCmFPX's own type-107
commit, so it is known-complete for this family. Prefer their `ggml/rocmfpx/` carve-out over spilling
into `ggml-common.h`.

Two named hazards, neither about ids: `Q3_0_ROCMFPX` (104) and ROCmFPX's runtime `TURBO3_0` (105) have
**identical geometry** (14 B / 32 values), so geometry can never be used to tell them apart — keep the
weight/runtime separation structural, not inferred. And upstream `fa72aeccb` removed rocWMMA
FlashAttention (-756 lines) while our master still carries it; that is this slot's rebase surface.

### Slot: HIP backend deltas — **UNBLOCKED, never actually blocked**

This slot introduces no `ggml_type` values. It was blocked only as a child of the format slot. It
becomes available the moment the formats land, and it inherits their ids with nothing further owed.
Its real content is the 25 `GGML_ROCMFP*` format-scoped build knobs, not the stock upstream HIP knobs.

### Slot: NVFP4 -> ROCmFP4 remap — **UNBLOCKED, no new id**

A conversion between two ids that both already exist: `GGML_TYPE_NVFP4 = 40` (upstream, in our tree
today) and `GGML_TYPE_Q4_0_ROCMFP4 = 100` (adopted above). It mints nothing, so it is outside this
table's scope entirely. Sequence it after the format slot for the destination type.

The RESUME's note stands and is unrelated to ids: the code lives in `src/llama-quant.cpp`, so gating it
as `ARIFI_TOOL_NVFP4_REMAP` mis-tiers it — it is an engine-side quantizer path, not a tool.

### Slot: TurboQuant weight quants — **UNBLOCKED, one prerequisite**

Adopt `TQ3_1S = 45` and `TQ4_1S = 46` **verbatim**. Both are free in our tree (`COUNT = 44`) and
upstream's (`COUNT = 43`). **No gate. No retag.**

**Prerequisite:** `topic/turboquant-kv` must move its PolarQuant types off 44/45/46 to 200/201/202 per
§6 before this slot lands, or the two collide with each other. That branch is parked and unmerged, so
today this is a documentation constraint rather than a code conflict — but it becomes a real conflict
the instant either merges.

Do not port TurboQuant's KV types as a side effect. Also carried over from the lane's findings: their
WHT is **randomized** (sign -> butterfly -> sign, seed 42) and not self-inverse, so it is *not*
interchangeable with upstream's plain orthonormal FWHT, and their dequant deliberately stays in the
rotated domain. The weight quants depend on that rotation.

---

## 9. What this document does not decide

- **Whether any of these slots should ship.** This is an allocation contract. Merge verdicts, defaults
  and measured performance live in `docs/OPTIONS-REGISTRY.md`.
- **`GGML_TYPE_COUNT = 256` is a ceiling, not a promise to fill it.** Ids in block R are handed out one
  at a time, by editing this table in the same commit that adds the type.
- **Whether to upstream anything.** Block R at 200-255 is deliberately un-upstreamable-as-is; if a type
  ever goes upstream it gets an upstream id and this table records the move.

---

## 10. Verification ledger

Every load-bearing claim, with what was read.

| claim | source | verdict |
|---|---|---|
| Upstream `Q2_0 = 42`, `COUNT = 43` | `upstream/master` `555881ebc`:`ggml/include/ggml.h` | confirmed |
| Our `Q2_0_G128 = 43`, `COUNT = 44` | `ggml/include/ggml.h:432-434` | confirmed |
| ROCmFPX `Q2_0_ROCMFPX = 107`, `COUNT = 108` | `rocmfpx/main` `3edc3d31e`:`ggml/include/ggml.h:432-440` | confirmed |
| Type 107 is 10 B / 32 values, dual UE4M3 | `rocmfpx/main:ggml/rocmfpx/rocmfpx.h:13-41`, `ggml/src/ggml.c:711-718` | **Sol's lead confirmed** |
| ROCmFPX 105/106 != TurboQuant 43/44 | block structs, both trees (§1.1) | **package premise falsified** |
| Neither fork serializes turbo types | both `gguf-py/gguf/constants.py`, both `include/llama.h` | confirmed |
| TurboQuant weight ids are 45/46 | `llama-cpp-turboquant:ggml/include/ggml.h:435-436`, `constants.py:4417-4418` | confirmed |
| KV type-ids persist to session files | `src/llama-kv-cache.cpp:2119-2127` | **Sol's stated reason corrected** |
| Session read is fail-closed on id + row size | `src/llama-kv-cache.cpp:2354-2372` | confirmed |
| `-g128key.gguf` files carry id **42**, not 43 | GGUF headers probed directly (§5) | **brief corrected** |
| Every native-43 file carries the marker key | same probe | confirmed |
| Sparse type-id space is already exercised | upstream holes at 4,5,31,32,33,36,37,38 | confirmed |

**Not verified, and named as such:**

- `sizeof(vk_matmul_pipeline2)` was not compiled; the ~1.5 KB-per-id figure in §3.3 is an estimate from
  the declarations. The 768 B/id term is exact.
- The §7 CUDA file list is taken from ROCmFPX's own type-107 commit, not from a build of this fork with
  CUDA enabled. This box is Vulkan.
- No slot was built or run. This document allocates; it does not port.
- ROCmFPX's `rocmfpx/main` was read from the fetched remote ref. The three on-disk ROCmFPX checkouts
  (`src/rocmfpx-fork`, `-a5605a7-wt`, `-latest-wt`) are all **older than type 107** and will mislead
  anyone who reads them instead; re-fetch or read `rocmfpx/main` from git objects.
