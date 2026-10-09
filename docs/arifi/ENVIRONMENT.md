# Environment variables

Every environment variable this fork reads that upstream llama.cpp `b11178` does not, plus the
upstream ones whose default this fork changed. The list comes from the source code: every `getenv`
read in `ggml/`, `src/`, `common/`, `tools/`, `examples/` and `powerinfer/`, compared with the `b11178` tree.

How we set them for each model: [HOW-WE-RUN.md](HOW-WE-RUN.md). Measured evidence per switch:
[OPTIONS-REGISTRY.md](../OPTIONS-REGISTRY.md). Paths in the Source column: `vk/` = `ggml/src/ggml-vulkan/`.
"reg" = the variable also has a row or paragraph in the options registry.

<!-- src: source scan of ggml/, src/, common/, tools/, examples/, powerinfer/ against upstream b11178 (f9af9be219ca647a59106f6201bf0d85fab00224) -->

## How defaults work

- **You need none of these to run.** Every default is chosen in code. The serve lines in
  [HOW-WE-RUN.md](HOW-WE-RUN.md) set no environment variable except on the NVMe-streamed GLM-5.3 line.
- **Some defaults depend on the GPU.** At start the Vulkan backend probes the device:
  the vendor (AMD), the architecture (AMD RDNA3; the Radeon 780M and 890M both class as RDNA3)
  or, for one switch, the device id (`0x150e` = Radeon 890M). Other GPUs keep the upstream behaviour.
- **An environment variable overrides the probe**, in either direction. Each tuned default prints
  one `ggml_vulkan:` line at start, with its source: `device-probe (RDNA3)`, `default`, or the variable name.
- **Values are exact strings.** Most switches accept only the values listed. An unknown value keeps
  the default (it is not read as "on"); several print a warning.

**To turn a default off**, set the value in the "Values" column that restores the old path, for
example `GGML_ARIFI_Q5K_MMVQ=legacy` or `GGML_VK_DISABLE_GDN_BANK=1`. Set it in the shell before you
start the server: `$env:GGML_ARIFI_Q5K_MMVQ = "legacy"` in PowerShell, `export GGML_ARIFI_Q5K_MMVQ=legacy` in bash.

`--arifi-profile <name>` (`cpu-only`, `gpu-offload`) sets a documented group of these variables and
logs each one. A variable you set yourself always wins over the profile. Build options such as
`GGML_ARIFI_ROCMFPX_FORMATS` are not environment variables; if set in the environment, the server warns and ignores it.

<!-- src: vk/ggml-vulkan.cpp:5172-5192 (device id probe), 6092-6103 (vendor probe), 3617-3618, 6224-6227, 6419-6421 (RDNA3 probe), 6250, 6315, 6353, 6393, 6410, 6428, 6434-6460 (startup lines); common/arifi-profile.cpp:28-50, 104-135; common/arg.cpp:1560 -->

## Vulkan: mat-vec kernels (decode and verify widths)

"n" = the number of tokens checked in one step (1 for plain decode, draft depth + 1 when drafting).

