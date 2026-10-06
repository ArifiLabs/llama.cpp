# Lane 299 runbook: qwen4exp decode fusions (Vulkan, X1)

Rerun recipe for a weaker model. Scripts live in `C:/ArifiLabs/cache/fusion-x1-lane/`.

1. Build. Configure from the x1i recipe (`cache/x1/hq74-build.ps1` + the b-e-x1 `CMakeCache.txt`, never a partial
   cache). Rebuild with `cache/fusion-x1-lane/build-9.ps1` (WinLibs first on PATH, ccache, Idle priority,
   targets `llama-server test-backend-ops llama-quantize llama-perplexity`, `-j1` when `ggml-vulkan.cpp.obj` rebuilds,
   start only with 7 GB available, kill below 2 GB). Queue it through `cache/gpu-queue/3-fusion.ready`.
   Copy the four WinLibs runtime DLLs (libstdc++-6, libgcc_s_seh-1, libwinpthread-1, libgomp-1) beside `build/bin`.
2. Op gate (a). `test-backend-ops test -b Vulkan0 -o <OP>` from a PowerShell file that calls `SetErrorMode(0x8003)`
   (template `optest-7.ps1`). Run fused, planted (must go RED), and off arms. A fused arm counts only if the matcher
   debug env prints MATCH > 0 (a green test with no MATCH is vacuous).
3. Served gates (b)+(c). Template `run-ab6.ps1`: llama-server only, `-lm dio --lazy-mode on`, 2 GB alarm, census arms
   with `GGML_VK_PERF_LOGGER=1` first (fail fast when the served graph never fuses), then 4+ interleaved rounds of
   short + 597-token prompts, 64 greedy tokens. Read with `compare-ab.py`, per-op diff `ab6-opdiff.py`, bootstrap CI
   `ab6-ci.py` (calls `interleaved_ab.significant`).
4. Noise floor. Two identical arms at 5 interleaved rounds read -1.63% / -1.10% (ab5). A reading inside about 2% is
   not a win, whatever the bootstrap says at n=4-5.

Measured state (2026-10-06): HC_POST_W WIN, GDN_BANK WIN (both on this branch). MUL_MAT_SCALE_SILU and CONV_BANK
reverted (no gain). `LLAMA_RING_OFF=1` is -5.7% vs stock: it routes the GDN state through slow GET_ROWS + CPY.
