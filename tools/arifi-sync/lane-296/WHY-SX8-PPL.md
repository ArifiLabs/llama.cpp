# Why the S-X8 model scored 0.28% worse on the new base (b10825 -> b11178)

lane-296 night maker (Opus 5.5 MEDIUM), 2026-09-28 21:5x EDT. Evidence: `night/BISECT-MEMO.md` (addenda 21:1x, 21:3x),
`night/ev/`, `night/ev3/`, `night/ev4/`, `night/BISECT-LOG.md`. Labels per LL-280: CHECKED = measured or read in code
tonight; ASSUMED = reasoning not yet measured.

## The answer
The cause is upstream commit `70c4e1582e` "vulkan: int8 coopmat1 matmul implementation for AMD RDNA3 and RDNA4"
(2026-09-24), together with one decision in our rebase that let it apply. The S-X8 weights themselves are not
involved.

1. The S-X8 file is a mixed model (CHECKED, gguf tensor types). S-X8 (type 57) covers only the 194 FFN matrices. The
   304 attention and GDN projections (`attn_qkv`, `attn_gate`, `ssm_alpha`, `ssm_beta`, ...) are Q8_0.
2. Before the base move, our engine (r86i) multiplied Q8_0 prompt batches in floating point on the 780M. Integer
   matmul under coopmat was opt-in and OFF: `GGML_ARIFI_MMQ_UNDER_COOPMAT`, whose log line reads "integer MMQ under
   coopmat (sx8, q8_0): OFF" (CHECKED, r86i-vk.cpp:6475, both run logs).
3. Upstream 70c4e1582e adds an int8 coopmat1 kernel for Q8_0 (and Q4_K, Q6_K, ...) on RDNA3. That kernel first rounds
   every prompt activation to 8 bits (q8_1: one scale per 32 values) (CHECKED, U8 ggml-vulkan.cpp:2735-2748, :7711).