| Variable | Values | Default | What it does | Source |
|---|---|---|---|---|
| `GGML_ARIFI_MMVQ_A_HOIST` | `0`, `1` | on for AMD (vendor probe), off elsewhere | q8_1 mat-vec decodes each weight block once per row and k-slice. Also enables the newer MMVQ routing (S-X8 at every width, Q4_K at n ≥ 5 when k ≤ 8192). | vk/ggml-vulkan.cpp:6097; reg |
| `GGML_ARIFI_MMVQ_A_HOIST_IQ1` | `1` | off | The same hoist for IQ1_S and IQ1_M. Never on when `GGML_ARIFI_MMVQ_A_HOIST=0`. | vk/ggml-vulkan.cpp:6110; reg |
| `GGML_ARIFI_MMVQ_ROUTE` | `legacy` | newer routing | `legacy` keeps the hoist but restores the older MMVQ routing rules. | vk/ggml-vulkan.cpp:6119; reg |
| `GGML_ARIFI_MMVQ_WIDE` | `legacy`, `v2` | `legacy` (4-row shape from n = 5) | `v2` starts the 4-row q8_1 shape at n = 4. | vk/ggml-vulkan.cpp:6379; reg |
| `GGML_ARIFI_MMVQ_WIDE_ROWS_FROM` | `0`-`8` | `4` | Index from which the 4-row shape is used. Overrides `GGML_ARIFI_MMVQ_WIDE`. | vk/ggml-vulkan.cpp:6385; reg |
| `GGML_ARIFI_Q4K_W5_SPLIT` | `0`, `1` | `1` on AMD RDNA3, `0` elsewhere | Q4_K split at n = 5-6 for m ≥ 131072 or m ≤ 1024, k ≤ 8192. | vk/ggml-vulkan.cpp:6422 |
| `GGML_ARIFI_Q4K_W5_OVERLAP` | `1`, `3` | `0` (off) | Overlap arms for the Q4_K n ≥ 5 path; measurement arms. | vk/ggml-vulkan.cpp:6400 |
| `GGML_ARIFI_Q4K_W5_ROWS` | `1`, `2`, `4` | `0` (shipped shape) | Rows per workgroup for the Q4_K n ≥ 5 path; measurement arms. | vk/ggml-vulkan.cpp:6405 |
| `GGML_ARIFI_Q5K_MMVQ` | `route`, `legacy` (`v2` runs as legacy) | `route` on AMD, `legacy` elsewhere | Q5_K on the q8_1 MMVQ path at n = 5-8, k ≤ 8192. | vk/ggml-vulkan.cpp:6139; reg |
| `GGML_ARIFI_Q5K_MMVQ_N1` | `0` | on (AMD) | `0` moves Q5_K at n = 1 from MMVQ to the f32 shader. | vk/ggml-vulkan.cpp:8727 |
| `GGML_ARIFI_Q5K_B_HOIST` | `0` | on for n ≤ 3, every device | Q5_K mat-vec loads each activation once per superblock. Bit-identical. | vk/ggml-vulkan.cpp:3551; reg |
| `GGML_ARIFI_Q6K_MMVQ` | `route`, `legacy`, `1`, `0` | `route` on AMD, `legacy` elsewhere | Q6_K on MMVQ at n = 7-8 (n = 6 at 5120x6144 on RDNA3). `1` = MMVQ at every width, `0` = off everywhere (both diagnostics). | vk/ggml-vulkan.cpp:6175; reg |
| `GGML_ARIFI_Q6K_MMVQ_ROWS` | `1`/`2`/`4`, with `@n` (from n) or `@n<n>` (only n), or `inherit` | RDNA3: 1 row at n = 6 only; elsewhere inherited | Rows per workgroup of the Q6_K MMVQ mat-vec. | vk/ggml-vulkan.cpp:3704; reg |
| `GGML_ARIFI_Q6K_F32_ROWS` | `1`/`2`/`4`, optional `@n` | inherited (2 on RDNA3) | Rows per workgroup of the Q6_K f32 mat-vec. | vk/ggml-vulkan.cpp:3658; reg |
| `GGML_ARIFI_Q6K_XFOLD` | `0` | on, every device | Q6_K activation hoist and −32 fold. Not bit-identical to off. | vk/ggml-vulkan.cpp:3525; reg |
| `GGML_VK_Q6K_DIRECT_SCALES` | `0` | on, every device | Q6_K reads its scales directly from the block. `0` = upstream shared-memory path. | vk/ggml-vulkan.cpp:3463; reg |
| `GGML_ARIFI_IQ3_MMVQ` | `legacy`, `v2` | `v2`, every device | iq3_s / iq3_xxs sign hoist. Bit-identical; `legacy` restores the inherited loop. | vk/ggml-vulkan.cpp:3580; reg |
| `GGML_ARIFI_IQ3_N7_ROWS` | `1`, `2`, `4` | `2` on AMD RDNA3, inherited (4) elsewhere | Rows at n = 7 for iq3_s / iq3_xxs. `4` brings back the register spill. | vk/ggml-vulkan.cpp:3620; reg |
| `GGML_ARIFI_IQ3S_N6_ROWS` | `1`, `2`, `4` | `2` on AMD RDNA3, inherited elsewhere | Rows at n = 6 for the iq3_s f32 mat-vec. | vk/ggml-vulkan.cpp:3743 |
| `GGML_ARIFI_IQ4XS_MMVQ` | `legacy`, `route`, `all`, `upstream` | `legacy`, every device | IQ4_XS on MMVQ. `route` failed the greedy-identity check, so it is off. | vk/ggml-vulkan.cpp:6198; reg |
| `GGML_ARIFI_IQ4XS_MV` | `upstream` | ours (16 threads per superblock) | `upstream` = upstream's IQ4_XS mat-vec shader. | vk/ggml-vulkan.cpp:6216 |
| `GGML_ARIFI_IQ4XS_MMVQ_BODY` | `upstream` | ours | `upstream` = upstream's IQ4_XS MMVQ body. | vk/ggml-vulkan.cpp:6218 |
| `GGML_ARIFI_IQ4NL_MMVQ` | `1` | off | IQ4_NL on the q8_1 integer dot at every n. | vk/ggml-vulkan.cpp:6449 |
| `GGML_ARIFI_ROCMFP4_MMVQ` | `0` (`1` = on) | on for ROCmFP4-FAST files | ROCmFP4-FAST on the q8_1 MMVQ route (AMD, n = 3 and 5). `0` = f32 dequant route. | vk/ggml-vulkan.cpp:6155; reg |
| `GGML_ARIFI_SX8_MMVQ` | `0` | on wherever the A-hoist is on (AMD) | S-X8 on the q8_1 MMVQ path. `0` = whole-block f32 route. | vk/ggml-vulkan.cpp:8656; reg |
| `GGML_VK_SX8_MMV_WG`, `GGML_VK_SX8_MMV_ROWS` | integers | unset = tuned lookup | Override workgroup size / rows of the S-X8 mat-vec. Setting either bypasses the tuned table. | vk/ggml-vulkan.cpp:1835, 1839; reg |
| `GGML_ARIFI_MMV_MAX_COLS` | integer | `8` | Widest n that takes mat-vec; wider goes to GEMM. `0` = GEMM at every width. | vk/ggml-vulkan-types.h:403; reg |
| `GGML_ARIFI_MMV_ID_ROWS` | `1`, `2`, `4`, `8` | `4` | Rows per workgroup of the MoE (MUL_MAT_ID) mat-vec, RDNA3 only. | vk/ggml-vulkan.cpp:3310; reg |
| `GGML_ARIFI_G128_MMV_ROWS` | integer > 0 | `2` | Rows per workgroup of the Q2_0_G128 mat-vec. | vk/ggml-vulkan.cpp:3341; reg |
| `GGML_VK_DISABLE_TQ_SUBGROUP` | any value | unset = subgroup path on where the subgroup size is a multiple of 32 | Forces the 32-thread TQ mat-vec instead of the subgroup one. | vk/ggml-vulkan.cpp:3402 |
| `GGML_ARIFI_Q8_1_SCALE` | `f16` | off (upstream) | Scale-consistent activation rounding for q8_1. | vk/ggml-vulkan.cpp:4307 |

<!-- src: vk/ggml-vulkan.cpp:1835-1904, 3310-3341, 3402, 3463, 3510-3760, 4307, 6092-6460, 8656, 8716-8731; vk/ggml-vulkan-types.h:403; docs/OPTIONS-REGISTRY.md "Runtime controls" -->

## Vulkan: prompt reading, attention and fused ops

