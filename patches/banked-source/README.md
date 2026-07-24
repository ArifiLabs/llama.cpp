# Banked upstream source patches

These are **other projects' patches**, captured verbatim as source material for a port.
They are *not* part of the ArifiLabs series and must never be applied with
`git am patches/series/*.patch`.

| Directory | Origin | What it is |
|---|---|---|
| `prismml-ternary-g128/` | [PrismML-Eng/llama.cpp](https://github.com/PrismML-Eng/llama.cpp) | PrismML's own `format-patch` output for the ternary `Q2_0` g128 Vulkan/CPU work, banked while the port was blocked at apply. The ported result lives in the ArifiLabs series as patches `0020`-`0031`. |

They live here rather than under `patches/series/` because `patches/series/` is a **generated**
directory: `tools/arifi-sync/arifi_sync.py series regen` rewrites it wholesale from git, and
anything hand-placed inside it would be destroyed on the next regeneration.
