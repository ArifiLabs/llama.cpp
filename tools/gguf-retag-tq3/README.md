# gguf-retag-tq3

Retags a tq3-authored GGUF onto this fork's type ids. Modelled on `tools/gguf-retag-g128/`,
but it never writes in place.

## Why this exists

`github.com/turbo-tan/llama.cpp-tq3` serializes `TQ3_4S` at type-id **46**. Our 46 is
`TQ4_1S`, in files we already wrote. Both are serialized weight formats, so for the first
time two formats we want claim the same id, and the imported one has to move. The contract
is `docs/TYPE-ID-ALLOCATION.md` §3.1.1; the mapping is:

| tq3 id | tq3 type | ours |
|---|---|---|
| 46 | `TQ3_4S` | **48** |
| 200 | `TQ3_0` (which is our `TURBO2_0`) | **49** |
| 36 | `TQ3_4SE` | **50** |

## Every rewrite is checked against geometry — per tensor, not per file

A type id does not identify a format. Id 46 proves it: tq3's `TQ3_4S` is 16 bytes per 32
values, our `TQ4_1S` is 20. That 25 % gap is the only thing that makes a mistake loud
instead of silent, so the tool measures it rather than trusting a filename, a path, or a
metadata key. Each tensor's byte span comes from the gaps between consecutive data offsets;
a span must land in `[expected, expected + slack)` where slack is the smaller of the file
alignment and a quarter of the expected size — the lower bound is exact because padding can
only add bytes, and the quarter cap means the band can never reach a competing geometry.

**Any** tensor that fails its check refuses the **whole file**. That includes a tensor whose
span matches our `TQ4_1S`, a span matching nothing, a tensor too small to decide, a table
with overlapping offsets, and a truncated file. This is deliberately per-tensor: an earlier
version took a file-level majority vote and would rewrite a mixed or partly-corrupt file on
the strength of one well-formed tensor.

The check applies to ids 36 and 200 as well, not just 46 — an earlier version rewrote those
on the id alone, which is exactly the pattern this tool exists to prevent, and was safe only
by luck.

That the ambiguity is real is not hypothetical:
`research/local-inference/models/gguf/qwen2.5-0.5b-TQ4_1S.gguf` carries 169 tensors at id 46,
all ours. It is refused, live, in the self-test's companion check.

## Ordering rule (binding)

This tool may only be used against a build whose tq3 codec is compiled in
(`GGML_ARIFI_TURBO_WEIGHT_QUANTS=ON`). Retagging a file for a build that cannot dequantize
it converts a loud load failure into a file that loads and emits garbage — the whole reason
§3.1.1 requires the codec and its coherence proof to land first.

## Use

```
python retag_tq3.py <input.gguf> <output.gguf> [--map-out map.tsv]
python retag_tq3.py --selftest
```

`--map-out` writes a `tensor · old_id · new_id` table, and the run prints whether
`token_embd.weight` was among the retagged tensors — i.e. whether loading the output will
exercise `get_rows` for the new type, or only `mul_mat`.

`--selftest` builds synthetic GGUFs in a temp dir and asserts **ten** refusals fire (in-place,
non-tq3, unmapped id, our-TQ4_1S-at-46, mixed tq3+ours, spans matching nothing, wrong geometry
at 36, wrong geometry at 200, a tensor-less GGUF, and the geometry-free code path), plus two
success paths and a byte-level check that **only** the 4-byte type fields differ between input
and output. It needs no model and no GPU. Every one of those cases was a real defect in this
tool at some point, which is why each stays as a case rather than an argument.

## Worked example (lane-142)

```
python retag_tq3.py \
  <MODELS>/hf/YTan2000/Qwen3.8-27B-TQ3_4S/Qwen3.8-27B-TQ3_4S-v2.gguf \
  <MODELS>/hf/YTan2000/Qwen3.8-27B-TQ3_4S/Qwen3.8-27B-TQ3_4S-v2-tq3retag-lane142.gguf \
  --map-out map.tsv
```

866 tensors in, histogram `{f32: 360, q6_K: 2, 46: 504}`, geometry verified on all 504 id-46
tensors individually, 504 rewrites, output histogram `{f32: 360, q6_K: 2, 48: 504}`. The output
generates coherent text at `-ngl 0` and at `-dev Vulkan0 -ngl 999`. Note `token_embd.weight` is
Q6_K in that file, so it exercises `mul_mat`, not `get_rows`.
