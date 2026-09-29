# lane-296 night — INT8 ACCURACY (int8 maker R4, Opus 5.5 MEDIUM, HQ seat-68, 2026-09-29 15:3x)

Labels: CHECKED = measured, or read in code or logs; ASSUMED = reasoning, not measured.
Evidence: `night/ev10/summary.txt` (R3 legs, 16 chunks, c512, seed 1), `night/ev10/*.txt` (raw logs), `night/ev8/`,
`night/ev7/`, `night/BISECT-LOG.md`. Commits: fork branch `night/sx8-numerics` (see `night/LANDED.txt`).

## 1. The answer

- The S-X8 +0.28% PPL loss on the new base comes from upstream `70c4e1582e` (int8 coopmat1 MMQ on RDNA3). It rounds
  every prompt activation of a Q8_0 matmul to 8 bits per 32-value block. On S-X8 the Q8_0 tensors are the attention and
  GDN projections. CHECKED (bisect, `night/WHY-SX8-PPL.md`).
- Inside that commit the loss sits mostly in ONE role: `ssm_out` (48 GDN output projections, 6144 -> 5120). CHECKED
  (night8 stage G role legs, section 3).
- **FLOOR = candidate A, the branch default at `d8c538d52c`:** Q8_0 int8 ON for every role except `ssm_out`, which
  stays on the float matmul. It meets the bar on all three lines (section 4). CHECKED.
- **GOAL = candidate B, int8 accurate FOR `ssm_out`:** the MATH is solved, the BAR is not met. Update 16:3x (int8
  goal maker, `night/INT8-MATH.md`, section 9 below): int8 on `ssm_out` with two 8-bit digits (B5, commit
  `1a88a9c09b`) is 50x MORE accurate than the float path it replaces (op error 1.4e-4 vs 7.7e-3, CHECKED against
  f64, in-graph witness CHECKED). Its S-X8 PPL is 7.431166 (bar 7.4163): FAIL. The 16-chunk PPL has a paired SE of
  0.006-0.012 and cannot resolve the difference (B5 - A = +0.0205, 1.7 SE). A 64-chunk paired check is running.

## 2. Tensor map (CHECKED, `night/gguf_map.py`, raw GGUF header)

S-X8v43: 26.13 GB of weights, 7.69 GB of them Q8_0 (29%). FFN and embeddings are S-X8 type 57 (no int8 path).

| role | count | cols x rows | GB | Q8_0 FLOP share | input to the matmul |
|---|---|---|---|---|---|
| ssm_alpha | 48 | 5120 x 48 | 0.013 | 0.2% | RMS-normalized hidden state |
| ssm_beta | 48 | 5120 x 48 | 0.013 | 0.2% | RMS-normalized hidden state |
| attn_k | 16 | 5120 x 1024 | 0.089 | 1.2% | RMS-normalized hidden state |
| attn_v | 16 | 5120 x 1024 | 0.089 | 1.2% | RMS-normalized hidden state |
| **ssm_out** | 48 | 6144 x 5120 | 1.604 | 20.9% | GDN output after the gated RMSNorm (norm x silu(z)) |
| attn_output | 16 | 6144 x 5120 | 0.535 | 7.0% | attention output after the sigmoid gate |
| attn_gate | 48 | 5120 x 6144 | 1.604 | 20.9% | RMS-normalized hidden state |
| attn_qkv | 48 | 5120 x 10240 | 2.674 | 34.8% | RMS-normalized hidden state |
| attn_q | 16 | 5120 x 12288 | 1.070 | 13.9% | RMS-normalized hidden state |

Q4_K_XL: 0.131 GB Q8_0 (ssm_alpha x48, ssm_beta x48, attn_v x11, attn_k x2, ssm_out x1). GSQ: no Q8_0 tensors.

## 3. Where the loss sits (CHECKED, S-X8 Mean KLD vs the r86i base, 16 chunks; PPL where the log has it)

