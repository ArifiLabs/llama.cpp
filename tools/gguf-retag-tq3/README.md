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

## Why id 46 is decided by geometry

Only 25 % separates the two meanings of id 46 — tq3's `TQ3_4S` is 16 bytes per 32 values,
our `TQ4_1S` is 20 — and that gap is what makes a mistake loud instead of silent. This tool
does not trust the filename, the path, or any metadata key: it measures each id-46 tensor's
byte span from the gaps between consecutive data offsets and votes. A file whose id-46
tensors measure 20 B/32 is ours, and it is **refused**.

That is not hypothetical. `research/local-inference/models/gguf/qwen2.5-0.5b-TQ4_1S.gguf`
carries 169 tensors at id 46, all ours. A retag keyed on the type id alone would silently
corrupt it.

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

`--selftest` builds synthetic GGUFs in a temp dir and asserts all five refusals fire plus
the one success path. It needs no model and no GPU.

## Worked example (lane-142)

```
python retag_tq3.py \
  C:/ArifiLabs/models/hf/YTan2000/Qwen3.8-27B-TQ3_4S/Qwen3.8-27B-TQ3_4S-v2.gguf \
  C:/ArifiLabs/models/hf/YTan2000/Qwen3.8-27B-TQ3_4S/Qwen3.8-27B-TQ3_4S-v2-tq3retag-lane142.gguf \
  --map-out map.tsv
```

866 tensors in, histogram `{f32: 360, q6_K: 2, 46: 504}`, id-46 verdict `tq3 504/504`,
504 rewrites, output histogram `{f32: 360, q6_K: 2, 48: 504}`. The output generates coherent
text at `-ngl 0` and at `-dev Vulkan0 -ngl 999`. Note `token_embd.weight` is Q6_K in that
file, so it exercises `mul_mat`, not `get_rows`.
