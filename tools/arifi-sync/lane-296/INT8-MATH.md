# lane-296 night — INT8 MATH (int8 goal maker, Opus 5.5 MEDIUM, HQ seat-68, 2026-09-29 16:xx)

Labels: CHECKED = measured, or read in code or logs; ASSUMED = reasoning, not measured.
Source tree: fork branch `night/sx8-numerics`, worktree `cache/lane296/night/src-urev`. File:line refers to that tree.
Evidence: `night/ev12/` (op probe logs), `night/ev12/ladder-table.md` (per-layer table), tool `tools/arifi-op-probe/`.

## 1. The math of one int8 matmul (upstream 70c4e1582e + fork patches) — CHECKED by reading

y[m, n] = sum over k of W[m, k] * x[k, n], K = 6144 for `ssm_out` and `attn_output`, M = 5120.

1. **Weights.** Q8_0: blocks of 32, int8 `qw` and one f16 scale `dw` per block. Exact in int8; no rounding at load.
2. **Activations become q8_1** (`vulkan-shaders/quantize_q8_1.comp`):
   - block of 32 consecutive k values of one column n; `amax` = max |x| over the block (:66-84).
   - `d = amax / 127` in f32 (:88); `q = round(x / d)` in f32, round half away from zero, no clamp (:89-90).
     |q| <= 127 because x / d <= 127 up to f32 rounding.
   - stored: `qs` as int8 and `ds = f16vec2(d, sum(q) * d)` (:150, :152). The scale is rounded to f16 HERE.
     Q8_0 weights are symmetric, so the `sum * d` half is not used by the Q8_0 kernel.
3. **The kernel** (`vulkan-shaders/mul_mmq_cm1.comp`, Q8_0 path = the `#else` branch :415-462):
   - BK = 32 (one q8 block), BK_STEP = 4 (:89-94): each loop step covers 128 k values.
   - per 16x16 tile and per 32-block: `acc = coopMatMulAdd(qw, qx)` in int32 (:447-452).
     Max |acc| = 32 * 127 * 127 = 516,128 < 2^31: **no saturation**. < 2^24: `float(acc)` is exact.
   - `sums += fma(float(acc) * dw, dx, 0)` in f32 (:136-141, :456-459): scales applied per 32-block, accumulated in
     f32 over K / 32 = 192 blocks. The magic-bias int->float trick (:125-127) runs only when WARP != 32; RDNA3 runs
     wave32 for this shader, so it is off (ASSUMED from the commit message "only force subgroup size 32 on AMD RDNA").
   - activation scale load: f16 -> f32 (`mul_mmq_cm1_funcs.glsl:522, :537`); weight scale likewise.
   - tail: K = 6144 = 48 x 128, M = 5120 = 40 x 128: no partial tile on these shapes. Out-of-range rows/cols are
     masked at the store (:497-507).
4. **So the int8 result is** y = sum over blocks b of dw_b * f16(d_b) * sum over i of qw_i * q_i.
   The only rounding is the activation: e_i = x_i - q_i * f16(d_b), with |x_i - q_i * d_b| <= d_b / 2 and a
   per-block scale error q_i * (d_b - f16(d_b)), |relative| <= 2^-11 (RTE).
   Per block, the rounding step is amax_b / 127. A block whose values are small except one outlier keeps the step of
   the outlier for every value: the small values are rounded to 0 or +-1 step. **Error grows with the block's
   max / typical ratio (heavy tails).**
5. **Fork dispatch** (`ggml-vulkan.cpp`): int8 chosen when `quantize_y` (:7773) and a Q8_1 pipeline exists (:7777);
   per-role gate from the weight name (:7794-7815, `GGML_ARIFI_Q8_0_CM1*`); q8_1 quantize with a reuse cache keyed
   by (pipeline, src1 pointer) (:8009-8021); two-digit path (:8035-8059): q2 written after q1 in `prealloc_y`,
   two MMQ passes into two split-k slices, `split_k_reduce_2d` sums them. The quantize dispatch ends with a barrier
   (:7702). The S-X8 PCA correction (`src/llama-graph.cpp:1654-1681`) applies to S-X8-type weights only, not to the
   Q8_0 `ssm_out`. Whole-block decode / MMVQ routes are for n < `cm1_int_min_n`; the prefill shapes here do not
   reach them (ASSUMED from the gate at :7783).