| config | Mean KLD | Median KLD | PPL | source |
|---|---|---|---|---|
| int8 off | 0.001973 | 0.001323 | 7.408170 | ev6 sx8-off (= ev10 c-off PPL) |
| only ssm_alpha, ssm_beta on | 0.001966 | 0.001345 | 7.409773 | ev8 g-onlyab |
| **skip ssm_out (= candidate A)** | **0.002188** | **0.001557** | **7.410711** | ev8 g-skipssmout = ev10 a-sx8, bit for bit |
| skip ssm_out + attn_output | 0.002279 | 0.001586 | 7.420754 | ev8 g-skipout |
| only attn_q on | 0.002364 | 0.001397 | 7.425040 | ev8 g-onlyq |
| only ssm_out + attn_output on | 0.002508 | 0.001693 | 7.448008 | ev8 g-onlyout |
| all on (vanilla-equivalent) | 0.002619 | 0.001759 | 7.437251 | ev6 sx8-on (= ev10 c-on PPL) |
| skip ssm_alpha, ssm_beta | 0.002664 | 0.001762 | 7.433674 | ev8 g-skipab |
| skip attn_output | 0.002682 | 0.001783 | 7.426277 | ev8 g-skipattnout |
| all on, Q8_1 scale f16 (R2 repair 1) | 0.002657 | 0.001745 | 7.425654 | ev8 s-on |
| skip ssm_out + attn_output, scale f16 | 0.002192 | 0.001512 | 7.410162 | ev8 s-skipout |

- The gate projections `ssm_alpha/beta` are innocent: H1 of the R2 memo is FALSIFIED.
- Taking `ssm_out` off int8 removes 0.000431 of the 0.000646 on-minus-off Mean KLD (67%). The rest spreads over the
  other roles and does not move PPL (A PPL 7.4107 vs off 7.4082, inside the 0.0085 stderr).
- All PPL and median cells are read from the ev6/ev8 logs (CHECKED, 15:3x). The Q4 row with scale f16 (ev8 q4-sf16:
  7.452918 / 0.003903) is the Q4 no-harm leg of repair 1, not an S-X8 row.

## 4. Candidate A, the FLOOR (CHECKED, ev10 a-sx8 / a-q4 / a-gsq)

Three numbers per line: r86i / vanilla b11178 / candidate A. Bar (pre-registered, `night/gpu-int8r3a.sh`): S-X8 PPL
at or under 7.4163, and no line worse than r86i.

| line | metric | r86i | vanilla b11178 | candidate A | bar |
|---|---|---|---|---|---|
| S-X8 | PPL 16ch | 7.4163 | N/A (type 57 is fork-only) | **7.410711** | PASS |
| S-X8 | Mean KLD vs r86i | base | N/A | 0.002188 | - |
| Q4_K_XL | PPL 16ch | 7.4918 | 7.473680 | **7.461558** | PASS |
| Q4_K_XL | Mean KLD vs r86i | base | 0.004083 | 0.003941 | - |
| GSQ | PPL 16ch | 7.5549 | 7.533470 (ev11 v-gsq2) | **7.530565** | PASS |
| GSQ | Mean KLD vs r86i | base | 0.003981 | 0.003899 | - |

- The default path equals the env path bit for bit: a-sx8 = night8 g-skipssmout (7.410711 / 0.002188 / max 0.050070),
  and the role log shows `ssm_out:FLOAT`, every other role `INT8`. a-gsq = night7 gsq-fix (GSQ has no Q8_0). The
  pre-registered F3 falsifiers PASS. CHECKED.
- Switches kept: `GGML_ARIFI_Q8_0_CM1=off` (all Q8_0 float), `=on` with no SKIP (all int8, vanilla numerics),
  `GGML_ARIFI_Q8_0_CM1_ONLY / _SKIP=<roles>`. CHECKED (commit `d8c538d52c`).
- Vanilla GSQ cell (`v-gsq`, ev10): EMPTY. CHECKED: the log is 36 lines, 4099 bytes, the process returned rc=0, and
  the file holds neither the load header nor the final block. It stops mid-row at chunk 15 of 16 (running PPL
  7.4208, KLD 0.00394; candidate A at chunk 15: 7.4229, 0.00390). The run speed (26.97 s per pass) matches the GPU
  legs, so it ran on Vulkan. Cause of the lost head and tail: NOT established (ASSUMED: output capture of the vanilla
  exe, not the model). R4 re-runs it once as the last leg of its chain (named cause: incomplete capture).

## 5. Candidate B, the GOAL (int8 accurate FOR ssm_out): OPEN

