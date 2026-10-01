<p align="center"><img src="media/arifilabs-llama-cpp-banner.webp" alt="ArifiLabs llama.cpp" width="820"></p>

# ArifiLabs llama.cpp

**Qwen3.8 27B at up to 9.5 tokens/s on an AMD mini-PC with integrated graphics. Vulkan, no discrete GPU.**

A llama.cpp fork built for AMD integrated GPUs. It gives Vulkan kernels to quant formats that only
CUDA or Metal users could run, makes the formats you already use faster, and keeps DFlash2 and MTP
speculative decoding stable on Qwen3.8. Built on upstream llama.cpp `b10825`.

## Speed: Qwen3.8 27B on a Radeon 780M

| 27B model file | Decode with DFlash2 drafting | Notes |
|---|---:|---|
| ROCmFP4-FAST (julianmb, 14.6 GB) | **9.5 t/s** | our q8_1 mat-vec kernel: 7.5 → 9.5 t/s, +27% ¹ |
| UD-Q3_K_XL (outsourc-e Unleashed, 13.2 GB) | **9.3 t/s** | our IQ3_S mat-vec kernel: 8.4 → 9.3 t/s, +11% |
| GSQ-RCO IQ3_S (ISTA-DASLab, 12.1 GB) | **8.1 t/s** | 5.1 t/s plain decode on the same build |
| S-X8 v4.3 (MarlaLabs, 24.3 GB) | **5.6 t/s** | 3.0x: this file did 1.8 t/s plain, and drafting crashed at load |

Measured with `llama-server` on a Beelink SER7 Pro (Ryzen 7 7840HS, Radeon 780M, DDR5-5600, Windows 11),
September 2026. Each "before" is the previous build of this fork on the same file.
¹ One switch: `GGML_ARIFI_ROCMFP4_MMVQ=1`.

**Speed is never bought with quality.** Our latest kernel release kept 27B perplexity flat
(Q4_K_XL 5.9932 → 5.9928) and greedy output bit-identical (6 of 6 runs).

**Runs on the Radeon 890M too:** 27B models load fully into GPU memory, 23.4 GiB for the largest we tried.

## Quick start (Windows, Vulkan)

```powershell
git clone -c core.longpaths=true <this repo> arifilabs-llama.cpp; cd arifilabs-llama.cpp
cmake -S . -B build-vulkan -G Ninja -DCMAKE_BUILD_TYPE=Release -DGGML_VULKAN=ON `
  -DGGML_ARIFI_ROCMFPX_FORMATS=ON -DGGML_ARIFI_TURBO_WEIGHT_QUANTS=ON `
  -DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0A00" -DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0A00" `
  -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" `
  -DLLAMA_USE_PREBUILT_UI=OFF -DLLAMA_BUILD_UI=OFF -DLLAMA_BUILD_WEBUI=OFF -DLLAMA_USE_PREBUILT_WEBUI=OFF
cmake --build build-vulkan --target llama-server -j 2
build-vulkan\bin\llama-server.exe -m Qwen3.8-27B-ROCmFP4-FAST.gguf -dev Vulkan0 -ngl 999 -fa on -c 8192 `
  -ctk q8_0 -ctv q8_0 --spec-type draft-dflash -md Qwen3.8-27B-DFlash2-Q4_K_M.gguf --spec-draft-n-max 2
```

The drafter is incoai's `Qwen3.8-27B-DFlash2-GGUF` (Q4_K_M). Toolchain, flags and the MinGW runtime
step are in [`docs/BUILDING.md`](docs/BUILDING.md). At startup the Vulkan backend prints one line for
each tuned default it applied on your GPU.

## Hardware

- **Tuned on:** AMD Radeon 780M (RDNA3), one shared pool of DDR5 system memory. Every speed number on
  this page comes from that box.
- **Running now on:** Minisforum AI X1 Pro-470, Ryzen AI 9 HX 470, Radeon 890M, 96 GB DDR5-5600 with
  72 GB reserved for the GPU. A placement fix for this GPU puts the whole model in the GPU reservation.
- **Other GPUs:** tuned defaults switch on by device probe (AMD, RDNA3); every other device runs the
  upstream code paths. All upstream backends (CPU, CUDA, Metal, SYCL and others) stay in the tree.

## What it adds over upstream

### Quant formats with GPU kernels