4. Our W1 rebase chose to let upstream own Q8_0 on RDNA3 (U8 ggml-vulkan.cpp:2703-2708, "Q8_0 is left to upstream
   there"). So the r86i gate still exists and is still OFF, but it no longer covers Q8_0. This is where the fork
   decided. The rebase record never listed this as a numerics change.
5. The S-X8 FFN weights have no int8 path on this device (the gate is OFF, and S-X8 is not in upstream's list). They
   still run the float mul_mm with the R52b packed decode (CHECKED, code + "packed tile decode: ON" in the log).

The cost therefore lands on the Q8_0 attention/GDN projections of the S-X8 model. The change only applies to prompt
batches of 48 tokens or more. The 48-token floor is our lane-296 N12 knob (`cm1_int_min_n`, RDNA3 default). Upstream
itself uses int8 at every MMQ width. Token generation (one token at a time) runs the mat-vec kernels, which this
commit does not touch. The KV cache that a prompt builds does carry the rounding.

## How we know (CHECKED unless marked)
| test | what it shows | PPL change vs r86i | KL vs r86i |
|---|---|---|---|
| vanilla b10825 vs r86i (Q4) | r86i = upstream b10825 bit for bit, so the move is all from the base | Q4 identical | -0.000012 |
| S-X8, new engine (U8) as shipped | the problem | +0.0254 (+0.28% +-0.13%) | 0.00262 |
| S-X8, U8, fusion off on both sides | fusion is not the cause | +0.0276 | 0.00261 |
| S-X8, U8 + upstream GDN-norm fix reverted | the GDN change is not the cause | +0.0248 | 0.00251 |
| **S-X8, U8, int8 matmul off** (`GGML_ARIFI_CM1_INT_MIN_N=100000`; fusion off both sides) | **the cause: on this model the knob changes only the Q8_0 tensors** | **+0.0056 +-0.0085 (+0.07%, noise)** | **0.00196** |
| Q4_K_XL: U8 as shipped / knob = ALL int8 off (`GGML_ARIFI_CM1_INT_MIN_N=100000`) | the knob moves Q4, inside noise | -0.0181 / +0.0039 | 0.00408 / 0.00202 |
| Q4_K_XL: landed gate 9c0e74c2f0 (Q8_0 int8 off only; Q4_K/Q6_K keep int8) | the gate is NOT the knob: most of Q4's move stays | PPL(Q) 7.4604 | 0.003863 |
| vanilla 70c4e1582e^ (6b790a9c29) / 70c4e1582e (Q4) | one commit, not drift; the parent equals U8 with int8 off bit for bit (KL 0.002018, dPPL +0.003940) | +0.0039 / -0.0142 | 0.002018 / 0.004083 (= b11178) |
| S-X8, U8 fused, int8 off | production config: the loss is fully gone | PPL 7.4082 vs 7.4163: -0.0037 +-0.0085 (-0.05%, noise) | 0.00197 |

- Why V/U8 sit 2.4x closer to the CPU reference on Q4 (KL 0.0026 vs 0.0063, Q4 numbers): the CPU reference also
  rounds activations (Q4_K uses Q8_K, S-X8 uses Q8_1, CHECKED ggml-cpu.c type traits). That the shared rounding is the
  reason is ASSUMED. It is agreement with the CPU's own rounding, not a gain in accuracy.
- Why S-X8 loses and Q4 does not (ASSUMED): in the Q4_K_XL model, weight error is already large, so 8-bit activation
  error disappears into it. In the S-X8 model, the FFN is near-lossless, so the Q8_0 activation rounding becomes a
  visible share of the total error.
- The KL that remains with int8 off (about 0.002 on both models) does not move PPL. Its source is ASSUMED to be rounding
  order in the other upstream commits. CHECKED in part: all of it sits before 70c4e1582e (b10825 -> 6b790a9c29), and
  nothing after 70c4e1582e moves Q4 KL.

## Where it stands (2026-09-29 06:0x, lane-296 relaunch 1f; read-only pass)

The switch-off below is a SAFE STATE, not the fix (President order: a switch-off is never the fix). The fix he
ordered is int8 ON everywhere with S-X8 quality at or above r86i. The int8 lane owns it (`cache/r50-prompts/NIGHT-INT8-MAKER.md`,
running since 05:55; its night6 chain holds the GPU marker at 06:05).

Safe state = fork branch `night/sx8-numerics`, commit `9c0e74c2f0`: upstream's Q8_0 x Q8_1 int8 coopmat1 path sits
behind `GGML_ARIFI_MMQ_UNDER_COOPMAT`, default OFF. Q4_K, Q6_K and the other upstream int8 types keep int8.

Three numbers, r86i / U8 as rebased / U8 + safe state (CHECKED, `night/ev5/summary.txt`, `ev19/B2c-*`, `CHECK-LANE296-FABLE-P3.md` §3):

| metric | r86i | U8 | U8 + 9c0e74c2f0 | read |
|---|---|---|---|---|
| S-X8 PPL, 16 ch -ub 512 | 7.4163 | 7.4373 | 7.4082 | the loss is gone (-0.05%, inside the 0.0085 stderr) |
| S-X8 KL vs r86i base | base | 0.00262 | 0.00197 | closer to r86i |
| Q4_K_XL KL vs CPU ref (c2 recipe, 8 ch) | 0.006327 | 0.002632 | 0.002891 | **misses the brief's bar (<= 0.0027) by 0.00026, about 1.6 SE** |
| Q4_K_XL PPL(Q), same legs | - | 6.7107 | 6.6930 | - |
| S-X8 pp512 t/s, night2 ABBA, n=2 | 42.06 | int8 on 44.56 / 45.65 | int8 off 44.63 / 43.27 | price of the safe state about -2.6%, NOT resolved at n=2 |

Why the Q4 bar is missed: the Q4_K_XL file carries some Q8_0 tensors, and the safe state returns them to float. So Q4
gives back a small part of its move toward the CPU path (0.002632 -> 0.002891). The CPU reference itself rounds
activations (see above), so this is distance to the CPU's own rounding, not a loss of accuracy (ASSUMED reading).

## The three commits (int8 maker R2, 2026-09-29 07:5x; BISECT-LOG.md)

Instrument for all three: vanilla point builds, Q4_K_XL, 16 chunks c512, KL vs the r86i base (`kl/base-Q4-R.kld`).
Output is deterministic per build (idx 0/2/3 read -0.000012 / 7.491784 each, CHECKED).

| # | commit | what it changes | Q4 step (CHECKED) | toward or away from the CPU reference |
|---|---|---|---|---|
| a | `5fdfa62829` "models: fix GDN normalization from max to rsqrt (#28068)" (idx 3 -> 4) | Model math, not numerics. GDN q/k normalization becomes flash-linear-attention's `x * rsqrt(sum(x^2) + eps)` (eps inside the root) instead of `x / max(sqrt(sum(x^2)), eps)`. Built from `rms_norm(x, eps/n) / sqrt(n)`, so it also swaps the op that runs on the GPU. transformers made the same fix (huggingface/transformers#40842). | KL -0.000012 -> 0.001981 (+0.001993 of the 0.002018 residual = ONE commit carries it). PPL 7.491784 -> 7.497631 (+0.0058, about 0.5 of the paired SE 0.0116). | TOWARD, by construction (ASSUMED until night9 measures it): the CPU reference was made with vanilla b11178, which carries this commit, so after it GPU and CPU run the same graph. night9 (`night/gpu-night9.sh`, ev9/summary.txt) reads idx 3 and idx 4 against `kl/base-Q4-CPU-c2.kld`. |
| b | `6788edb4f3` "vulkan: small M matrix optimizations for qwen (#28457)" (idx 73 -> 74; located by night8 stage A2, exact-value bisect, BISECT-LOG.md) | Numerics, summation order only. It swaps A/B for m=1 matmuls and allows split-k for small-M matmuls (ggml-vulkan.cpp, 27 lines, CHECKED by `git show --stat`). Split-k sums f32 partial results in a different order; small-M matmuls in this model are the 48-row GDN gate projections `ssm_alpha/beta`. ASSUMED: the step comes from that reordering, not from a precision change. | PPL 7.497587 -> 7.491812 (-0.0058, same size as a, opposite sign); KL 0.002042 -> 0.002018. Below one SE. | Not measured. Summation order has no "right" side vs the CPU; this step is noise-sized and needs no fix. A KL-only move idx 20 -> 41 (0.001981 -> 0.002042) is not located and is below one SE. |
| c | `70c4e1582e` "vulkan: int8 coopmat1 matmul for AMD RDNA3 and RDNA4 (#27952)" (idx 334 -> 335) | Numerics. Prompt activations are rounded to 8 bits per 32-value block before Q8_0 / K-quant matmuls. | Q4 KL 0.002018 -> 0.004083, PPL 7.491812 -> 7.473680. On S-X8 this is the whole 0.28% loss. | Toward on Q4 (the CPU also rounds activations: U8 0.002632 vs r86i 0.006327 vs CPU, night5). On S-X8 it is a loss vs r86i; which way it moves vs the S-X8 CPU reference is the night8 stage F read (64ch). |

Why reverting (a) on U8 did not recover the S-X8 PPL (CHECKED, table above: +0.0248 vs +0.0254): (a) changes the GDN
q/k norm on every build after idx 4, int8 on or off, and its PPL effect on Q4 is inside noise. The S-X8 loss follows
the Q8_0 int8 switch only (int8 off: +0.0056 inside noise). So (a) explains the KL residual that stays with int8 off,
and (c) explains the S-X8 PPL loss. Where inside (c) the S-X8 loss sits: `night/INT8-ACCURACY.md`.

## Status (int8 maker R4, 2026-09-29 15:3x; CHECKED, `night/ev10/summary.txt`)
- FLOOR shipped as the branch default, `d8c538d52c`: Q8_0 int8 on except `ssm_out`. S-X8 PPL 7.410711 (bar 7.4163),
  Q4_K_XL 7.461558 (r86i 7.4918), GSQ 7.530565 (r86i 7.5549). Passes 3 of 3.
- GOAL (int8 accurate for `ssm_out`) OPEN: B1 7.446102 and B2 7.426905 miss the bar. Mechanism and next variant:
  `night/INT8-ACCURACY.md` section 5.

## Status of the three commits (int8 maker R3, 2026-09-29 14:xx)
- All three are named: `5fdfa62829` (a), `6788edb4f3` (b), `70c4e1582e` (c). Together they carry the whole
  r86i -> vanilla b11178 numerics move on Q4_K_XL (CHECKED, BISECT-LOG.md rows).
- Only (c) costs S-X8 quality. Inside (c) the loss sits in the `ssm_out` matmul (night8 stage G, CHECKED). The
  earlier lead, the gate projections `ssm_alpha/beta`, is FALSIFIED. Candidates and numbers: `night/INT8-ACCURACY.md`.
  HQ takes the merge word to the President.

Rejected paths: keeping upstream numerics as-is (+0.28% on the quality-first model); retuning the S-X8 dequant
constants (the error sits in the Q8_0 activation rounding, not in the S-X8 weights).