What B1 and B2 changed (CHECKED, commit `4714944839`, `GGML_ARIFI_Q8_0_CM1_2D=<roles>`): the listed roles get a second
int8 "digit". Pass 1 = today's q8_1 (q1 = round(x / d1)). Pass 2 = a residual quantizer
(`quantize_q8_1.comp`, spec constant RESIDUAL) that requantizes r = x - q1 * f16(d1) with its own block scale
d2 = max|r| / 127. The MMQ runs once on q1 and once on q2 into two split-k slices; `split_k_reduce` sums them. Weights
are exact int8 and the accumulator is f32 (the cm1 int8 shader is only built without f16acc,
`vulkan-shaders-gen.cpp:647`), so the design target was about 15-bit activations.

| variant | roles with two digits | S-X8 PPL | Mean KLD vs r86i | Median KLD | 99.9% KLD | verdict |
|---|---|---|---|---|---|---|
| int8 off | - | 7.408170 | 0.001973 | 0.001323 | - | reference |
| all on (one digit) | none | 7.437251 | 0.002619 | 0.001759 | - | reference |
| B1 | ssm_out only | 7.446102 | 0.002680 | 0.001736 | 0.056349 | FAIL (bar 7.4163) |
| B2 | every Q8_0 role | 7.426905 | 0.002224 | 0.001506 | 0.034295 | FAIL (bar 7.4163) |
| A (floor) | none, ssm_out float | 7.410711 | 0.002188 | 0.001557 | 0.027863 | PASS |

Why B1 moved the wrong way (numbers CHECKED; cause ASSUMED):
- A correct second digit on `ssm_out` must land near A. B1 lands on all-on: median 0.001736 vs 0.001759, PPL +0.0089
  (about 0.8 of one 16-chunk SE). So the second digit gave `ssm_out` NOTHING. B1 is "one digit plus noise", not a real
  move worse.
- The role log reads `ssm_out (5120 rows): INT8x2` (CHECKED, ev10/b1-sx8.txt:41), so the path did run.
- Median KLD is the clean read (Mean KLD is driven by a few tail tokens, R2 memo section 2): B2 median 0.001506 is
  about A's 0.001557. A = the other roles on one digit plus `ssm_out` float; B2 = the other roles on two digits plus
  `ssm_out` two digits. So in both B1 and B2 the other roles gain from the second digit and `ssm_out` does not. The
  failure is specific to the `ssm_out` input values.
- Named mechanism (ASSUMED, not measured): the block scale is stored as f16 (`quantize_q8_1.comp`, `ds = f16vec2(d,
  sum*d)`, CHECKED). d2 is about amax / 32258. f16 is normal only above 6.1e-5, so d2 is subnormal for every block with
  amax below about 1.97. A subnormal scale at amax 0.1-2 still keeps about 8-10 bits, which is enough for a second
  digit. So the digit only VANISHES if f16 denormals are flushed to zero (on the f16 store, or on the scale read in
  `mul_mmq_cm1`), or if the `ssm_out` blocks are very small (amax below about 0.01). The device reports both
  `shaderDenormPreserveFloat16` and `shaderDenormFlushToZeroFloat16` = true, and neither shader declares a denorm
  execution mode, so the driver's default decides (CHECKED vulkaninfo and shader source; which default = NOT known).
  `ssm_out` inputs are norm x silu(z) products, smaller per block than the normalized hidden states (ASSUMED).

The variants that follow from that mechanism (one per leg, S-X8 first; a variant at or under 7.4163 gets Q4 and GSQ):
- **B3 (runs first): two digits with the second digit prescaled by 2^8.** The residual quantizer multiplies r by 256
  before it takes d2, so d2 sits in the same f16 range as d1 (normal whenever d1 is normal). The reduce multiplies the
  pass-2 slice by 2^-8 (exact, power of two). LANDED `c445bedf35` (`GGML_ARIFI_Q8_0_CM1_2D_SHIFT`, default 8; 0 =
  the B1/B2 path; the plain split_k_reduce pipeline keeps spec constant 0, unchanged). Chain `night/gpu-int8r4.sh`,
  evidence `night/ev11/`. Pre-registered: CONFIRMED if Mean KLD <= 0.0023, median <= 0.0016 and PPL <= 7.4163; then
  Q4 (bar 7.4918) and GSQ (must equal a-gsq bit for bit, which also proves the reduce change neutral). FALSIFIED if
  B3 lands on B1. Caveat: the prescale fixes d2 only; blocks with amax below about 0.008 have a subnormal d1 too.