| Variable | Values | Default | What it does | Source |
|---|---|---|---|---|
| `GGML_ARIFI_SX8_CM1` | `0` | on (int8 coopmat devices) | S-X8 prompt reading on the int8 coopmat1 kernel. `0` = float path. | vk/ggml-vulkan.cpp:2492 |
| `GGML_ARIFI_SX8_CM1_MIN_N` | integer | `56` | Smallest prompt width (columns) that takes the int8 S-X8 path. | vk/ggml-vulkan.cpp:6297; vk/ggml-vulkan-types.h:1333 |
| `GGML_ARIFI_SX8_CM1_ROLES` | comma list of tensor roles | all roles | Limits the int8 S-X8 path to these roles. | vk/ggml-vulkan.cpp:6288 |
| `GGML_ARIFI_SX8_CM1_2D` | role list, `none`, `all` | `ffn_up` | Roles that use two-digit int8 on outlier blocks. | vk/ggml-vulkan.cpp:6303 |
| `GGML_ARIFI_SX8_CM1_2D_GATE` | integer | `20` | Two-digit only where amax > gate x mean abs(x). | vk/ggml-vulkan.cpp:6323; vk/ggml-vulkan-types.h:1343 |
| `GGML_ARIFI_SX8_CM1_H16` | `1` | off | 16-value activation scales instead of two-digit (only if int8 coopmat K = 16). | vk/ggml-vulkan.cpp:6317 |
| `GGML_ARIFI_SX8_MM_PACKED` | `0`, `1` | `1` on AMD RDNA3, `0` elsewhere | Packed S-X8 tile decode in the prefill matmul. Bit-identical. | vk/ggml-vulkan.cpp:2467; reg |
| `GGML_ARIFI_MMQ_UNDER_COOPMAT` | `1` | off | Registers integer MMQ pipelines on coopmat devices (S-X8, Q8_0). | vk/ggml-vulkan.cpp:2485; reg |
| `GGML_ARIFI_CM1_INT_MIN_N` | integer | `48` on AMD RDNA3, `0` elsewhere | Smallest width that takes the int8 coopmat1 MMQ. | vk/ggml-vulkan.cpp:6228 |
| `GGML_ARIFI_CM1_INT_SMALLN` | `down`, `off` | off | `down` = IQ4_XS / Q6_K `ffn_down` takes int8 below the minimum width. | vk/ggml-vulkan.cpp:6234 |
| `GGML_ARIFI_CM1_F16B` | `upstream`, `never` | `auto` | When coopmat1 converts the B matrix to f16. | vk/ggml-vulkan.cpp:6341 |
| `GGML_ARIFI_CM1_F16B_MIN_N` | integer | `24` on AMD RDNA3, `0` elsewhere | Smallest width for f16-B conversion. | vk/ggml-vulkan.cpp:6349 |
| `GGML_ARIFI_CM1_F16B_ID` | `0` | on | f16-B staging for the MoE (MUL_MAT_ID) coopmat1 path. `0` keeps f32-B. | vk/ggml-vulkan.cpp:9963 |
| `GGML_ARIFI_Q8_0_CM1` | `floor`, `on`, `off`, or a row count | `floor` (int8 on, `ssm_out` stays float) | Q8_0 prompt reading on the int8 coopmat1 kernel. | vk/ggml-vulkan.cpp:6243 |
| `GGML_ARIFI_Q8_0_CM1_ONLY`, `GGML_ARIFI_Q8_0_CM1_SKIP` | role lists (`none` = empty) | skip `ssm_out` | Roles that take, or skip, the Q8_0 int8 path. | vk/ggml-vulkan.cpp:6253, 6254 |
| `GGML_ARIFI_Q8_0_CM1_2D` | role list | none | Roles that use two-digit int8 for Q8_0. | vk/ggml-vulkan.cpp:6271 |
| `GGML_ARIFI_Q8_0_CM1_2D_SHIFT` | `0`-`16` | `8` | Second-digit prescale (2^shift) for Q8_0 two-digit. | vk/ggml-vulkan.cpp:6280; vk/ggml-vulkan-types.h:1326 |
| `GGML_ARIFI_F32ACC` | role list | none | Forces f32 accumulation on the float path for these roles. | vk/ggml-vulkan.cpp:6329 |
| `GGML_ARIFI_FA_SPARSE_PREFILL` | `0` | on | Sparse attention at prefill (QSA). | vk/ggml-vulkan.cpp:10794 |
| `GGML_ARIFI_FA_SPARSE_PREFILL_MIN_RATIO` | integer ≥ 1 | `2` | Minimum ratio for the sparse prefill path to engage. | vk/ggml-vulkan.cpp:10795 |
| `GGML_ARIFI_FA_DEQUANT_KV` | `0`, `1` | on, every device (upstream behaviour) | `1` forces the f16 dequant-KV scratch copy, `0` the in-place quantized-KV read. | vk/ggml-vulkan.cpp:10893; reg |
| `GGML_VK_DISABLE_GDN_BANK` | any value | unset = fused GDN state bank on | Turns off the in-place recurrent-state update (GDN state bank). Also refuses `LLAMA_GDN_REPLAY`. | vk/ggml-vulkan.cpp:6502; src/llama-memory-recurrent.cpp:60 |
| `GGML_VK_DISABLE_HC_POST_W` | any value | unset = fused on | Turns off the single-dispatch hyper-connection post step. | vk/ggml-vulkan.cpp:6501 |
| `GGML_VK_CONCAT_TRANSPOSE` | `0` | on | Tiled concat-transpose for the delta-net conv state. | vk/ggml-vulkan.cpp:354; reg |
| `GGML_VK_ESCHA_NO_MULTICOL` | any value | unset = multi-column on | Escha-W2: one column per dispatch instead of shared decode across columns. | vk/ggml-vulkan.cpp:9595 |
| `GGML_VK_ESCHA_MAX_COLS` | integer | `0` (no cap) | Caps the Escha-W2 column ladder (A/B arm). | vk/ggml-vulkan.cpp:9627 |
| `GGML_VK_ESCHA_F16_STAGE` | any value | off | Escha-W2 f16 staging (opt-in, error budget not yet proven). | vk/ggml-vulkan.cpp:9663 |
| `GGML_VK_ESCHA_BLOCK2` | any value | off | Escha-W2 two-block variant (measured slower; kept reachable). | vk/ggml-vulkan.cpp:9681 |
| `GGML_VK_ESCHA_LUT` | any value | off | Escha-W2 table-generator arm, single column. | vk/ggml-vulkan.cpp:9686 |
| `GGML_VK_ESCHA_NO_COOPMAT` | any value | unset = coopmat used where available | Escha-W2 widest rung without coopmat. | vk/ggml-vulkan.cpp:9699 |

<!-- src: vk/ggml-vulkan.cpp:354, 2460-2500, 6220-6357, 6498-6502, 9595-9705, 9955-9966, 10790-10796, 10885-10897; vk/ggml-vulkan-types.h:1314-1347; README.md "What is new in this release" -->

## MoE, expert cache and NVMe expert streaming

