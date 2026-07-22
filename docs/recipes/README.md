# Recipes

This directory will hold reproducible, evidence-linked procedures for model
production and local serving. Recipe additions must identify source revision,
hardware path, exact commands, validation criteria, and measured outcome.

Planned recipe families:

- NVFP4 compressed-tensors to GGUF;
- Q6_K vocabulary-head recompression;
- MTP draft extraction, compatibility checks, and `n-max` sweeps;
- Q4_0 expert-bundle generation and SSD streaming;
- TurboQuant KV usage and Vulkan limitations;
- PrismML ternary `Q2_0_g128` benchmarking;
- Windows IOCP-overlap validation once an actual implementation exists.

A recipe is not a performance claim. It must distinguish observed capability,
placement-specific measurements, and unmeasured future work.