- B4 (only if B3 fails, and only with a mechanism named from B3's numbers): outlier handling (clamp vs per-block
  scale) or a per-row activation scale. Not run without a stated mechanism.
- Accumulation width is NOT a candidate: the int8 kernel accumulates in f32 (CHECKED above).

## 6. Cross-check vs the 16-chunk CPU slice (CHECKED, ev10 c-*; REPORTED, not a judge)

Reference: `night/kl/base-SX8-CPU-16slice.kld` = the first 16 chunks of night8 stage C (fork U8 on CPU, `-dev none
--no-repack`). The CPU path itself rounds activations to 8 bits per 32-value block (q8_1), like the int8 GPU path, so
it cannot referee int8 vs float.

| config | PPL | Mean KLD vs CPU slice |
|---|---|---|
| int8 all on | 7.437251 | 0.003689 |
| candidate A | 7.410711 | 0.003939 |
| int8 off | 7.408170 | 0.004047 |
| r86i engine | 7.416274 | 0.004097 |

The pre-registered prediction (on <= A <= off on this metric) HOLDS. It ranks by "shares the CPU's rounding", not by
accuracy. The 64-chunk CPU reference is NOT available today: night8 stage C (pid 13696) is PAUSED at about 27 of 64
parts. No new CPU reference leg was started.

## 7. Owed

- TIMED legs: HELD by the measurement law (`night/gpu-int8-timed.sh`). Candidate A's price vs all-on is not measured.
  A moves 20.9% of the Q8_0 FLOPs back to float, so a small pp512 cost is ASSUMED.
- The 64-chunk S-X8 reference (night8 stage C, when HQ resumes it) and KL of off / A / B vs it.
- Vanilla GSQ cell (section 4): FILLED by R4's re-run `ev11/v-gsq2` = PPL 7.533470, Mean KLD 0.003981 (CHECKED,
  stdout/stderr split capture; candidate A 7.530565 / 0.003899).
- Goal B: the 64-chunk paired PPL A vs B5 (`ev14/`, running at 16:30). HQ's call (not the maker's): whether the bar
  stays a 16-chunk PPL with a margin under one paired SE, or moves to an instrument that ranks accuracy (KLD vs an
  INDEPENDENT accurate reference: the float path with f32 accumulation, env switch at `ggml-vulkan.cpp:7384`, op
  error to verify with the probe first; NOT the two-digit path itself, which would be circular for B5).
- B5 Q4_K_XL and GSQ no-harm legs: not run (B5 did not hold the S-X8 bar, pre-registered). GSQ has no Q8_0 and the
  fix touches only env-gated paths, so GSQ is unchanged (ASSUMED, not re-measured).
- The selective two-digit option (15 layers) is not built; it needs a layer list in the role gate.
- Probe price actual: about 190 probe runs (synthetic 4, replay 30, ladder 128, batched 6, witness compares 15, multi
  1, early 2) of 5-20 s each, about 25 min of GPU slot; 4 model loads (capture + 3 witness) about 8 min; 1 S-X8 leg
  5 min. Over the 20-run price line in count, near it in time.

## 9. Int8 goal (maker, 16:3x): mechanism at the level of one multiplication (full derivation `night/INT8-MATH.md`)

What the R4 reading got wrong (CHECKED, op probe `tools/arifi-op-probe`, evidence `night/ev12/`):
- **Flush-to-zero of f16 scales: FALSIFIED.** Subnormal first-digit scales are kept down to amax 0.001.
- **"B1/B3 = one digit plus noise": FALSIFIED.** In the real graph (4 sequences, ubatch 4 x 128), B3's `ssm_out`
  output is 5.0e-4 from f64 on every sampled layer (L0/16/32/48/62), 15x below the float path's 7.6-7.9e-3.

The mechanism, named with its numbers:
1. The kernel has NO defect: int32 per 32-block, no saturation (max 516,128 < 2^31), f32 accumulation, no tail on
   6144 x 5120. GPU = ideal q8 simulator to 1e-5.
2. The loss is precision: one 8-bit scale per 32 activations on heavy-tailed blocks. `ssm_out` block max/median
   reaches 857 at p99 (L0). Op error, one digit vs float path: median 1.19x over 48 layers, L0 3.5x, L52-62 1.3-1.5x;
   `attn_output` control 1.14x (`ev12/ladder-table.md`).
