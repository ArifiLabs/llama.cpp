# NVMe expert candidate probe

Created: 2026-10-03
Last Updated: 2026-10-03
Status: Experimental; GPU streaming acceptance pending
Owner: moe-x1-c2-10031938

EXECUTOR-STAMP: provider=openai model=gpt-6.1-sol effort=medium binding=validated-argv runtime=unverified doubt=RUNTIME_IDENTITY_UNVERIFIED

From PowerShell, run the fixture probe (creates only this named lane file):

```powershell
& C:/ArifiLabs/cache/moe-x1-lane/b-c2-final/bin/llama-moe-disk-probe.exe C:/ArifiLabs/cache/moe-x1-lane/direct-probe-repeat.bin
```

Read-only model-prefix probe:

```powershell
& C:/ArifiLabs/cache/moe-x1-lane/b-c2-final/bin/llama-moe-disk-probe.exe D:/ArifiLabs/models/hf/ornith-ai/Ornith-1.5-35B-A3B-GGUF/Ornith-1.5-35B-Q4_K_M.gguf readonly
```

Expected exit 0, four byte comparisons including the actual file EOF, range/failure rejection, registry cleanup. Read-only mode compares against small buffered reference reads and never writes the model. Failure RED is a synthetic callback failure, not a production ReadFile exception or GPU numerical test.

Streaming is opt-in with `GGML_ARIFI_MOE_NVME=1`, `GGML_ARIFI_MOE_NVME_CACHE_MIB=<numeric --moe-cache budget>`, `-lm mmap`, and one-token microbatches. `GGML_ARIFI_MOE_NVME_POLICY=none` invalidates unread cached slots at each node for a plain direct-read control. This still computes experts on Vulkan. It is not CPU mmap inference. Weight-plus-cache local leases are limited to 70 GiB; the loader reserves 4 GiB for compute/KV and assigns whole expert tensors within the remaining resident allowance. Overflow mappings are address identities only; no page prefetch, mlock, CPU expert arithmetic, or scheduler tensor copies are permitted. Cache slabs are pure device-local; staging growth is bounded and allocation failures abort rather than compute CPU experts.

Only `cell-c2.ps1` in the lane provides the external machine guard. Inspect REPORT.md and current receipts before promotion. Do not run retired `-ncmoe`, host-slab, or unguarded predecessors. Timed work remains HQ-owned.
