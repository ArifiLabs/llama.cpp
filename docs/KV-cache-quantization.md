# KV Cache Quantization with TurboQuant

TurboQuant adds three runtime-only KV cache quantization types that compress
the K/V cache far beyond the standard `q8_0` while keeping decode quality via a
Walsh-Hadamard rotation (WHT) that Gaussianizes the cache vectors before
quantization:

| Type               | Enum                       | Size            | Compression vs f16 |
|--------------------|----------------------------|-----------------|--------------------|
| `turbo2`           | `GGML_TYPE_TURBO2_0` (200) | 2 bits/value    | 6.4x               |
| `turbo3`           | `GGML_TYPE_TURBO3_0` (201) | 3.25 bits/value | 4.9x               |
| `turbo4`           | `GGML_TYPE_TURBO4_0` (202) | 4.25 bits/value | 3.8x               |

(Ids corrected 2026-09-07: the fork numbers these 43/44/47; this tree carries them in the
runtime-only block at 200..202, see `docs/TYPE-ID-ALLOCATION.md` 3.2.)

These are KV-cache-only types: they are never stored in model files. The
corresponding model-weight quantization types are `TQ3_1S` (45) and `TQ4_1S`
(46) - 3/4-bit WHT-rotated Lloyd-Max quantization, block size 32, exposed in
`llama-quantize` as `TQ3_1S` / `TQ4_1S`.

## Second family: `tbq3_0` / `tbq4_0` (jtrefon/llama.cpp-turboq-mtp)

| Type      | Enum                     | Block                | Size             | Backends with a kernel |
|-----------|--------------------------|----------------------|------------------|------------------------|
| `tbq3_0`  | `GGML_TYPE_TBQ3_0` (58)  | 128 values, 50 bytes | 3.125 bits/value | CPU reference only     |
| `tbq4_0`  | `GGML_TYPE_TBQ4_0` (59)  | 128 values, 66 bytes | 4.125 bits/value | CPU reference only     |

Taken from `jtrefon/llama.cpp-turboq-mtp@6a02d0494` (their ids 42/43, renumbered here because 42 is
upstream `Q2_0` and 43 is our `Q2_0_G128`). Unlike the turbo family above, these are BOTH
`llama-quantize` targets (`TBQ3_0` / `TBQ4_0`, one f16 norm per 128 values, signed Walsh-Hadamard
rotation, Lloyd-Max centroids) and KV cache types (`-ctk tbq4_0 -ctv tbq4_0`). The codec's
dequantizer returns the original domain, so no graph rotation op is involved: the cache holds one
quantized row per token across all heads (the block must divide `n_embd_k_gqa`), and
`build_attn_mha` dequantizes the K view to f32 and the V view to f16 before attention. That is the
source fork's own non-fused path (its Metal and CPU arms); its fused CUDA flash-attention kernel is
not carried yet. On a backend without a SET_ROWS/CPY kernel for 58/59 the scheduler runs those
nodes on the CPU. `tbq3_0` V is refused unless `LLAMA_ALLOW_TBQ3_KV=1` (the source fork measured a
token-repetition collapse with it); `tbq4_0` is the supported member.

## Usage

```bash
llama-cli -m model.gguf -c 8192 -ngl 99 \
    --cache-type-k q8_0 --cache-type-v turbo3
```

Any combination of `f16`, `q8_0`, `turbo2`, `turbo3`, `turbo4` for K and V is
supported; mixing quantized V with unquantized K is the common configuration.

Turbo KV types require flash attention. If a turbo cache type is requested
with flash attention disabled, it is enabled automatically (a warning is
printed). A quantized V cache with flash attention explicitly disabled is an
error, matching upstream behavior for all quantized V types.

The same flags work in `llama-server`, `llama-bench`, and `llama-perplexity`.

## Rotation

K and V vectors are rotated by a fixed 128x128 orthonormal Walsh-Hadamard
matrix before quantization and inverse-rotated after dequantization. Head
dimensions that are not multiples of 128 are zero-padded to the next multiple
of 128 for turbo types. MLA models have no separate V cache (V is a view of
K), so V rotation and padding are skipped for them.

## Environment knobs

| Variable                        | Default | Effect                                                          |
|---------------------------------|---------|-----------------------------------------------------------------|
| `TURBO_LAYER_ADAPTIVE`          | `0`     | Layer-adaptive KV precision; `7` = Boundary V (first/last layers in `q8_0`, middle in turbo) |
| `TURBO_AUTO_ASYMMETRIC`         | `1`     | Auto-select asymmetric K/V types for large-GQA models (`0` disables) |
| `TURBO_SPARSE_V`                | `1`     | Sparse-V dequant skip in flash attention (`0` disables)        |
| `LLAMA_ATTN_ROT_K_OVERRIDE`     | off     | Enable upstream #21038 attention rotation for K                |
| `LLAMA_ATTN_ROT_V_OVERRIDE`     | off     | Enable upstream #21038 attention rotation for V                |
| `LLAMA_ATTN_ROT_DISABLE`        | `0`     | Hard lock-out: force rotation off on both sides (`1` disables) |

Upstream attention rotation is off by default: TurboQuant manages rotation
itself (the WHT applied at cache write is equivalent and interacts with the
cache types). `LLAMA_ATTN_ROT_*` only affects the optional upstream rotation
path for models that benefit from it.

## Model-weight quantization (TQ3_1S / TQ4_1S)

```bash
llama-quantize model-f16.gguf model-tq4.gguf TQ4_1S
```

`TQ3_1S` and `TQ4_1S` are first-class weight types with CUDA/HIP (warp
cooperative mmvq), Metal, and Vulkan kernels. MoE models disable CUDA graphs
for TQ `MUL_MAT_ID` automatically.