| Variable | Values | Default | What it does | Source |
|---|---|---|---|---|
| `GGML_ARIFI_MOE_NVME` | `1` | off | Streams routed experts from NVMe with direct reads. Needs `-lm mmap -b 8 -ub 8 --no-host --no-op-offload` and `--moe-cache`. | vk/ggml-vulkan-buffers.cpp:749; src/llama-model-loader.cpp:24; reg |
| `GGML_ARIFI_MOE_NVME_CACHE_MIB` | MiB | unset (required when streaming) | Must equal the `--moe-cache` budget, or loading refuses. | src/llama-context.cpp:833; reg |
| `GGML_ARIFI_MOE_NVME_IO_LANES` | integer ≥ number of drives | `4` | Parallel NVMe read lanes. | src/llama-moe-disk.h:190; reg |
| `GGML_ARIFI_MOE_NVME_REPLICAS` | path to a manifest | unset | Names identical copies of the model file on other drives; misses are spread over them. | src/llama-moe-disk.h:47; reg |
| `GGML_ARIFI_MOE_NVME_BATCH_READ` | `0` | on | `0` = sequential per-range reads. | ggml/src/ggml-backend.cpp:103 |
| `GGML_ARIFI_MOE_NVME_SIBLING` | `0` | on | Sibling pre-read of experts. | vk/ggml-vulkan-moe-cache.cpp:251 |
| `GGML_ARIFI_MOE_NVME_POLICY` | `none` | heat (any other value) | `none` = no residency reuse between expert nodes. | vk/ggml-vulkan-moe-cache.cpp:1698; reg |
| `GGML_ARIFI_MOE_NVME_RESIDENT_MIB` | MiB | `61440` | Budget for experts kept resident. | vk/ggml-vulkan-moe-cache.cpp:1592; reg |
| `GGML_ARIFI_MOE_NVME_HOT_MODE` | `1` | off | Preloads the hottest experts from a profile. | vk/ggml-vulkan-moe-cache.cpp:1547; reg |
| `GGML_ARIFI_MOE_NVME_PROFILE_IN`, `_PROFILE_OUT`, `_PROFILE_TAG` | path, path, text | unset | Read / write an expert-use profile; the tag must match the model. | vk/ggml-vulkan-moe-cache.cpp:1548, 1862, 1561; reg |
| `GGML_ARIFI_MOE_NVME_SIDECAR_MAP` | path | unset | Manifest of sidecar files for streaming. | src/llama-model-loader.cpp:1965 |
| `GGML_ARIFI_MOE_DECODE_MAX_TOKENS` | `1`-`64` | `1` | Largest node counted as a decode step in the cache counters (3 covers a depth-2 verify). Counters only. | vk/ggml-vulkan-moe-cache.cpp:200 |
| `GGML_ARIFI_VK_MOE_CACHE` | `v1`, `v2` | `v1` | Vulkan expert-cache version. | vk/ggml-vulkan-moe-cache.cpp:138; reg |
| `GGML_ARIFI_VK_MOE_CACHE_SHADER_QUANT` | `0` | on | `0` turns off the cache's quantized shader path. | vk/ggml-vulkan-moe-cache.cpp:149 |
| `GGML_ARIFI_VK_MOE_MV` | `coop` | `row` | Thread mapping of the cache mat-vec. | vk/ggml-vulkan-moe-cache.cpp:159 |
| `GGML_ARIFI_VK_MOE_READBACK_CACHED` | `0` | on (cached) | `0` = staging readback. | vk/ggml-vulkan-moe-cache.cpp:172 |
| `GGML_ARIFI_VK_MOE_CACHE_POOL_CLAMP` | non-zero | off | Restores the old one-tensor pool clamp. | vk/ggml-vulkan-moe-cache.cpp:1294; reg |
| `GGML_ARIFI_VK_MOE_CACHE_HOST_SLAB` | `1`, `2` | off | Host-mapped slabs (`1` legacy, `2` budgeted). Not allowed with NVMe streaming. | vk/ggml-vulkan-moe-cache.cpp:548, 1833; reg |
| `GGML_ARIFI_VK_SHARED_BUDGET_MIB` | MiB | no cap below 11800 | Caps shared (host-visible) allocations; hard maximum 11800 MiB. | vk/ggml-vulkan-shared-budget.h:76, 107; reg |
| `GGML_ARIFI_VK_SHARED_ADMIT` | `0`, `1` | on only when `GGML_ARIFI_MOE_NVME=1` | Admission check for shared allocations against available RAM minus 4 GiB. | vk/ggml-vulkan-shared-budget.h:94; reg |
| `GGML_ARIFI_VK_FILE_CACHE_RESERVE_MIB` | MiB | `2048` | RAM kept for the file-mapping window. | vk/ggml-vulkan-shared-budget.h:72, 103; reg |
| `GGML_ARIFI_MOE_GATHER` | `0`, `1`, `2`, `4` | `1` | MoE expert mat-vec in slot-major order at this width. `0` = off. | vk/ggml-vulkan.cpp:4015 |
| `GGML_ARIFI_MOE_GATHER_ROWS` | `1`, `2`, `4`, `8` | inherited rows; iq3_s gather `2` | Rows per workgroup of the gather pipelines. | vk/ggml-vulkan.cpp:4023, 4037 |
| `GGML_ARIFI_MOE_GATHER_TAIL` | `1` (`2` = planted defect) | off | IQ4_NL gather K-tail redistribution. | vk/ggml-vulkan.cpp:622 |
| `GGML_ARIFI_IQ2S_BODY` | `0`-`5` (`2`, `3` = diagnostics) | `5` (int8 body + batched grid init) | IQ2_S gather body. `0` = legacy body. | vk/ggml-vulkan.cpp:631, 4040 |
| `GGML_ARIFI_IQ4NL_BODY` | `1` (`3` = planted defect) | `0` | IQ4_NL gather byte-pair lookup table. | vk/ggml-vulkan.cpp:638 |
| `GGML_ARIFI_IQ3S_INIT`, `GGML_ARIFI_IQ3XXS_INIT` | `1` (`2` = planted defect) | `0` | Batched grid init for the iq3 gathers. | vk/ggml-vulkan.cpp:645, 651 |
| `GGML_SCHED_PREFETCH_EXPERTS` | integer slots | `0` (off) | Host-to-device expert prefetch; off when NVMe streaming is on. | ggml/src/ggml-backend.cpp:2452; reg |
| `EXPERT_BUNDLE_PATH` | path | unset | PowerInfer: loads experts from an on-disk Q4_0 bundle (CPU streaming). | src/llama-model.cpp:2043; reg |
| `GENERATE_EXPERT_BUNDLE` | any value | unset | PowerInfer: writes an expert bundle. | src/llama-model.cpp:2049 |
| `MAX_N_CACHED` | integer > 512 | `6144` (3 x 64 x 32) | PowerInfer hot-expert RAM cache size. | powerinfer/moe_sparse_pipeline/moe_sparse_pipeline/config.hpp:66; reg |
| `POWERINFER_IOCP` | `0` | on (Windows) | `0` = synchronous reads instead of IOCP for bundle streaming. | powerinfer/moe_sparse_pipeline/iou.cpp:28; reg |
| `POWERINFER_NO_BUFFERING` | `1` | off | Opens the bundle with `FILE_FLAG_NO_BUFFERING` (Windows). | powerinfer/moe_sparse_pipeline/iou.cpp:64; reg |
| `POWERINFER_EXPERT_HEATMAP` | path | unset | Writes an expert-use heat map. | powerinfer/moe_sparse_pipeline/expert_cache.cpp:41 |
| `LANE110_PREFETCH_CAP` | integer | `3 x n_expert_used` | Cap for the PowerInfer expert prefetch. | src/llama-graph.cpp:2365; reg |
| `LLAMA_MOE_F16_ACT_GUARD` | `0`, `1` | on only when Metal is scheduled | L2 rescale of MoE activations before an f16 MUL_MAT_ID. | src/llama-graph.cpp:43 |

