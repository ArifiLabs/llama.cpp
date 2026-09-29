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
- **GOAL = candidate B, int8 accurate FOR `ssm_out`:** OPEN. Two variants tried (B1, B2), both miss the S-X8 bar
  (section 5).

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

| config | Mean KLD | PPL | source |
|---|---|---|---|
| int8 off | 0.001973 | 7.408170 | ev8 / ev10 c-off |
| only ssm_alpha, ssm_beta on | 0.001966 | see ev8 log | ev8 g-onlyab |
| **skip ssm_out (= candidate A)** | **0.002188** | **7.410711** | ev8 g-skipssmout = ev10 a-sx8, bit for bit |
| skip ssm_out + attn_output | 0.002279 | see ev8 log | ev8 g-skipout |
| only ssm_out + attn_output on | 0.002508 | see ev8 log | ev8 g-onlyout |
| all on (vanilla-equivalent) | 0.002619 | 7.437251 | ev8 s-on / ev10 c-on |
| skip attn_output | 0.002682 | see ev8 log | ev8 g-skipattnout |
| Q8_1 scale f16 (R2 repair 1) | 0.002657 | see ev8 log | ev8 q4-sf16 |

- The gate projections `ssm_alpha/beta` are innocent: H1 of the R2 memo is FALSIFIED.
- Taking `ssm_out` off int8 removes 0.000431 of the 0.000646 on-minus-off KLD (67%). The rest spreads over the other
  roles and does not move PPL (A PPL 7.4107 vs off 7.4082, inside the 0.0085 stderr).
- The S-X8 rows marked "see ev8 log" carry a PPL line in their ev8 log; R4 did not re-read them (no new number here).

## 4. Candidate A, the FLOOR (CHECKED, ev10 a-sx8 / a-q4 / a-gsq)

Three numbers per line: r86i / vanilla b11178 / candidate A. Bar (pre-registered, `night/gpu-int8r3a.sh`): S-X8 PPL
at or under 7.4163, and no line worse than r86i.

| line | metric | r86i | vanilla b11178 | candidate A | bar |
|---|---|---|---|---|---|
| S-X8 | PPL 16ch | 7.4163 | N/A (type 57 is fork-only) | **7.410711** | PASS |
| S-X8 | Mean KLD vs r86i | base | N/A | 0.002188 | - |
| Q4_K_XL | PPL 16ch | 7.4918 | 7.473680 | **7.461558** | PASS |
| Q4_K_XL | Mean KLD vs r86i | base | 0.004083 | 0.003941 | - |
| GSQ | PPL 16ch | 7.5549 | OWED (see below) | **7.530565** | PASS |
| GSQ | Mean KLD vs r86i | base | OWED | 0.003899 | - |

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

| variant | roles with two digits | S-X8 PPL | Mean KLD vs r86i | 99.9% KLD | verdict |
|---|---|---|---|---|---|
| all on (one digit) | none | 7.437251 | 0.002619 | - | reference |
| B1 | ssm_out only | 7.446102 | 0.002680 | 0.056349 | FAIL (bar 7.4163) |
| B2 | every Q8_0 role | 7.426905 | 0.002224 | 0.034295 | FAIL (bar 7.4163) |
| A (floor) | none, ssm_out float | 7.410711 | 0.002188 | 0.027863 | PASS |

Why B1 moved the wrong way (reading of the numbers, CHECKED; cause ASSUMED):
- A correct second digit on `ssm_out` must land near A (0.002188). B1 lands at the all-on value (0.002680 vs
  0.002619; PPL +0.0089 = about 0.8 of one 16-chunk SE). So the second digit gave `ssm_out` NOTHING. B1 is "one digit
  plus noise", not a real move worse.
- The role log reads `ssm_out (5120 rows): INT8x2` (CHECKED, ev10/b1-sx8.txt:41), so the path did run.
- B2 gives the OTHER roles a real gain (0.002680 -> 0.002224 when they get two digits too). So the two-pass wiring
  works where the inputs are RMS-normalized hidden states, and fails on `ssm_out`.
- Named mechanism (ASSUMED, not measured): the block scale is stored as f16 (`quantize_q8_1.comp`, `ds = f16vec2(d,
  sum*d)`, CHECKED). d2 is about amax / 32258. f16 is normal only above 6.1e-5, so d2 is subnormal for every block with
  amax below about 1.97, and rounds to zero below about 0.001. `ssm_out` inputs are norm x silu(z) products, smaller
  per block than the normalized hidden states. Where d2 is subnormal it keeps few bits; where it is zero the second
  digit vanishes and the block is back to one digit. This explains B1 = one digit, and B2 helping the other roles.

The variants that follow from that mechanism (one per leg, S-X8 first; a variant at or under 7.4163 gets Q4 and GSQ):
- **B3 (runs first): two digits with the second digit prescaled by 2^8.** The residual quantizer multiplies r by 256
  before it takes d2, so d2 sits in the same f16 range as d1 (normal whenever d1 is normal). The reduce multiplies the
  pass-2 slice by 2^-8 (exact, power of two). Prediction if the mechanism is right: S-X8 KLD near A (about 0.0022) and
  PPL at or under 7.4163. If B3 lands on B1 again, the f16-scale mechanism is FALSIFIED.
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
- Vanilla GSQ cell (section 4), re-run in R4's chain.
- Goal B (section 5).

## 8. Recommendation

Ship candidate A as the floor for S-X8: quality at or better than r86i on all three lines, int8 kept on 79% of the
Q8_0 FLOPs. Keep goal B open until a variant with a named mechanism lands `ssm_out` on int8 at the bar. Nothing is
merged or pushed; HQ takes the merge word to the President.
