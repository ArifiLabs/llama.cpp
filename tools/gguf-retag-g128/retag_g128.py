#!/usr/bin/env python3
"""Fail-closed GGUF Q2_0-g128 metadata retagger.

@capability: verify Q2_0-g128 GGUF geometry and write the GGML_Q2_0_G128=1 metadata key to a safe copy
@intent: retag g128 GGUF, mark Q2_0 g128 model, verify ternary GGUF geometry, add GGML_Q2_0_G128 metadata
@run: python tools/gguf-retag-g128/retag_g128.py <model.gguf>
"""

from __future__ import annotations

import argparse
import gc
import math
import os
import shutil
import struct
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

GGUF_MAGIC = b"GGUF"
METADATA_KEY = "GGML_Q2_0_G128"

G64_VALUES_PER_BLOCK = 64
G64_BYTES_PER_BLOCK = 18
G128_VALUES_PER_BLOCK = 128
G128_BYTES_PER_BLOCK = 34


class RetagError(RuntimeError):
    """A validation or safe-writing failure that must stop the retag."""


@dataclass(frozen=True)
class HeaderLayout:
    kv_count: int
    tensor_info_start: int
    tensor_info_end: int
    data_offset: int
    alignment: int


@dataclass(frozen=True)
class Q2Span:
    name: str
    offset: int
    span: int
    g64_payload: int
    g128_payload: int
    g64_matches: bool
    g128_matches: bool


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Verify a Q2_0 GGUF is unambiguously g128, then add "
            "GGML_Q2_0_G128=1 to a copy."
        ),
    )
    parser.add_argument("input", type=Path, help="source GGUF file")

    destination = parser.add_mutually_exclusive_group()
    destination.add_argument(
        "-o",
        "--output",
        type=Path,
        help="destination GGUF path; default: <input-stem>.g128.gguf",
    )
    destination.add_argument(
        "--in-place",
        action="store_true",
        help=(
            "replace the source after successful verification and temporary-copy "
            "validation; this is intentionally opt-in"
        ),
    )
    return parser.parse_args()


def default_output_path(source: Path) -> Path:
    return source.with_name(f"{source.stem}.g128{source.suffix}")


def validate_source(source: Path) -> None:
    print(f"Validating input: {source}")

    if not source.exists():
        raise RetagError(f"input file does not exist: {source}")
    if not source.is_file():
        raise RetagError(f"input path is not a regular file: {source}")

    try:
        with source.open("rb") as handle:
            magic = handle.read(4)
    except OSError as exc:
        raise RetagError(f"cannot read input file: {source}: {exc}") from exc

    if magic != GGUF_MAGIC:
        raise RetagError(
            f"{source} does not begin with valid GGUF magic bytes "
            f"{GGUF_MAGIC!r}; refusing to retag"
        )

    print("  GGUF magic bytes: valid.")


def import_gguf() -> Any:
    try:
        import gguf  # type: ignore[import-not-found]
    except ImportError as exc:
        raise RetagError(
            "gguf-py is required. Install the fork's gguf-py package into the "
            "Python environment used to run this tool."
        ) from exc

    return gguf


def field_size(field: Any) -> int:
    return sum(int(part.nbytes) for part in field.parts)


def header_layout(reader: Any) -> HeaderLayout:
    if reader.byte_order != "I" or sys.byteorder != "little":
        raise RetagError(
            "this tool supports only little-endian GGUF files; refusing to "
            "rewrite a byte-swapped file"
        )

    if not reader.tensors:
        raise RetagError("GGUF has no tensor table; refusing to retag")

    kv_count_field = reader.get_field("GGUF.kv_count")
    if kv_count_field is None:
        raise RetagError("GGUF reader did not expose GGUF.kv_count")

    tensor_info_start = min(tensor.field.offset for tensor in reader.tensors)
    tensor_info_end = max(
        tensor.field.offset + field_size(tensor.field) for tensor in reader.tensors
    )
    data_offset = int(reader.data_offset)
    alignment = int(reader.alignment)

    if alignment <= 0 or alignment & (alignment - 1):
        raise RetagError(
            f"GGUF declares invalid tensor alignment {alignment}; refusing to retag"
        )

    if not (24 <= tensor_info_start <= tensor_info_end <= data_offset):
        raise RetagError(
            "GGUF header/tensor-table offsets are inconsistent; refusing to retag"
        )

    return HeaderLayout(
        kv_count=int(kv_count_field.contents()),
        tensor_info_start=tensor_info_start,
        tensor_info_end=tensor_info_end,
        data_offset=data_offset,
        alignment=alignment,
    )