<!-- src: listed file:line sites; docs/OPTIONS-REGISTRY.md "X1 NVMe expert streaming", "Runtime controls" -->

## Speculative decoding: MTP and DFlash2

| Variable | Values | Default | What it does | Source |
|---|---|---|---|---|
| `LLAMA_SPEC_DRAFT_RECENT_PENALTY` | `k:strength` | `4:3.0` (on) | MTP drafting de-weights its `k` most recent tokens. `0:0` turns it off. | common/speculative.cpp:1968-1969, 2047 |
| `LLAMA_DFLASH_FUSED_INJECT` | `1` | off (legacy encode+decode path) | `1` = fused DFlash2 KV injection, the upstream shape. This fork's default differs from upstream on purpose. | common/speculative.cpp:1195; reg |
| `GGML_DFLASH2_ADAPTIVE` | truthy | off | DFlash2 adaptive depth (measured a loss on S-X8 27B). | common/speculative.cpp:1187 |
| `GGML_DFLASH2_PREVENTIVE` | truthy | off | Preventive mode of the adaptive controller; needs `GGML_DFLASH2_ADAPTIVE`. | common/speculative.cpp:1188 |
| `GGML_DFLASH2_BLOCK_SIZE_OVERRIDE` | `3`-`64` | the drafter's own block width | Overrides the DFlash2 block width. | common/speculative.cpp:1119 |
| `LLAMA_SPEC_CHAIN` | any value but `0` | off | Chained MTP drafting, as `--spec-chain`. | common/speculative.cpp:68 |
| `LLAMA_SPEC_CHAIN_SUB` | integer | `32768` | In chained MTP drafting, the draft head scores only the first N vocabulary rows (`0` = full head). | src/models/qwen35.cpp:711 |
| `LLAMA_SPEC_NGRAM_CAP0` | integer | `0` (off) | Caps each n-gram draft at this many tokens after a miss; the cap doubles on full accepts. | common/speculative.cpp:2996 |
| `LLAMA_RING_OFF` | any value | ring on | Turns off the recurrent-state ring; rollback then uses checkpoints only. | common/common.h:422 |
| `LLAMA_SPEC_CKPT_ON_DEVICE` | any value | off | Draft checkpoints saved on the device (partial state). | tools/server/server-context.cpp:492 |
| `LLAMA_GDN_REPLAY` | any value | off | Deferred commit and replay of gated-delta-net state. Refused when the GDN bank is off. | src/llama-memory-recurrent.cpp:58 |
| `LLAMA_TOOLCALL_ANCHOR_TOKEN` | token id | unset | Adds one token id to the tool-call checkpoint anchors (`--ctx-checkpoints-toolcall`). | tools/server/server-context.cpp:2312 |

<!-- src: listed file:line sites; README.md "Speculative decoding, MoE and memory"; docs/arifi/STATUS.md "Results that showed no gain" (adaptive depth) -->

## KV cache and recurrent state

| Variable | Values | Default | What it does | Source |
|---|---|---|---|---|
| `LLAMA_ATTN_ROT_K_OVERRIDE` | non-zero | off | Turns on Hadamard rotation of a quantized K cache (head dim a multiple of 64). | src/llama-kv-cache.cpp:751 |
| `LLAMA_ATTN_ROT_V_OVERRIDE` | non-zero | off | The same for a quantized V cache. | src/llama-kv-cache.cpp:758 |
| `LLAMA_ATTN_ROT_K_NROT` | integer | `64` | Rotation tile size. | src/llama-kv-cache.cpp:2742 |
| `LLAMA_ALLOW_TBQ3_KV` | any value | off | Allows a `tbq3_0` V cache, which is refused by default (token repetition). | src/llama-context.cpp:4654; reg |
| `LLAMA_KV_NGRAM_INDEX` | `0` | on (`seq_pos` index) | `0` = the older window scan over used cells. | src/llama-kv-cache.cpp:3191; reg |
| `LLAMA_KV_TAIL` | cells | `0` (off) | Keeps the newest N KV cells exact in an f16 ring beside a quantized cache. | src/llama-kv-cache.cpp:121 |
| `LLAMA_KV_TAIL_F16` | non-zero | off | Composes the read window in f16 instead of f32. | src/llama-kv-cache.cpp:166 |
| `LLAMA_KV_TAIL_PERQ` | non-zero | off | Per-query source selection: quantized body and exact ring in one attention node. | src/llama-kv-cache.cpp:185 |
| `GGML_ARIFI_QSA_POOL_CACHE` | `0` | on | Pooled indexer-key cache for long-context decode. | src/llama-memory-hybrid-idx.cpp:81 |
| `GGML_RECURRENT_STATE_F16` | any value | off (f32) | Stores the recurrent state in f16. Numerics not validated. | src/llama-model.cpp:2821; reg |
| `LLAMA_RS_WROW_SHARE` | `0` | on | Shares one write-row input in the recurrent-state ring. | src/models/delta-net-base.cpp:891 |
| `TURBO_AUTO_ASYMMETRIC` | `0` | on | With a turbo K cache and GQA ratio ≥ 6, upgrades K to `q8_0`. | src/llama-kv-cache.cpp:284 |
| `TURBO_LAYER_ADAPTIVE` | `1`-`7` | `0` (off) | Per-layer K/V types for TurboQuant caches. | src/llama-kv-cache.cpp:456 |

<!-- src: listed file:line sites; docs/OPTIONS-REGISTRY.md "Runtime controls" -->

## Memory and loading