Every format below has Vulkan shaders in this tree. The weight formats pass `test-backend-ops` on the
GPU against the CPU reference, with 0 failures.

| Format | Format author | Vulkan | CPU | CUDA | Metal |
|---|---|---|---|---|---|
| S-X8 v4.3, 7.5 bpw (type 57) | MarlaLabs | kernel | kernel | none | none |
| ROCmFP4, ROCmFP4-FAST | charlie12345 | kernel | kernel | none | none |
| ROCmFPX FP2 / FP3 / FP6 / FP8 | charlie12345 | kernel | kernel | none | none |
| TQ3_1S, TQ4_1S | TheTom (llama-cpp-turboquant) | kernel | kernel | kernel ² | kernel ² |
| TQ3_4S family (types 48-51) | turbo-tan (llama.cpp-tq3) | kernel | kernel | kernel ² | kernel ² |
| TurboQuant KV cache (turbo2/3/4) | TheTom (llama-cpp-turboquant) | kernel | kernel | kernel ² | kernel ² |
| Q2_0_G128 ternary (type 43) | PrismML | kernel | kernel + VNNI repack | none | none |
| Escha-W2, 2 and 3 bpw (types 55/56) | EschaLabs | fused kernel | kernel | none | none |

² Carried from the format author's tree; we build and test on Vulkan and CPU.

### Faster kernels for formats you already use

Speculative decoding verifies several tokens per step, so mat-vec speed at widths 2-8 decides drafted
decode. These kernels target exactly those widths. Off switches restore the upstream path.

| Kernel | Measured effect (Radeon 780M) | Default |
|---|---|---|
| IQ3_S mat-vec, 16 threads per superblock at verify widths | Q3_K_XL 27B drafted 8.37 → 9.27 t/s, +10.8% | on |
| IQ4_XS dedicated mat-vec shader | IQ4_XS 27B drafted +0.62 t/s, 8 of 8 cells | on |
| iq3 mat-vec at width 7: register spill removed | 465,400 → 20,890 µs per op, 22x | on (RDNA3), `GGML_ARIFI_IQ3_N7_ROWS=4` reverts |
| q6_K on the q8_1 MMVQ path at widths 7-8 | lm_head 20,046 → 14,313 µs, 1.40x | on (AMD), `GGML_ARIFI_Q6K_MMVQ=legacy` reverts |
| q4_K width-5 split | lm_head 12,749 → 9,747 µs, 1.31x | on (RDNA3), `GGML_ARIFI_Q4K_W5_SPLIT=0` reverts |
| q5_K on the q8_1 MMVQ path at widths 5-8 | +7% to +20% per verify column | on (AMD), `GGML_ARIFI_Q5K_MMVQ=0` reverts |
| iq3_s two rows per workgroup at width 6 | 6th verify column 47.2 → 23.8 ms per step | on (RDNA3) |
| S-X8 decode kernel, A-hoist and MMVQ route | plain 1.83 → 2.25 t/s; drafted 3.33 → 4.48 t/s; 71-73 GB/s at widths 4-8 | on |
| TQ subgroup mat-vec | TQ3_4S 27B 0.67 → 1.80 t/s; TQ4_1S 0.5B 44.7 → 126.7 t/s | on |
| ROCmFP4-FAST q8_1 MMVQ | drafted 7.50 → 9.52 t/s, +26.9% | `GGML_ARIFI_ROCMFP4_MMVQ=1` |
| Escha-W2 column-blocked matmul | 3.51x on the prefill shape | on |
| Q2_0_G128 VNNI repack (CPU) | decode 2.01 → 6.69 t/s, 3.3x | `GGML_ARIFI_VNNI_REPACK=2` |

### Speculative decoding, MoE and memory

- **Stable DFlash2 and MTP drafting on Qwen3.8.** Attaching a drafter no longer corrupts the recurrent
  state of hybrid models. Replayed draft tokens are not re-verified after a checkpoint restore, which
  stops a slot loop on Vulkan. A rejected draft checkpoint no longer stops the server.
- **Adaptive draft length:** `--spec-draft-adaptive` sizes each draft from the measured acceptance.
- **Bigger-than-RAM MoE:** expert streaming from SSD (`EXPERT_BUNDLE_PATH`) runs a 35B model from a
  17 GB expert bundle.
