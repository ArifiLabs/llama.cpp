# GGUF Q2_0-g128 retagger

`retag_g128.py` verifies that a GGUF file's on-disk Q2_0 tensor spans are
unambiguously compatible with the 128-value Q2_0 geometry, then writes
`GGML_Q2_0_G128=1` into a new GGUF copy.

It is for the ArifiLabs ternary-g128 llama.cpp fork series. It does not
quantize, dequantize, or otherwise alter model tensor bytes.

## Prerequisite

Run it with a Python environment containing the fork's `gguf-py` package:

```text
uv pip install --python <python> -e gguf-py
```

The tool uses `gguf-py` to parse the input's metadata, tensor table, declared
alignment, and Q2_0 tensor dimensions. It copies the raw tensor-data region
byte-for-byte because these g128 files intentionally serialize their tensors
with the existing Q2_0 wire type; a generic g64 Q2_0 rewrite could calculate
the wrong payload size.

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

`GGML_Q2_0_G128=1` is a per-file compatibility declaration. A loader can make
the file self-describing: the marker is present only on a verified g128 GGUF,
and an unmarked file continues through the canonical upstream Q2_0 g64 path.

An ambient environment variable would apply to whichever file happened to be
loaded in that process. That makes g64 and g128 interpretation depend on
external process state instead of the model artifact itself. The metadata-key
gate avoids that ambiguity.

The tool writes the key as a scalar GGUF `UINT32` value of `1`. The fork loader
accepts that representation for the gate.

## Fail-closed geometry check

The retagger uses the same span-size versus alignment math as the loader guard:

| Layout | Values per block | Bytes per block |
|---|---:|---:|
| g64 Q2_0 | 64 | 18 |
| g128 Q2_0 | 128 | 34 |

For every declared Q2_0 tensor, it calculates:

```text
g64 payload  = rows * (columns / 64)  * 18
g128 payload = rows * (columns / 128) * 34
```

It then compares the tensor's on-disk span—the distance to the next tensor
offset, or to the end of the tensor-data region—against each candidate's raw
size and alignment-padded size using the GGUF file's declared alignment.

The tool refuses with a non-zero exit when:

- the input is missing, is not a regular file, or does not begin with `GGUF`;
- `gguf-py` cannot parse the file;
- there are no Q2_0 tensors;
- a Q2_0 tensor is g64-only;
- a Q2_0 tensor matches neither geometry;
- no Q2_0 span is a g128-only discriminator, including when every span is
  alignment-ambiguous;
- the marker already exists, which prevents duplicate GGUF metadata keys; or
- the destination already exists in copy mode.

An individual alignment-ambiguous tensor is not evidence for either layout.
It is allowed only when another Q2_0 tensor in the same file unambiguously
confirms g128. This exactly preserves the loader guard's rule: do not silently
reinterpret an ambiguous model.