def span_matches(span: int, payload: int, alignment: int) -> bool:
    """Match the loader guard exactly: raw payload or alignment-padded payload."""
    if span == payload:
        return True

    padded = ((payload + alignment - 1) // alignment) * alignment
    return span == padded


def q2_0_spans(source: Path, reader: Any, layout: HeaderLayout) -> list[Q2Span]:
    file_size = source.stat().st_size
    if layout.data_offset > file_size:
        raise RetagError(
            "GGUF data offset is beyond end of file; refusing to retag"
        )

    data_size = file_size - layout.data_offset
    all_offsets: list[int] = []

    for tensor in reader.tensors:
        offset = int(tensor.data_offset) - layout.data_offset
        if offset < 0 or offset > data_size:
            raise RetagError(
                f"tensor {tensor.name!r} has an offset outside the GGUF data region"
            )
        all_offsets.append(offset)

    results: list[Q2Span] = []
    for tensor in reader.tensors:
        if tensor.tensor_type.name != "Q2_0":
            continue

        shape = [int(dimension) for dimension in tensor.shape]
        if not shape or shape[0] <= 0:
            raise RetagError(
                f"Q2_0 tensor {tensor.name!r} has an invalid shape; refusing to retag"
            )

        n_cols = shape[0]
        n_rows = math.prod(shape[1:]) if len(shape) > 1 else 1

        # This is the same prerequisite as llama_q2_0_g128_nbytes().
        if n_cols % G128_VALUES_PER_BLOCK:
            raise RetagError(
                f"Q2_0 tensor {tensor.name!r} has row width {n_cols}, which is not "
                f"divisible by the g128 block width {G128_VALUES_PER_BLOCK}; "
                "refusing to retag"
            )

        offset = int(tensor.data_offset) - layout.data_offset
        next_offset = min(
            (candidate for candidate in all_offsets if candidate > offset),
            default=data_size,
        )
        if next_offset < offset:
            raise RetagError(
                f"Q2_0 tensor {tensor.name!r} has a non-monotonic GGUF offset"
            )

        span = next_offset - offset
        g64_payload = n_rows * (n_cols // G64_VALUES_PER_BLOCK) * G64_BYTES_PER_BLOCK
        g128_payload = (
            n_rows * (n_cols // G128_VALUES_PER_BLOCK) * G128_BYTES_PER_BLOCK
        )

        results.append(
            Q2Span(
                name=tensor.name,
                offset=offset,
                span=span,
                g64_payload=g64_payload,
                g128_payload=g128_payload,
                g64_matches=span_matches(span, g64_payload, layout.alignment),
                g128_matches=span_matches(span, g128_payload, layout.alignment),
            )
        )

    return results


def verify_g128_geometry(source: Path, reader: Any, layout: HeaderLayout) -> None:
    print("Checking Q2_0 tensor spans against g64 and g128 geometry...")

    spans = q2_0_spans(source, reader, layout)
    if not spans:
        raise RetagError(
            "file has no Q2_0 tensors, so g128 geometry cannot be established; "
            "refusing to retag"
        )

    confirmed_g128 = False
    for result in spans:
        print(
            f"  {result.name}: span={result.span} bytes; "
            f"g64 payload={result.g64_payload}, match={result.g64_matches}; "
            f"g128 payload={result.g128_payload}, match={result.g128_matches}"
        )

        if result.g128_matches and not result.g64_matches:
            confirmed_g128 = True
            print("    confirmed: g128-only discriminator")
            continue

        if result.g64_matches and not result.g128_matches:
            raise RetagError(
                f"Q2_0 tensor {result.name!r} has a {result.span}-byte span that "
                "matches legacy g64 geometry only; refusing to create a wrongly "
                "tagged file"
            )

        if not result.g64_matches and not result.g128_matches:
            raise RetagError(
                f"Q2_0 tensor {result.name!r} has a {result.span}-byte span that "
                "matches neither g64 nor g128 geometry; refusing to retag"
            )

        print(
            "    alignment-ambiguous: not evidence for either layout; a separate "
            "g128-only discriminator is required"
        )

    if not confirmed_g128:
        raise RetagError(
            "every Q2_0 tensor span is alignment-ambiguous; no span "
            "unambiguously confirms g128 geometry, so the file will not be retagged"
        )

    print("  Geometry check: confirmed g128.")


def marker_bytes() -> bytes:
    key = METADATA_KEY.encode("utf-8")

    # GGUF string: uint64 length + UTF-8 bytes.
    # GGUF metadata entry: key string + uint32 value type + scalar value.
    # The loader accepts scalar unsigned 1; writing UINT32 preserves the literal
    # GGML_Q2_0_G128=1 contract.
    return b"".join(
        (
            struct.pack("<Q", len(key)),
            key,
            struct.pack("<I", 4),  # GGUF_TYPE_UINT32
            struct.pack("<I", 1),
        )
    )


def copy_range(source: Any, destination: Any, start: int, end: int) -> None:
    if end < start:
        raise RetagError("attempted to copy an invalid GGUF byte range")

    source.seek(start)
    remaining = end - start
    chunk_size = 8 * 1024 * 1024

    while remaining:
        chunk = source.read(min(chunk_size, remaining))
        if not chunk:
            raise RetagError("input ended unexpectedly while copying GGUF header")
        destination.write(chunk)
        remaining -= len(chunk)


def write_marked_copy(
    source: Path,
    destination: Path,
    layout: HeaderLayout,
) -> None:
    """Copy all tensor bytes unchanged while inserting one metadata entry.

    gguf-py's reader is deliberately used for parsing/validation. The payload copy
    is raw because on-disk g128 tensors still advertise Q2_0; a generic Q2_0 writer
    would calculate g64 payload lengths and could truncate those tensor bytes.
    """
    marker = marker_bytes()

    with source.open("rb") as input_file, destination.open("xb") as output_file:
        first_16 = input_file.read(16)
        if len(first_16) != 16:
            raise RetagError("input ended before the full GGUF header")

        # Preserve magic, version, and tensor count. Replace only kv_count.
        output_file.write(first_16)
        output_file.write(struct.pack("<Q", layout.kv_count + 1))

        # Existing metadata, then the new marker, then unchanged tensor infos.
        copy_range(input_file, output_file, 24, layout.tensor_info_start)
        output_file.write(marker)
        copy_range(
            input_file,
            output_file,
            layout.tensor_info_start,
            layout.tensor_info_end,
        )

        padding = (-output_file.tell()) % layout.alignment
        output_file.write(b"\x00" * padding)

        # Tensor offsets are relative to the aligned data region, so the complete
        # original data region can remain byte-for-byte unchanged.
        input_file.seek(layout.data_offset)
        shutil.copyfileobj(input_file, output_file, length=8 * 1024 * 1024)


def verify_written_marker(output: Path, gguf: Any) -> None:
    print("Validating written metadata key with gguf-py...")

    try:
        reader = gguf.GGUFReader(str(output), "r")
        field = reader.get_field(METADATA_KEY)
        if field is None:
            raise RetagError(f"written file is missing {METADATA_KEY}")

        if field.types != [gguf.GGUFValueType.UINT32] or field.contents() != 1:
            raise RetagError(
                f"written {METADATA_KEY} is not a scalar UINT32 value of 1"
            )
    except RetagError:
        raise
    except Exception as exc:
        raise RetagError(
            f"gguf-py could not validate the written output {output}: {exc}"
        ) from exc
    finally:
        if "reader" in locals():
            del reader
            gc.collect()


def write_atomically(
    source: Path,
    destination: Path,
    layout: HeaderLayout,
    gguf: Any,
    *,
    in_place: bool,
) -> None:
    descriptor, temporary_name = tempfile.mkstemp(
        dir=destination.parent,
        prefix=f".{destination.name}.retag-",
        suffix=".tmp",
    )
    os.close(descriptor)
    temporary = Path(temporary_name)
    temporary.unlink()

    try:
        print(f"Writing {METADATA_KEY}=1 to: {destination}")
        write_marked_copy(source, temporary, layout)
        verify_written_marker(temporary, gguf)

        if not in_place and destination.exists():
            raise RetagError(
                f"output file appeared while writing: {destination}; refusing to overwrite it"
            )

        if in_place:
            print(f"Replacing source after successful temporary-copy validation: {source}")
        os.replace(temporary, destination)
    except Exception:
        temporary.unlink(missing_ok=True)
        raise


def main() -> int:
    args = parse_args()
    source = args.input.resolve()

    try:
        validate_source(source)
        gguf = import_gguf()

        print("Opening input with gguf-py...")
        try:
            reader = gguf.GGUFReader(str(source), "r")
        except Exception as exc:
            raise RetagError(f"gguf-py could not parse {source}: {exc}") from exc

        try:
            if reader.get_field(METADATA_KEY) is not None:
                raise RetagError(
                    f"{METADATA_KEY} is already present; refusing to create a "
                    "duplicate metadata key"
                )

            layout = header_layout(reader)
            verify_g128_geometry(source, reader, layout)
        finally:
            del reader
            gc.collect()

        if args.in_place:
            destination = source
            print(
                "WARNING: --in-place was explicitly requested. The source file will "
                "be replaced only after a verified same-directory temporary copy is "
                "written successfully."
            )
        else:
            destination = (args.output or default_output_path(source)).resolve()
            if destination == source:
                raise RetagError(
                    "output resolves to the input path; use --in-place explicitly "
                    "if replacement is intended"
                )
            if not destination.parent.is_dir():
                raise RetagError(
                    f"output directory does not exist: {destination.parent}"
                )
            if destination.exists():
                raise RetagError(
                    f"output already exists: {destination}; refusing to overwrite it"
                )

        write_atomically(
            source,
            destination,
            layout,
            gguf,
            in_place=args.in_place,
        )
        print(f"Done: verified g128 marker written to {destination}")
        return 0

    except RetagError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"ERROR: filesystem failure: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