6. **The float path it replaces** ("int8 off" = r86i numerics): activations converted to f16 (`cm1_f16b`,
   :7823-7832), weights dequantized to f16, coopmat1 matmul. Accumulator width: f16acc is selected when available
   for the default precision (:7384, `ggml_vk_get_mul_mat_mat_f16acc`); that it is the f16acc pipeline here is
   ASSUMED (not traced to the pipeline name), but the error it produces is CHECKED (section 2).

## 2. Error of ONE multiplication (CHECKED, op probe vs f64 host reference, 256 output rows x 512 tokens)

Tool: `tools/arifi-op-probe` (`arifi-op-probe synth|replay`, `arifi-op-capture`). The reference is f64 from the exact
Q8_0 weights. "Simulator" = the q8_1 math above done on the host; GPU vs simulator distance is the engagement witness.

### 2a. Synthetic (random Q8_0 6144x5120, Gaussian activations with a fixed block amax; `ev12/synth-*.txt`)

| case | float path | int8 one digit | ideal q8 simulator | GPU vs simulator | 2-digit shift 0 | 2-digit shift 8 | ideal 2-digit |
|---|---|---|---|---|---|---|---|
| amax 1 | 8.12e-3 | 5.14e-3 | 5.14e-3 | 1.5e-5 | 1.95e-5 | 1.93e-5 | 1.95e-5 |
| amax 0.005 (d subnormal) | 8.11e-3 | 5.20e-3 | 5.20e-3 | 2.5e-7 | 1.70e-3 | 1.51e-3 | - |
| amax 0.001 (d subnormal) | 8.15e-3 | 5.19e-3 | 5.19e-3 | 2.7e-5 | 4.54e-3 | 2.35e-5 | - |
| amax 1e-5 (d below f16 min) | 1.39e-1 | 2.43e-1 | 2.43e-1 | 2.5e-7 | 2.43e-1 | 1.35e-3 | - |
| heavy tail (1 in 64 x30) | 8.14e-3 | 1.23e-2 | 1.23e-2 | 5.1e-6 | 5.08e-4 | 5.08e-4 | 4.70e-5 |

- **Flush-to-zero of subnormal f16 scales: FALSIFIED.** int8 at amax 0.001 (d = 7.9e-6, subnormal) equals the
  simulator with denormals kept. The R4 memo's B1 mechanism does not hold on this driver.
- **The kernel is correct.** GPU = ideal simulator to 1e-5 or better in every one-digit case: no shape, tail or
  accumulator defect on 6144 x 5120.
- **The float path is not exact:** 8.1e-3 relative on every case, independent of scale (ASSUMED cause: f16
  accumulation over K = 6144).
- **The two-digit path has a defect:** with varying block scales it floors at 5.08e-4 against 4.70e-5 ideal.

### 2b. Real S-X8 inputs (one 512-token wikitext pass, float path; `ev12/cap/`, `ev12/r-*.txt`, `ev12/ladder/`)

Activation profile per 32-block (`PROFILE` lines):

| tensor | block amax p50 | amax p99 | max / median in block, p50 | p99 | worst |
|---|---|---|---|---|---|
| ssm_out L0 | 0.0336 | 2.47 | 16.7 | 857 | 23787 |
| ssm_out L16 | 0.112 | 0.737 | 8.9 | 48.8 | 3185 |
| ssm_out L32 | 0.313 | 1.44 | 8.7 | 48.0 | 988 |
| ssm_out L48 | 0.38 | 2.29 | 9.6 | 63.6 | 2720 |
| ssm_out L62 | 1.6 | 18.6 | 15.8 | 237 | 2670 |
| attn_output L3 | 0.0304 | 0.514 | 11.4 | 307 | 5042 |
| attn_output L31 | 0.434 | 1.76 | 6.0 | 36.8 | 485 |
| attn_output L63 | 2.79 | 11.1 | 11.2 | 81.6 | 530 |