3. The float path is not exact: 7.5-8.5e-3 per Q8_0 matmul (ASSUMED cause: f16 accumulation).
4. Fork defect found and fixed: the two-digit residual used `packHalf2x16` (rounds toward zero on this driver)
   while the stored scale rounds to nearest; floor 5.1e-4. Fix `1a88a9c09b`: 1.38e-4 (L0), 3e-5 (L16-62), equal
   to the ideal simulator.

Candidates (S-X8 16 chunks, vs r86i base):

| variant | `ssm_out` op error vs f64 | PPL | Mean KLD | Median KLD | bar 7.4163 |
|---|---|---|---|---|---|
| int8 off (float everywhere) | 7.7e-3 | 7.408170 | 0.001973 | 0.001323 | - |
| A floor (ssm_out float) | 7.7e-3 | 7.410711 | 0.002188 | 0.001557 | PASS |
| all on (one digit) | 8.1e-3 - 2.8e-2 | 7.437251 | 0.002619 | 0.001759 | FAIL |
| B3 (two digits, R4) | 5.0e-4 (in graph) | 7.425242 | 0.002513 | 0.001719 | FAIL |
| **B5 (two digits, fixed, `1a88a9c09b`)** | **1.4e-4 / 3e-5** | **7.431166** | **0.002612** | **0.001697** | **FAIL** |

The instrument (CHECKED, `night/paired_noise.py`): paired SE of the 16-chunk PPL = 0.006-0.012; bar margin over A
= 0.0056 (under 1 SE); B5 - A = +0.0205 (1.7 SE), B5 - B3 = +0.006 (0.6 SE); a config with MORE int8 beat one with
LESS by 2.1 SE (attn_q + attn_qkv vs only attn_q). KLD vs r86i is distance from the float path's own 7.8e-3 rounding
and cannot rank accuracy: B5 is 50x closer to exact than A on `ssm_out` and reads 0.0026 vs 0.0022.

Exit clause answered with numbers: 8-bit arithmetic is NOT the wall. The limiting property of ONE 8-bit digit is
one scale per 32 values on heavy tails (1.19x the float error, 3.5x on L0). The smallest wider datatype that makes it
accurate is a second 8-bit digit (effectively 16-bit integer activations, still int8 coopmat): 50x below the float
path. What stands between B5 and the bar is the 16-chunk PPL instrument, not the math.

Price of B5 (op microbench on the `ssm_out` shape, INDICATION ONLY, untimed law; served timing HELD): one digit
about 11 ms, float about 16 ms, two digits about 20 ms per 6144x5120x512 matmul. B5 is slower than the floor on
`ssm_out` (21% of Q8_0 FLOPs). A cheaper option (not built): two digits only on the layers where one digit exceeds
1.25x the float error (15 of 48: L0, 2, 21, 22, 38, 42, 52-54, 56-58, 60-62), about 13.8 ms average (ASSUMED from
the per-op indications), every layer at or under 1.25x float.

Running when this was written: `night/gpu-int8goal-p64.sh` (64-chunk paired PPL, A vs B5, no KL base, evidence
`ev14/`, done file `night/int8goal-p64.done`). Pre-registered: |B5 - A| < 2 SE at 64 chunks = the 16-chunk gap was
instrument noise; B5 - A >= +0.012 at > 2 SE = a real effect outside the `ssm_out` matmul's accuracy.

## 8. Recommendation

Ship candidate A as the floor for S-X8: quality at or better than r86i on all three lines, int8 kept on 79% of the
Q8_0 FLOPs. Keep goal B open until a variant with a named mechanism lands `ssm_out` on int8 at the bar. Nothing is
merged or pushed; HQ takes the merge word to the President.

Update 16:3x (int8 goal maker): the floor stays the default. B5 (`GGML_ARIFI_Q8_0_CM1=on GGML_ARIFI_Q8_0_CM1_2D=
ssm_out`, commit `1a88a9c09b`) is the accurate int8 path for `ssm_out` (50x below the float error, CHECKED at op
level and in graph) but misses the 16-chunk PPL bar (7.431166) and costs more than the float path on that op
(indication). Two decisions for HQ and the President: (1) the instrument (64-chunk paired result pending in
`ev14/`, or an independent accurate KL reference); (2) speed vs accuracy on `ssm_out` (float floor, B5, or the
15-layer selective two-digit option).
