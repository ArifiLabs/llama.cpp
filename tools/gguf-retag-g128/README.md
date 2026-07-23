# GGUF Q2_0-g128 retagger

`retag_g128.py` verifies that a GGUF file's on-disk Q2_0 tensor spans are
unambiguously compatible with the 128-value Q2_0 geometry, then writes
`GGML_Q2_0_G128=1` and retags only the verified tensor-info `ggml_type`
fields from `42` (`GGML_TYPE_Q2_0`) to `43` (`GGML_TYPE_Q2_0_G128`) in a
new GGUF copy.

It is for the ArifiLabs ternary-g128 llama.cpp fork series. It does not
quantize, dequantize, or otherwise alter model tensor bytes.

## Write contract

The tool uses only a bounded standard-library parser for the GGUF v3 header,
metadata, and tensor-info table; it never constructs or reads tensor payloads.
Before it writes any output bytes, it runs the fail-closed g64-versus-g128 span
discriminator over every legacy type-42 entry.

Only after that verification succeeds, it streams the payload unchanged and
rewrites exactly those verified four-byte type fields. Post-write validation
compares the entire tensor-info table byte-for-byte, allowing only those
`42 -> 43` substitutions; names, dimensions, offsets, and every other table
field must be identical.

## Usage

From the llama.cpp fork root:

```text
python tools/gguf-retag-g128/retag_g128.py model.gguf
```

The default output is a sibling file named `model.g128.gguf`. The source is
never modified by default.

Choose an explicit output path:

```text
python tools/gguf-retag-g128/retag_g128.py model.gguf --output model-g128.gguf
```

In-place replacement is available only through explicit opt-in:

```text
python tools/gguf-retag-g128/retag_g128.py model.gguf --in-place
```

`--in-place` prints a warning first, writes and validates a same-directory
temporary copy, then replaces the source only after that succeeds.

## Why this is metadata, not an environment variable

`GGML_Q2_0_G128=1` is the retained per-file audit and compatibility marker.
The native type-43 tag makes the tensor geometry self-describing to the GGUF
parser, while the marker retains the fork's explicit g128 declaration.

An ambient environment variable would apply to whichever file happened to be
loaded in that process. That makes g64 and g128 interpretation depend on
external process state instead of the model artifact itself. The metadata-key
gate avoids that ambiguity.

The tool writes the key as a scalar GGUF `UINT8` value of `1`. The fork loader
accepts that representation for the gate.

## Fail-closed geometry check

The retagger uses the same span-size versus alignment math as the loader guard:

| Layout | Values per block | Bytes per block |
|---|---:|---:|
| g64 Q2_0 | 64 | 18 |
| g128 Q2_0 | 128 | 34 |

For every declared legacy type-42 Q2_0 tensor, it calculates:

```text
g64 payload  = rows * (columns / 64)  * 18
g128 payload = rows * (columns / 128) * 34
```

It then compares the tensor's on-disk span—the distance to the next tensor
offset, or to the end of the tensor-data region—against each candidate's raw
size and alignment-padded size using the GGUF file's declared alignment.

The tool refuses with a non-zero exit when:

- the input is missing, is not a regular file, or does not begin with `GGUF`;
- there are no Q2_0 tensors;
- a Q2_0 tensor is g64-only;
- a Q2_0 tensor matches neither geometry;
- no Q2_0 span is a g128-only discriminator, including when every span is
  alignment-ambiguous;
- the marker already exists, which prevents duplicate GGUF metadata keys; or
- the destination already exists in copy mode.

It also refuses if post-write validation finds any tensor-info-table change
other than a verified `42 -> 43` type-field rewrite.

An individual alignment-ambiguous tensor is not evidence for either layout.
It is allowed only when another Q2_0 tensor in the same file unambiguously
confirms g128. This exactly preserves the loader guard's rule: do not silently
reinterpret an ambiguous model.