Op error vs f64, all 48 `ssm_out` and 16 `attn_output` layers (full table `ev12/ladder-table.md`):

| role | int8 / float, median | int8 / float, sum of squares | worst layer |
|---|---|---|---|
| ssm_out (48) | 1.19 | 1.65 | L0: int8 2.70e-2 vs float 7.66e-3 (3.5x); L52-62 1.3-1.5x |
| attn_output (16) | 1.14 | 1.26 | L59 1.52x |

Two digits on real `ssm_out` (shift 8): L0 5.08e-4, L16 4.82e-4, L32 5.02e-4, L48 4.98e-4, L62 4.97e-4 — 15x
BELOW the float path. Ideal two digits (simulator): 1.38e-4 (L0). The GPU value 5.0845e-4 equals the simulator with
a round-toward-zero f16 scale in the residual subtract to 5 digits (L0: 5.0845e-4 both). CHECKED.
Batched shape (4 sequences x 128 tokens, as in the KL legs): identical errors to 5 digits (`ev12/b4-*.txt`).

## 3. Mechanisms named with their numbers

1. **One-digit int8 on `ssm_out` is only 1.19x the float path's own error (median), 3.5x on layer 0.** The loss is
   the q8_1 activation step on heavy-tailed blocks (max/median up to 857 at p99 on L0). It is a precision property
   of one 8-bit scale per 32 values, not a kernel defect. The float path itself errs 7.5-8.5e-3 per Q8_0 matmul.
2. **Two-digit residual defect (fork, R3/R4, `quantize_q8_1.comp` old :94 vs :150):** the residual used
   `unpackHalf2x16(packHalf2x16(d))`, which rounds toward zero on this driver, while the stored scale
   `f16vec2(d)` rounds to nearest. The residual was taken against the wrong d1 for about half the blocks, floor
   2^-11 relative (5.1e-4). The same mismatch sat in the R2 `SCALE_F16` repair (old :86). FIX: both use the store's
   own conversion (now :88, :96). Predicted: 5.1e-4 -> 1.4e-4 on real `ssm_out` (simulator).
3. **The instrument.** KLD vs r86i measures distance from the float path, which carries 7.8e-3 op error per Q8_0
   matmul. Any other rounding — even a 15x more accurate one — is "distance". PPL over 16 chunks moves by about
   +-0.01 under numerical perturbations of the same size (the row-count ladder: only attn_q int8 7.4250, attn_q +
   attn_qkv int8 7.4136). See section 4 for the in-graph witness that separates "B3 not engaged" from "PPL does
   not rank op accuracy at this resolution".

## 4. In-graph witness and the instrument (CHECKED)

### 4a. In-graph `ssm_out` output vs f64 (`night/gpu-int8goal-witness.sh`, `ev12/w-cmp-*.txt`)

One S-X8 forward pass per path in the KL-leg shape (4 sequences x 512 tokens, ubatch = 4 x 128), dumping the
`ssm_out` matmul's own input and output from inside the graph (`cb_eval`); f64 reference from the same in-graph input.

| layer | float path (off) | int8 one digit (all on) | B3 two digits (as it ran in R4) |
|---|---|---|---|
| 0 | 7.59e-3 | 2.79e-2 | 5.12e-4 |
| 16 | 7.80e-3 | 8.11e-3 | 4.82e-4 |
| 32 | 7.83e-3 | 8.31e-3 | 5.01e-4 |
| 48 | 7.90e-3 | 9.26e-3 | 5.03e-4 |
| 62 | 7.71e-3 | 1.09e-2 | 5.06e-4 |

