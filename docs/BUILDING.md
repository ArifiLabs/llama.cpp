# Building this fork

Upstream's generic instructions are in [`build.md`](build.md) and they still apply. This page
covers only what is **specific to this fork or to the toolchain it is developed on**, including
three flags whose absence produces build or runtime failures that name neither the flag nor the fix.

Every claim on this page is labelled with how it was established. `MEASURED` means a run on the
hardware named in [scope](#scope-of-these-instructions); `UNTESTED` means exactly that.

---

## 0. Before you clone: Windows path length

`git clone` of this repository **fails on Windows** unless long paths are enabled:

```
error: unable to create file tools/ui/src/lib/components/app/chat/ChatAttachments/ChatAttachmentsPreview/ChatAttachmentsPreviewCurrentItem/ChatAttachmentsPreviewCurrentItemUnavailable.svelte: Filename too long
fatal: unable to checkout working tree
```

That path is **161 characters** and comes from upstream `b10068` — it is not something this fork
added. Windows' default `MAX_PATH` is 260, so it survives only if your clone directory is shorter
than about 98 characters. Either clone somewhere shallow, or turn long paths on:

```bash
git clone -c core.longpaths=true <this repo> arifilabs-llama.cpp
```

`MEASURED 2026-07-25`: clone into a 115-character prefix fails exactly as above; the same clone
with `core.longpaths=true` succeeds.

---

## 1. Prerequisites

| Component | What we develop against | Notes |
|---|---|---|
| Compiler | **WinLibs MinGW-w64 GCC 14.2.0** (UCRT, POSIX threads, SEH) | The flags in §2 are specific to this toolchain. MSVC and clang-cl are **UNTESTED here**. |
| CMake | 4.3.3 | Any version upstream supports should work. |
| Generator | Ninja 1.13.2 | Not required; it is what the recorded recipe uses. |
| Vulkan SDK | 1.4.350.0 | **Only** for a Vulkan build. A CPU-only build needs no SDK. |
| Python | 3.8+ | For `tools/arifi-sync/` and the GGUF tooling. |

---

## 2. The three flags that are not optional on MinGW

These are not tuning. Each one prevents a specific failure, and the failure message does not
mention the flag. This fork lost three separate builds to them, one flag at a time.

### `-D_WIN32_WINNT=0x0A00` — required, or the build does not link

Without it, the build **fails at roughly 262/346 targets**:

```
vendor/cpp-httplib/httplib.cpp:1471:9: error: '::CreateFile2' has not been declared; did you mean 'CreateFileW'?
 1471 |       ::CreateFile2(wpath.c_str(), GENERIC_READ,
```

`CreateFile2` is a Windows 8+ API and MinGW-w64's headers gate it behind `_WIN32_WINNT`. WinLibs
defaults below `0x0602`, so the declaration is never emitted. `0x0A00` selects Windows 10.

Set it on **both** language flags — the failing translation unit is C++, but the C sources need the
same target level:

```
-DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0A00" -DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0A00"
```

`MEASURED 2026-07-25`, fresh clone, CPU-only, no Vulkan: without the flag the build stops at the
error above; with it, `llama-server` links and runs. It is **not** a Vulkan-only concern — a plain
CPU-only build fails identically.

### `-static-libgcc -static-libstdc++` — required, or the binary can die at startup

```
-DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++"
```

Without a static GCC runtime, any other `libstdc++-6.dll` earlier on `PATH` shadows the one the
binary was built against. That produced `0xC0000139` (`STATUS_ENTRYPOINT_NOT_FOUND`) when
`ui-assets.cmake` ran the asset-embed helper. Linking the runtime statically makes the binaries
immune to DLL shadowing. See [`OPTIONS-REGISTRY.md` §Build variants](OPTIONS-REGISTRY.md#build-variants).

### The web UI is on by default, and offline

This fork carries a pinned copy of the web UI (`tools/ui/dist.tar.gz`, checked against
`tools/ui/dist.tar.gz.sha256` at configure time). It takes priority over the npm build and the
network download, so every build embeds the same UI without network access or Node.js. To leave the
UI out of a minimal or CI build, see §4.

---

## 3. CPU-only build (no GPU, no Vulkan SDK)

This is the path to use if you have no AMD Vulkan device, or if you want the CPU kernels.

```bash
cmake -S . -B build-cpu -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0A00" \
  -DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0A00" \
  -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++"

cmake --build build-cpu --target llama-server -j 6
```

`MEASURED 2026-07-25` on a fresh clone: configure and build both succeed, `llama-server --version`
reports `10141 (bc626ba1e) built with GNU 14.2.0 for Windows AMD64`, and `--list-devices` prints an
empty device list. **Nothing in this fork requires a GPU backend to be present**; the Vulkan
backend is simply not compiled and the CPU backend is selected.

**If you are CPU-only, read [`HARDWARE-PROFILES.md`](HARDWARE-PROFILES.md) before benchmarking.**
Several defaults in this fork are tuned for a GPU-offloaded rig and are the wrong choice for you —
`GGML_ARIFI_VNNI_REPACK` in particular ships OFF and is a large measured win on a CPU-only path.

## 4. Vulkan build (the rig this fork is developed on)

```bash
cmake -S . -B build-vulkan -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DGGML_VULKAN=ON \
  -DGGML_ARIFI_ROCMFPX_FORMATS=ON -DGGML_ARIFI_TURBO_WEIGHT_QUANTS=ON \
  -DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0A00" \
  -DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0A00" \
  -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++"

cmake --build build-vulkan --target llama-server -j 6
```

The server this builds includes the **web UI**: run it and open `http://127.0.0.1:8080`. The UI
comes from the pinned `tools/ui/dist.tar.gz` (see §2), so the build stays offline and repeatable.
The pinned copy is embedded whenever the server is built (`scripts/ui-assets.cmake` uses
`tools/ui/dist` first), so `-DLLAMA_BUILD_UI=OFF` and `-DLLAMA_USE_PREBUILT_UI=OFF` do not remove
it in this fork; they only switch off the npm build and the network download. To run a server
without the UI, start it with `--no-ui` (environment `LLAMA_ARG_UI=0`). The UI touches no decode
path, so it does not change any speed number.

## 4.5 Make `bin/` self-sufficient before you run or copy it

`-static-libgcc -static-libstdc++` removes two runtime DLLs, not all of them. A MinGW build still
imports `libgomp-1.dll` (OpenMP), `libwinpthread-1.dll` and, through `libgomp-1`, `libdl.dll`; a
build configured without the static flags also imports `libstdc++-6.dll` and `libgcc_s_seh-1.dll`.
If any of them is found only on `PATH`, the binary depends on whatever `PATH` holds when it starts:
missing = `0xC0000135` (`STATUS_DLL_NOT_FOUND`), a foreign copy first on `PATH` (Git for Windows
ships one) = `0xC0000139`. The Windows loader searches the executable's own directory **before**
`PATH`, so copying the DLLs **of the toolchain that built the binary** next to it ends both failures:

```powershell
$tc = Split-Path (Get-Command g++).Source      # the compiler that ran the build
foreach ($b in 'build-vulkan\bin', 'build-cpu\bin') {
  if (Test-Path $b) {
    foreach ($d in 'libstdc++-6', 'libgcc_s_seh-1', 'libwinpthread-1', 'libgomp-1', 'libdl') {
      if (Test-Path "$tc\$d.dll") { Copy-Item "$tc\$d.dll" $b -Force }
    }
  }
}
```

Copy them fresh from the toolchain every time; never from an older build directory, because a
toolchain upgrade makes the old copies foreign to the new binary. Then prove it with a `PATH` that
holds Windows only (run it in a new PowerShell window, or `$env:PATH` stays changed):

```powershell
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
build-vulkan\bin\llama-server.exe --version
```

It prints the version and build line and exits 0. Linking fully static (`-static`) was evaluated
and not adopted for this release.

## 5. Reproducing our exact configuration

`tools/arifi-sync/recipe/build-vulkan.cache-snapshot.txt` is the **committed** record of the real
build: all 310 non-internal CMake cache entries. You do not need a build directory to read it:

```bash
python tools/arifi-sync/arifi_sync.py recipe export \
    --live tools/arifi-sync/recipe/build-vulkan.cache-snapshot.txt \
    --initial-cache arifi-recipe.cmake
```

**Do not feed that file to `cmake -C` unchanged on another machine.** 35 of the 310 entries are
absolute paths into *our* WinGet, Ninja and Vulkan SDK installs (`CMAKE_C_COMPILER`,
`CMAKE_CXX_COMPILER`, `CMAKE_MAKE_PROGRAM`, `Vulkan_*`, the binutils set). Treat the snapshot as
the authoritative *reference* for what was set, and use the explicit commands in §3/§4 to build.

Once you have your own configured build directory, `recipe diff` will tell you every entry where
you differ from us — which is how a missing flag gets found without guessing:

```bash
python tools/arifi-sync/arifi_sync.py recipe diff --live build-cpu/CMakeCache.txt
```

## 6. Reproducing the patch series

Do **not** try to `git am patches/series/*.patch` after checking out the upstream base: that
directory does not exist at the base commit, and the shell would have nothing to expand. Use the
driver, which replays into its own worktree:

```bash
python tools/arifi-sync/arifi_sync.py series replay --onto 9e0e220594af405a62835dc3a27495729fd8506b
python tools/arifi-sync/arifi_sync.py series check      # regenerate, byte-compare, replay, diff
```

`MEASURED 2026-07-25`: `series check` passes from a completely fresh clone — 62 patches replay to a
tree identical to `master` outside the generated series directory.

---

## 7. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `git clone` → `Filename too long` / `unable to checkout working tree` | Upstream's 161-char `tools/ui/...` path exceeds `MAX_PATH` | `git clone -c core.longpaths=true`, or clone to a shallower directory (§0) |
| `'::CreateFile2' has not been declared` at `vendor/cpp-httplib/httplib.cpp:1471` | `_WIN32_WINNT` too low on MinGW | `-D_WIN32_WINNT=0x0A00` on both C and CXX flags (§2) |
| Binary exits immediately with `0xC0000139` (`STATUS_ENTRYPOINT_NOT_FOUND`) | A foreign `libstdc++-6.dll` on `PATH` shadows the build's runtime | `-static-libgcc -static-libstdc++` (§2), then §4.5 |
| Binary exits immediately with `0xC0000135` (`STATUS_DLL_NOT_FOUND`) | A runtime DLL (often `libdl.dll` or `libgomp-1.dll`) is not beside the binary and not on `PATH` | §4.5 |
| **`0xC0000005` ACCESS_VIOLATION during decode, after `CPU_REPACK model buffer size` is logged** | **Upstream's 8×8 repack kernels. Not this fork.** See below. | Drop `--no-host`, or use a quant whose repack path is 4-wide |
| `git am` → `unable to auto-detect email address` | No git identity configured | `git config user.email` / `user.name` |
| `series check` byte-compare fails right after a `git` upgrade | Patch signature line changed | `series regen` — the series is generated, never hand-edited |

### `0xC0000005` in an 8×8 repack kernel — a real crash you can hit blind

Passing **`--no-host`** makes the `CPU_REPACK` buffer type reachable. Three of upstream's five
repack types then abort with `0xC0000005` ACCESS_VIOLATION, *after* the buffer is allocated:

- `Q4_0` (`q4_0_8x8`), `IQ4_NL` (`iq4_nl_8x8`), `MXFP4` (`mxfp4_8x8`)
- gdb frame 0 is `ggml_gemv_q4_0_8x8_q8_0`, frame 1
  `ggml::cpu::repack::tensor_traits<block_q4_0, 8, 8, GGML_TYPE_Q4_0>::compute_forward`,
  reached from `llama_decode` on a libgomp worker.

**Attribution is settled: this reproduces on stock upstream `b10068` on the identical model and
box, so it is upstream's, not ours.** The 4-wide kernels — this fork's `q2_0_4x8` and upstream's
`q2_K_8x8` / `q4_K_8x8` Q8_K pair — run fine.

You will only meet this if you pass `--no-host`, which you should only be doing when you are
deliberately A/B-testing a CPU repack path. `MEASURED` on the hardware in
[scope](#scope-of-these-instructions); whether it reproduces on other CPUs is **UNTESTED**.
Full evidence is in the `GGML_ARIFI_VNNI_REPACK` row of [`OPTIONS-REGISTRY.md`](OPTIONS-REGISTRY.md).

---

## Scope of these instructions

Everything marked `MEASURED` on this page was measured on:

- **Beelink SER7**, AMD Ryzen 7 7840HS, **Radeon 780M (gfx1103)**, unified memory, ~32 GB RAM
- **Windows 11**, WinLibs MinGW-w64 GCC 14.2.0 (UCRT/POSIX/SEH), CMake 4.3.3, Ninja 1.13.2
- Vulkan SDK 1.4.350.0 for the Vulkan build

**UNTESTED anywhere in this repository:** Linux, macOS, MSVC, clang-cl, CUDA, ROCm/HIP, Arm/NEON,
and every non-gfx1103 GPU. Upstream supports all of them and this fork does not remove that
support — but no one has built or run this fork on them, and we will not imply otherwise.

---

## The off-rig backends, and how they are verified

The box above is the only hardware this project owns, so CUDA, Metal, SYCL, HIP, OpenCL and WebGPU
have **no toolchain here**. That is a verification gap, not a scope decision: this engine is a
distributable product and its off-rig backends are somebody else's only backend.

`.github/workflows/arifi-backends-matrix.yml` is what closes it. Four jobs, all **compile + link
only**, all with the ArifiLabs weight formats switched **ON**:

| job | runner | configure |
|---|---|---|
| `cuda-ubuntu` | `ubuntu-24.04`, `nvidia/cuda:12.6.2-devel` container | `-DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=89-real` |
| `metal-macos` | `macos-latest` | `-DGGML_METAL=ON -DGGML_METAL_EMBED_LIBRARY=ON` |
| `sycl-ubuntu` | `ubuntu-24.04` + oneAPI | `-DGGML_SYCL=ON`, `icx`/`icpx` |
| `vulkan-windows` | `windows-latest` + Vulkan SDK | `-DGGML_VULKAN=ON` — the rig's own path, on a second toolchain |

Every job also runs `test-quantize-fns`, which is a pure-CPU correctness test, and uploads its full
build log as an artifact.

**Why this is not redundant with upstream's workflows.** `build-cuda-ubuntu.yml`, `build-apple.yml`,
`build-sycl.yml` and `build-vulkan.yml` are kept and still run — but they configure with default
options, and `GGML_ARIFI_ROCMFPX_FORMATS` and `GGML_ARIFI_TURBO_WEIGHT_QUANTS` both default to `OFF`
([`ggml/CMakeLists.txt`](../ggml/CMakeLists.txt) lines 144 and 151). Upstream's matrix therefore
compiles this fork with the code that differentiates it switched off. This matrix turns it on.

**What a green run does and does not prove.** GitHub's runners have no NVIDIA, Apple or Intel GPU.
Nothing in this matrix executes a GPU kernel and nothing in it is a benchmark. A green run licenses
exactly one claim — **"compile-verified, not benched"** — and any report that says more than that
about an off-rig change is overclaiming. Numbers still come from the rig, under the bench-purity
rules above.