- **Placement that fills the GPU reservation** on the Radeon 890M (`GGML_VK_UMA_PLACEMENT`), plus a
  per-heap allocator that serves 27B drafted lines which used to fail at load.

## For format authors and researchers

You were sent this link because your format runs here on the GPU. Find your format below.

**S-X8 v4.3 (MarlaLabs).** Vulkan: `dequant_sx8.comp`, `mul_mat_vec_sx8.comp`, a q8_1 MMVQ path and a
packed cooperative-matrix tile for prefill. Qwen3.8 27B S-X8: 5.55 t/s drafted, 2.25 t/s plain, mat-vec at
71-73 GB/s on a 780M, close to the memory bus.
```powershell
python tools/gguf-retag-sx8/retag_sx8.py model-sx8.gguf model-sx8.57.gguf   # your type id 41 -> our 57
build-vulkan\bin\llama-server.exe -m model-sx8.57.gguf -dev Vulkan0 -ngl 999 -fa on -c 8192
```

**ROCmFP4 and ROCmFPX (charlie12345).** Vulkan: `dequant_rocmfp4.comp`, `dequant_rocmfp4_fast.comp`,
`dequant_rocmfpx_fp2.comp`, `_fp3`, `_fp6`, `_fp8`, mat-vec and get-rows pipelines for all six types, and a
q8_1 MMVQ path for ROCmFP4-FAST. Qwen3.8 27B ROCmFP4-FAST: 9.52 t/s drafted (from 7.50).
```powershell
cmake ... -DGGML_ARIFI_ROCMFPX_FORMATS=ON          # the quick-start build already has it
$env:GGML_ARIFI_ROCMFP4_MMVQ="1"
build-vulkan\bin\llama-server.exe -m Qwen3.8-27B-ROCmFP4-FAST.gguf -dev Vulkan0 -ngl 999 -fa on -c 8192
```

**TQ3_1S, TQ4_1S, TQ3_4S and TurboQuant KV (TheTom, turbo-tan).** Vulkan: `dequant_tq3_1s.comp`,
`dequant_tq4_1s.comp`, `dequant_tq3_4s.comp`, `mul_mat_vec_tq3_1s.comp`, `mul_mat_vec_tq4_1s.comp`,
`mul_mat_vec_tq3_4s.comp`, the subgroup mat-vec `mul_mat_vec_tq_sg.comp`, `tq_rotate_act.comp`,
`dequant_turbo3_0.comp` and `turbo_wht.comp`. TQ3_4S 27B: 0.67 → 1.80 t/s; TQ4_1S 0.5B: 44.7 → 126.7 t/s.
```powershell
cmake ... -DGGML_ARIFI_TURBO_WEIGHT_QUANTS=ON       # the quick-start build already has it
python tools/gguf-retag-tq3/retag_tq3.py model-tq3_4s.gguf model-tq3_4s.arifi.gguf   # tq3 ids -> 48-51
build-vulkan\bin\llama-server.exe -m model-tq3_4s.arifi.gguf -dev Vulkan0 -ngl 999 -fa on
```

**Q2_0_G128 ternary (PrismML).** Vulkan: `dequant_q2_0_g128.comp` plus mat-vec. CPU: a VNNI repack
with GEMV and GEMM kernels, 2.01 → 6.69 t/s decode (3.3x) with `GGML_ARIFI_VNNI_REPACK=2`.
```powershell
python tools/gguf-retag-g128/retag_g128.py model-q2_0.gguf   # writes model-q2_0.g128.gguf
build-vulkan\bin\llama-server.exe -m model-q2_0.g128.gguf -dev Vulkan0 -ngl 999
```

**Escha-W2 (EschaLabs).** Native types 55 and 56 with a fused `GGML_OP_ESCHA_MM`
(`escha_mm.comp`): Hadamard rotation, code decode and matmul in one dispatch. Decode is coherent on
2- and 3-bit files; column blocking makes the prefill shape 3.51x faster. Load the GGUF with its
`.escha_aux` sidecar beside it; `test-backend-ops -o ESCHA_MM` checks it against the CPU.

**K-quants and i-quants (upstream formats).** Dedicated or retuned Vulkan mat-vec kernels for IQ4_XS
(`mul_mat_vec_iq4_xs.comp`), IQ3_S and IQ3_XXS, Q4_K, Q5_K and Q6_K. See the kernel table above. Every
switch and its default is in [`docs/OPTIONS-REGISTRY.md`](docs/OPTIONS-REGISTRY.md).