| Variable | Values | Default | What it does | Source |
|---|---|---|---|---|
| `GGML_VK_UMA_PLACEMENT` | `auto`, `legacy`, `device-local` | `auto`: `device-local` on the Radeon 890M (AMD device id `0x150e`), `legacy` (upstream) elsewhere, 780M included | Which memory type backs device buffers on a shared-memory GPU. | vk/ggml-vulkan.cpp:5181; reg |
| `GGML_VK_PLACEMENT` | `bulk-large-heap` | off | Alternative bulk placement on the large heap. | vk/ggml-vulkan-buffers.cpp:37; reg |
| `GGML_VK_STAGING_RESERVE` | bytes (`0` = none) | the suballocation block size (1 GiB unless lowered) | Memory reserved for staging. | vk/ggml-vulkan-buffers.cpp:228; reg |
| `GGML_VK_HOST_SPLIT_MAX` | bytes (`0` = no host placement) | derived from free heap bytes | Bound for buffers placed in host memory. | vk/ggml-vulkan-buffers.cpp:1935; reg |
| `GGML_VK_HOST_SPLIT_MEMTYPE` | `legacy`, index, `auto` | `legacy` | Memory type for host-split buffers. `auto` needs `GGML_VK_MEMTYPE_PROBE_FILE`. | vk/ggml-vulkan-buffers.cpp:176; reg |
| `GGML_VK_HOST_SPLIT_CACHED` | `0`, `1` | no preference | Prefers a host-split type without / with `HOST_CACHED`. | vk/ggml-vulkan-buffers.cpp:200; reg |
| `GGML_VK_MEMTYPE_PROBE_FILE` | path | unset | Receipt from `test-backend-ops memtype`, read by `auto` above. | vk/ggml-vulkan-buffers.cpp:129; reg |
| `GGML_ARIFI_UMA_READ_PATH` | `auto`, `direct`, `staging` | `auto` | How tensors are read back on a shared-memory GPU. `direct` = upstream. | vk/ggml-vulkan-buffers.cpp:1500; reg |
| `LLAMA_MMAP_PREFETCH` | `0`, `1` | on, but off when a device copies weights out of the mapping | Whole-file prefetch of a memory-mapped model. | src/llama-model.cpp:1816 |
| `GGML_ARIFI_MMAP_PREFETCH_MIB` | MiB (max 2048) | whole file when prefetch is on; `0` while NVMe streaming | Size of the mmap prefetch. | src/llama-model-loader.cpp:1880; reg |
| `LLAMA_PLE_PREFETCH` | `0`, `1`, `2` | `1` | Prefetches embedding rows per prompt chunk (`2` also in decode, unmeasured). Qwen3.8-Flash-Next. | src/models/qwen4exp.cpp:1383 |
| `LLAMA_PLE_RELEASE` | `0` | on | Releases embedding rows from the Windows working set. | src/models/qwen4exp.cpp:1358 |
| `LLAMA_PLE_RELEASE_MIB` | MiB | `256` | Release budget for the above. | src/models/qwen4exp.cpp:1359 |
| `LLAMA_L299_LAYOUT_OFF` | any value | layout copies on | Turns off the Flash-Next conv-state layout copies. | src/models/qwen4exp.cpp:14 |
| `LLAMA_L299_ALPHA_OFF` | any value | on | Turns off the Flash-Next alpha-bias reorder (MUL_MAT + ADD fusion). | src/models/qwen4exp.cpp:20 |
| `LLAMA_WIN_UNMAP_FRAGMENT` | any value | off | Drops mapped pages from the Windows working set after use. | src/llama-mmap.cpp:832 |
| `GGML_ARIFI_SX8_PCA` | `0` | on when a PCA companion GGUF is found | `0` skips the S-X8 PCA companion file. | src/llama-model-loader.cpp:878; reg |
| `GGML_ARIFI_SX8_PCA_FILE` | path | auto-discovered | Names the PCA companion file. | src/llama-model-loader.cpp:881; reg |
| `GGML_SCHED_SPLIT_INPUTS_CAP` | integer ≥ 30 | `64` | Inputs per scheduler split. Upstream's fixed value is 30; `30` reproduces it. | ggml/src/ggml-backend.cpp:1181-1186 |

<!-- src: listed file:line sites; README.md "What is new in this release" -->

## Determinism (same bits at every verify width)

All off by default. They make a token's arithmetic at a verify width match plain decode (n = 1).

| Variable | Values | Default | What it does | Source |
|---|---|---|---|---|
| `GGML_ARIFI_MMV_ROWSTABLE_N` | integer | `0` (off) | For n up to N, mat-vec uses the n = 1 route, fold and dot order. | vk/ggml-vulkan.cpp:1904 |
| `GGML_ARIFI_FA_ROWSTABLE_N` | integer | `0` (off) | For up to N query rows, flash attention keeps the n = 1 KV split. | vk/ggml-vulkan.cpp:10747 |
| `GGML_ARIFI_ADD_RMS_ROWSTABLE_N` | integer | `0` (off) | For up to N rows, the ADD + RMS_NORM fusion keeps n = 1 partials. | vk/ggml-vulkan.cpp:7030 |
| `LLAMA_ARIFI_MOE_SUMROWS_MAX` | integer ≥ 1 | `1` | Widths up to N keep the n = 1 MoE expert-sum order. | src/llama-graph.cpp:2578 |
| `GGML_VK_FA_DEADV_KEEP` | any value | off (fix on) | Restores the old path where masked KV cells reach the coopmat1 attention sum (non-repeatable output). | vk/ggml-vulkan.cpp:1511 |

<!-- src: listed file:line sites; README.md "What is new in this release" (repeatable greedy decode) -->

## Other

| Variable | Values | Default | What it does | Source |
|---|---|---|---|---|
| `GGML_ARIFI_VNNI_REPACK` | `1`, `2` | off | CPU repack for Q1_0 / Q2_0 / Q2_0_G128 weights (`2` = dual residency). Set `1` on a CPU-only machine. | ggml/src/ggml-cpu/repack.cpp:4865, 5322; reg |
| `LLAMA_R46_CKPT_FAIL_ALLOC`, `LLAMA_R46_CKPT_FAIL_COPY` | attempt number | inert | Checkpoint fault injection; compiled only with the test-only fault-injection build option. | src/llama-context.cpp:3441, 3500 |

<!-- src: listed file:line sites; src/llama-context.cpp:3347-3352 (compile gate) -->

## Diagnostics and planted defects (never for serving)