- **B3 WAS engaged and accurate in the real graph**: 15x below the float path on every sampled layer. The R4 memo's
  reading "B1/B3 = one digit plus noise" is FALSIFIED at the op level.
- Limitation: `cb_eval` splits the graph at each observed node, so hazards BETWEEN nodes are masked. Closed separately:

### 4b. Multi-node replay, one graph, no callback (`ev12/multi-b3.txt`, library at 1a88a9c09b)

Seven Q8_0 matmuls in one graph (ssm_out L0, attn_output L3, ssm_out L16, ssm_alpha L4, ssm_out L32, attn_output L31,
ssm_out L62), two-digit on `ssm_out`, read after the second compute: ssm_out 1.38e-4 / 3.10e-5 / 3.25e-5 / 4.53e-5;
the one-digit controls equal their single-op values (1.060e-2, 9.021e-3). No stale-buffer or hazard effect in this
order. That the real graph order behaves the same is ASSUMED (same buffer layout).

### 4c. The model-level instrument (`night/paired_noise.py` over ev6/ev8/ev10/ev11 logs)

Per-chunk ln PPL recovered from the running PPL column; pairs share the same 16 chunks.

| pair | dPPL | paired SE | dPPL / SE | chunks where the first is worse |
|---|---|---|---|---|
| B3 - A | +0.0145 | 0.0102 | +1.4 | 11/16 |
| B2 - off | +0.0187 | 0.0099 | +1.9 | 11/16 |
| all-on - off | +0.0291 | 0.0115 | +2.5 | 12/16 |
| A - off | +0.0025 | 0.0074 | +0.3 | 8/16 |
| B1 - all-on | +0.0088 | 0.0071 | +1.2 | 11/16 |
| only attn_q - (attn_q + attn_qkv) | +0.0114 | 0.0055 | +2.1 | 10/16 |
| all-on - A | +0.0266 | 0.0120 | +2.2 | 10/16 |

- One paired SE of the 16-chunk S-X8 PPL is 0.006-0.012. The bar's margin over the floor (7.4163 - 7.4107 =
  0.0056) is under one SE.
- A MORE int8 config beat a LESS int8 config by 2.1 SE (only attn_q vs attn_q + attn_qkv): perturbation effects of
  about 2 SE occur with no accuracy change in the direction measured. B3 (15x more accurate than float on
  `ssm_out`) vs A (float `ssm_out`) is +1.4 SE: inside that band.
- KLD vs r86i is the distance from the float path, which itself errs 7.6-7.9e-3 per Q8_0 matmul. Any other
  rounding, even a 50x more accurate one, reads as distance. It cannot rank accuracy.

## 5. Candidate from the numbers, verified (CHECKED)

- Predicted before building: two 8-bit digits with ONE f16 conversion for the scale remove both the heavy-tail
  precision limit (1.19x-3.5x float) and the rounding-mismatch floor (5.1e-4). Simulator: 1.38e-4 on L0.
- Built: `1a88a9c09b` (`quantize_q8_1.comp` :88, :96). Op level: 1.38e-4 (L0), 3.1e-5 (L16), 3.3e-5 (L32),
  4.5e-5 (L62) in a 7-node graph with no callback: 50-250x below the float path. The bar of step 5 ("bring
  `ssm_out` to the level of the innocent controls") is exceeded: the controls sit at 7.5e-3 to 1.06e-2.
- Model level, one S-X8 leg (`ev13/b5-sx8.txt`, pre-registered in `night/gpu-int8goal-b5.sh`): PPL 7.431166,
  Mean KLD 0.002612, median 0.001697. FAIL of the 7.4163 bar, as pre-registered (the op model predicted B5 ~ B3).
- Kernel-level time on the `ssm_out` shape (probe, 10 reps, INDICATION ONLY under the untimed law): one digit
  about 11 ms, float about 16 ms, two digits about 20 ms.
- 64-chunk paired PPL, A vs B5: `night/gpu-int8goal-p64.sh`, `ev14/`, running at 16:30.