## Built to rebase

- **A linear patch series** on upstream `b10825` (`9e0e220594af405a62835dc3a27495729fd8506b`), zero
  merge commits. `python tools/arifi-sync/arifi_sync.py series check` regenerates it, replays it with
  `git am` and compares the result with this tree.
- **Runtime switches.** Each tuned default has an environment switch that restores the old path, listed
  in [`docs/OPTIONS-REGISTRY.md`](docs/OPTIONS-REGISTRY.md).
- **Protected wins.** [`tools/arifi-sync/protected-wins.json`](tools/arifi-sync/protected-wins.json) names
  the code each measured win depends on, so an upstream bump cannot drop one silently.
- **Provenance in every commit.** `Taken-from:` names the source repository and commit; `Measured-effect:`
  names the measured result. `python tools/arifi-sync/arifi_sync.py provenance` checks them all.

## Credits

This fork stands on other people's work. [`NOTICE`](NOTICE) names each licensed source with its licence.

- [ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp): the engine and the base of this fork.
- [LaurentZuijdwijk/llama.cpp](https://github.com/LaurentZuijdwijk/llama.cpp): the IQ3_S 16-thread mat-vec
  body, the tiled concat-transpose for the delta-net conv state, the draft replay-skip fix and adaptive
  draft sizing.
- [spiritbuun/buun-llama-cpp](https://github.com/spiritbuun/buun-llama-cpp): the DFlash2 adaptive
  controller and the checkpoint-rewind hardening.
- [charlie12345/ROCmFPX](https://github.com/charlie12345/ROCmFPX): the ROCmFP4 and ROCmFPX formats, their
  CPU paths, quantizer wiring and q8_1 mat-vec functions.
  [ciru-ai/ROCmFPX](https://github.com/ciru-ai/ROCmFPX): a Vulkan build fallback.
- [TheTom/llama-cpp-turboquant](https://github.com/TheTom/llama-cpp-turboquant): TurboQuant KV, the TQ3_1S
  and TQ4_1S weight formats and their kernels.
  [TheTom/turboquant_plus](https://github.com/TheTom/turboquant_plus): KV-fidelity tooling (Apache-2.0).
- [turbo-tan/llama.cpp-tq3](https://github.com/turbo-tan/llama.cpp-tq3): the TQ3_4S format family with its
  CUDA and Metal kernels.
- [Tiiny-AI/PowerInfer](https://github.com/Tiiny-AI/PowerInfer): sparse execution and SSD expert streaming.
  The Windows IOCP read path is built on it.
- [PrismML-Eng/llama.cpp](https://github.com/PrismML-Eng/llama.cpp): the Q2_0_G128 ternary format, the VNNI
  repack design and the f16 recurrent state.
- MarlaLabs (Martí Vidal Leandro): the S-X8 v4.3 format (Apache-2.0). The Vulkan kernels here are ours.
- [jtrefon/llama.cpp-turboq-mtp](https://github.com/jtrefon/llama.cpp-turboq-mtp): the TBQ3_0 and TBQ4_0
  KV cache types.
- [thecodacus/llama.cpp](https://github.com/thecodacus/llama.cpp): three host-transfer expert-prefetch
  patches.
- EschaLabs: the Escha-W2 code format.

## Next

Qwen3.8 Flash with NVMe offload, on the 96 GB Radeon 890M box.

## Requirements and limits

- Tested on Windows 11 with Vulkan; MinGW-w64 GCC is the build of record ([`docs/BUILDING.md`](docs/BUILDING.md)).
- Tuned for AMD integrated GPUs; on other GPUs the upstream code paths run.
- `GGML_ARIFI_ROCMFP4_MMVQ` and `GGML_ARIFI_VNNI_REPACK` are opt-in.
- Full engineering ledger, every protected win with its state: [`docs/arifi/STATUS.md`](docs/arifi/STATUS.md).

## License

[`LICENSE`](LICENSE) is upstream llama.cpp's MIT licence, verbatim. [`NOTICE`](NOTICE) names every
third-party component with its licence; the licence texts are in [`licenses/`](licenses/). S-X8 and
turboquant_plus are Apache-2.0, and their notices travel with this repository.