These print traces, run probes, or plant a known defect so a test can prove it fails. Do not set them on a serving line.

| Variable | What it does | Source |
|---|---|---|
| `GGML_ARIFI_MMVQ_TRACE` | One stderr line per distinct MMVQ route decision. | vk/ggml-vulkan.cpp:8878; reg |
| `GGML_ARIFI_MMV_PIPELINE_TRACE` | Traces the mat-vec pipeline chosen per call. | vk/ggml-vulkan.cpp:8997 |
| `GGML_ARIFI_FA_TRACE` | One stderr line per distinct flash-attention dispatch. | vk/ggml-vulkan.cpp:11109 |
| `GGML_ARIFI_GATE_CENSUS` | Records S-X8 two-digit gate counts. | vk/ggml-vulkan.cpp:8449 |
| `GGML_ARIFI_VK_MOE_TRACE` | Traces expert-cache staging and fills. | vk/ggml-vulkan-buffers.cpp:1128 |
| `GGML_VK_ALLOC_TRACE` | Prints every buffer placement decision. | vk/ggml-vulkan-buffers.cpp:1942; reg |
| `GGML_VK_GDN_BANK_DEBUG` | Debug output for the GDN state bank. | vk/ggml-vulkan.cpp:16910 |
| `GGML_VK_GDN_SNAP1_DIAG` | Writes GDN bank slot 0 only; output is wrong on any rejected draft. | vk/ggml-vulkan.cpp:13263; reg |
| `GGML_VK_FA_MASK_OPT_DISABLE` | Turns off the flash-attention mask optimization. | vk/ggml-vulkan.cpp:11035 |
| `GGML_VK_FORCE_SYNC` | Barrier before every node (bisect switch). | vk/ggml-vulkan.cpp:15310 |
| `GGML_VK_ARIFI_NEG_CPY_F16` | Loads a wrong-scale quant-to-f16 copy (negative control). | vk/ggml-vulkan.cpp:4441 |
| `GGML_VK_GDN_BANK_PLANT`, `GGML_VK_GDN_REPLAY_PLANT`, `GGML_VK_HC_POST_W_PLANT` | Planted defects for the GDN bank, GDN replay and hyper-connection op tests. | vk/ggml-vulkan.cpp:13260, 13273, 15505 |
| `GGML_ARIFI_SX8_CM1_RED`, `GGML_ARIFI_SX8_RESIDUAL_RED` | Planted defects for the S-X8 int8 path. | vk/ggml-vulkan.cpp:2496, 8252 |
| `GGML_ARIFI_MOE_GATHER_PLANT` | Planted wrong token count in the expert gather. | vk/ggml-vulkan.cpp:4019 |
| `GGML_ARIFI_QSA_POOL_RED`, `GGML_ARIFI_QSA_RED_PLANT` | Planted defects in the indexer-key cache and sparse prefill. | src/llama-memory-hybrid-idx.cpp:557; vk/ggml-vulkan.cpp:11286 |
| `LLAMA_PLE_PREFETCH_PLANT`, `LLAMA_L299_ALPHA_PLANT`, `LLAMA_L299_LAYOUT_PLANT`, `LLAMA_QWEN4EXP_MTP_RED` | Planted defects in the Flash-Next graph. | src/models/qwen4exp.cpp:1384, 1105, 1449, 334 |
| `LLAMA_R45_RED_QUANT_VIEW` | Planted defect in checkpoint sizing. | src/llama-context.cpp:3332 |
| `LLAMA_KV_TAIL_NORING`, `LLAMA_KV_TAIL_BREAK`, `LLAMA_KV_TAIL_HIWATER`, `LLAMA_KV_TAIL_AUDIT` | KV tail control arm, broken arm, legacy rule and content audit. | src/llama-kv-cache.cpp:143, 196, 209; src/llama-context.cpp:2479 |
| `LLAMA_CKPT_STORAGE_TRACE` | Traces checkpoint storage in the server. | tools/server/server-context.cpp:2834 |
| `LLAMA_DFLASH_VERIFY_TRACE` | Traces DFlash2 verify steps. | common/speculative.cpp:1768 |
| `LLAMA_DFLASH_CAPTURE_PROBE`, `LLAMA_PPL_CAPTURE_PROBE` | Keeps the next-token capture on without drafting (server / perplexity). | tools/server/server-context.cpp:1545; tools/perplexity/perplexity.cpp:2071 |
| `LLAMA_DFLASH2_FORCE_CAUSAL` | Runs the DFlash2 drafter causally (probe). | common/speculative.cpp:1261 |
| `LLAMA_FORCE_RS_SEQ` | Forces the recurrent-state ring size. | common/common.cpp:1827 |
| `LLAMA_MOE_UNION_LOG` | Logs the union of routed experts. | common/common.cpp:1858 |
| `LANE110_PROF` | Per-op profiling. | src/llama-context.cpp:1829; reg |
| `LLAMA_GDN_SEQ_VERIFY`, `LLAMA_RS_R1_OFF`, `LLAMA_RS_CONV_MULTIWRITE`, `LLAMA_RS_CONV_NOSHIFT`, `LLAMA_RS_CONV_FRESH1`, `LLAMA_RS_BANK_OFF_SSM` | Ring diagnostics. Any one of them turns off the runtime-index ring. | src/llama-memory-recurrent.cpp:44-49 |
| `LLAMA_RS_BANK_OFF_CONV`, `LLAMA_RS_CONV_DIRECT`, `LLAMA_RS_SHIFT_CONT`, `LLAMA_RS_DUMMY_NODE` | Ring-repair probes in the delta-net graph. | src/models/delta-net-base.cpp:490, 615, 606, 482 |
| `LLAMA_RS_AGE_FIX`, `LLAMA_RS_NO_ZERO_ON_CLEAR`, `LLAMA_RS_ZERO_PLANE0_ONLY`, `LLAMA_RS_ZERO_SL_ONLY`, `LLAMA_RS_ZERO_RL_ONLY` | Ring-repair arms for state restore and clearing. | src/llama-memory-recurrent.cpp:390, 286, 290, 293, 294 |
| `LLAMA_RS_TRACE`, `LLAMA_RS_POST_DECODE_HASH`, `LLAMA_RS_ZERO_AUDIT`, `LLAMA_RS_ZERO_GATHER_OUT`, `LLAMA_RS_ZERO_TAP` | Ring traces, hashes and zeroing probes. | src/llama-graph.cpp:1224, 3951, 3965; src/llama-context.cpp:2548, 2565 |

<!-- src: listed file:line sites -->

## Upstream variables whose default or scope this fork changed

| Variable | Upstream `b11178` | This fork | Source |
|---|---|---|---|
| `LLAMA_ATTN_ROT_DISABLE` (and rotation itself) | Hadamard rotation on by default for a quantized K or V cache (head dim a multiple of 64). | Rotation off on both sides by default; turn it on per side with `LLAMA_ATTN_ROT_K_OVERRIDE` / `_V_OVERRIDE`. `LLAMA_ATTN_ROT_DISABLE=1` still blocks both. | src/llama-kv-cache.cpp:739-763 |
| `GGML_VK_DISABLE_FUSION` | Disables Vulkan op fusions. | Also refuses `LLAMA_GDN_REPLAY`. | src/llama-memory-recurrent.cpp:60 |
| `GGML_VK_FA_SPARSE_DISABLE` | Read by upstream. | Also turns off this fork's sparse prefill path. | vk/ggml-vulkan.cpp:10796 |

The scheduler split-input cap (`GGML_SCHED_SPLIT_INPUTS_CAP`, a new variable) also changes a default: 64 instead of upstream's fixed 30.

<!-- src: upstream b11178 src/llama-kv-cache.cpp:315-334 vs fork src/llama-kv-cache.cpp:739-772; vk/ggml-vulkan.cpp:10794-10796; ggml/src/ggml-backend.cpp:1176-1186 -->

## Command-line flags this fork added, and their environment mirrors

Each flag can also be set through its `LLAMA_ARG_*` variable, as in upstream.

| Variable | Flag | Default | Source |
|---|---|---|---|
| `LLAMA_ARG_ARIFI_PROFILE` | `--arifi-profile` | none | common/arg.cpp:1560 |
| `LLAMA_ARG_CTX_CHECKPOINTS_DEVICE` | `--ctx-checkpoints-device {auto,on,off}` | `auto` = on for recurrent / hybrid models | common/arg.cpp:1775 |
| `LLAMA_ARG_CTX_CHECKPOINTS_TOOLCALL` | `--ctx-checkpoints-toolcall {on,off}` | on (needs `--ctx-checkpoints` > 0) | common/arg.cpp:1789 |
| `LLAMA_ARG_DFLASH_DEFER_INJECTION` | `--dflash-defer-injection <0\|1>` | `1` | common/arg.cpp:4307 |
| `LLAMA_ARG_KV_MEAN_CENTER` | `--kv-mean-center FNAME` | unused (needs `-DGGML_ARIFI_KV_MEANCENTER=ON` and `-ctk q4_0`) | common/arg.cpp:2550 |
| `LLAMA_ARG_MOE_CACHE` | `--moe-cache MODE` | `auto` | common/arg.cpp:2917 |
| `LLAMA_ARG_SPEC_CHAIN` | `--spec-chain 0\|1\|N` | off | common/arg.cpp:4398 |
| `LLAMA_ARG_SPEC_DRAFT_ADAPTIVE` | `--spec-draft-adaptive` | off | common/arg.cpp:4333 |
| `LLAMA_ARG_SPEC_DRAFT_N_MIN_ADAPTIVE` | `--spec-draft-n-min-adaptive N` | see `--help` | common/arg.cpp:4380 |
| `LLAMA_ARG_SPEC_DRAFT_N_CTX` | `-cd`, `--ctx-size-draft N` | `0` (from the model) | common/arg.cpp:4314 |

<!-- src: common/arg.cpp listed lines; common/common.h:363-364, 684-686 -->

## Carried from source trees, not run on our machines

These come with code we carry from other forks. We build them but have no hardware to run them.

| Variables | Backend | Origin |
|---|---|---|
| `GGML_CUDA_MOE_CACHE`, `_ADMIT_AFTER`, `_BUDGET_MB`, `_DEDICATED_MMV`, `_FAIL`, `_HOT_USES`, `_INSERTS`, `_MAX_BATCH`, `_MIN_CC`, `_MIN_EXPERT_KB`, `_MODE`, `_NDEV`, `_OVERLAP_CPU_ROWS`, `_QUEUE`, `_QUEUE_MB`, `_RESERVE_MB`, `_SERIAL_FILL`, `_STATS`, `_THROTTLE` | CUDA MoE expert cache | TheTom/llama-cpp-turboquant (ggml/src/ggml-cuda/moe-cache.cu, ggml/src/ggml-moe-cache-common.h) |
| `GGML_CUDA_TQ3_4S_FP4`, `_CACHE`, `_CACHE_EXCLUDE`, `_CACHE_INCLUDE`, `_CACHE_LOG`, `_EXCLUDE`, `_INCLUDE`, `_TRANSIENT` | CUDA TQ3_4S | turbo-tan/llama.cpp-tq3 (ggml/src/ggml-cuda/mmq.cu) |
| `GGML_TQ_NATIVE` (default on; `1` turns it off), `TQ3_FUSE_OFF` | CUDA TQ | ggml/src/ggml-cuda/ggml-cuda.cu:826, 2002 |
| `GGML_TURBO_MMA_FUSED` (default on; `0` turns it off), `TURBO_INNERQ`, `TURBO_INNERQ_STRENGTH` | CUDA TurboQuant KV | ggml/src/ggml-cuda/fattn.cu:367, turbo-quant.cuh:164, 172 |
| `TURBO_FLASH`, `TURBO_FORCE_4MAG`, `TURBO_FORCE_NONVEC`, `TURBO_PROFILE_MODE`, `TURBO_SPARSE_V`, `TQ_NO_ROTATE` | Metal TurboQuant / TQ | ggml/src/ggml-metal/ |

## Tool-only variables

Read only by tools in this repository, never by the server: `DETP_*` (12, `tools/arifi-op-probe/det-probe.cpp`),
`PROBE_*` (8, `tools/arifi-op-probe/`), `MMVW_N`, `MMVW_SCALE`, `MMVW_SHAPES`, `MMVW_TYPES`
(`tools/arifi-op-probe/mmv-width.cpp`), `MOE_TRACE_OUT` (`examples/moe-trace/`), `PERF_TRACE_PATH`
and `AZ_TRACE_PATH` (`powerinfer/`), `STRATA_MTP_REVISION` (`tools/qwen4exp-mtp-sidecar/mtp_fetch.py`).

<!-- src: source scan; tools/, examples/, powerinfer/ sites as listed -->
