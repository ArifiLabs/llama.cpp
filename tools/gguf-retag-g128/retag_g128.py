#!/usr/bin/env python3
"""
Fail-closed GGUF v3 retagger for Prism-style Q2_0 g128 files.

This parser deliberately never uses gguf-py and never constructs tensor arrays.
It reads only GGUF structural fields (header, metadata KV entries, tensor-info
table), derives Q2_0 geometry from declared dimensions and byte spans, and
streams the tensor-data region through byte-for-byte unchanged.
"""

from __future__ import annotations

import argparse
import os
import struct
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO


GGUF_MAGIC = b"GGUF"
GGUF_VERSION = 3
DEFAULT_ALIGNMENT = 32

GGML_TYPE_Q2_0 = 42

GGUF_VALUE_TYPE_UINT8 = 0
GGUF_VALUE_TYPE_INT8 = 1
GGUF_VALUE_TYPE_UINT16 = 2
GGUF_VALUE_TYPE_INT16 = 3
GGUF_VALUE_TYPE_UINT32 = 4
GGUF_VALUE_TYPE_INT32 = 5
GGUF_VALUE_TYPE_FLOAT32 = 6
GGUF_VALUE_TYPE_BOOL = 7
GGUF_VALUE_TYPE_STRING = 8
GGUF_VALUE_TYPE_ARRAY = 9
GGUF_VALUE_TYPE_UINT64 = 10
GGUF_VALUE_TYPE_INT64 = 11
GGUF_VALUE_TYPE_FLOAT64 = 12

MARKER_KEY = b"GGML_Q2_0_G128"
ALIGNMENT_KEY = b"general.alignment"

_HEADER = struct.Struct("<4sIQQ")
_U32 = struct.Struct("<I")
_U64 = struct.Struct("<Q")

COPY_CHUNK_BYTES = 1024 * 1024


class RetagRefusal(RuntimeError):
    """Raised when the tool must fail closed."""


@dataclass(frozen=True)
class TensorInfo:
    name: bytes
    dimensions: tuple[int, ...]
    ggml_type: int
    offset: int


@dataclass(frozen=True)
class GgufLayout:
    path: Path
    file_size: int
    tensor_count: int
    metadata_kv_count: int
    alignment: int
    metadata_start: int
    tensor_table_start: int
    tensor_table_end: int
    tensor_data_start: int
    tensors: tuple[TensorInfo, ...]
    marker_entries: tuple[tuple[int, int | None], ...]


@dataclass(frozen=True)
class SpanVerification:
    q2_tensor_count: int
    g128_discriminators: int
    ambiguous_count: int


class _StructureReader:
    """
    Bounded sequential reader for GGUF structural sections.

    Metadata strings and unneeded metadata values are skipped with seek(), not
    materialized. Tensor payload offsets are never passed to this reader.
    """

    def __init__(self, path: Path) -> None:
        self._file = path.open("rb")
        self.file_size = os.fstat(self._file.fileno()).st_size
        self.position = 0

    def close(self) -> None:
        self._file.close()

    def __enter__(self) -> _StructureReader:
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def _require_available(self, count: int, context: str) -> None:
        if count < 0 or count > self.file_size - self.position:
            raise RetagRefusal(
                f"invalid GGUF: {context} extends beyond the end of the file"
            )

    def read_exact(self, count: int, context: str) -> bytes:
        self._require_available(count, context)
        data = self._file.read(count)
        if len(data) != count:
            raise RetagRefusal(f"invalid GGUF: short read while reading {context}")
        self.position += count
        return data

    def skip(self, count: int, context: str) -> None:
        self._require_available(count, context)
        self._file.seek(count, os.SEEK_CUR)
        self.position += count

    def u8(self, context: str) -> int:
        return self.read_exact(1, context)[0]

    def u32(self, context: str) -> int:
        return _U32.unpack(self.read_exact(4, context))[0]

    def u64(self, context: str) -> int:
        return _U64.unpack(self.read_exact(8, context))[0]

    def gguf_string(self, context: str, maximum_length: int) -> bytes:
        length = self.u64(f"{context} length")
        if length > maximum_length:
            raise RetagRefusal(
                f"invalid GGUF: {context} length {length} exceeds {maximum_length}"
            )
        return self.read_exact(length, context)

    def skip_gguf_string(self, context: str) -> None:
        length = self.u64(f"{context} length")
        self.skip(length, context)


_FIXED_VALUE_SIZES = {
    GGUF_VALUE_TYPE_UINT8: 1,
    GGUF_VALUE_TYPE_INT8: 1,
    GGUF_VALUE_TYPE_UINT16: 2,
    GGUF_VALUE_TYPE_INT16: 2,
    GGUF_VALUE_TYPE_UINT32: 4,
    GGUF_VALUE_TYPE_INT32: 4,
    GGUF_VALUE_TYPE_FLOAT32: 4,
    GGUF_VALUE_TYPE_BOOL: 1,
    GGUF_VALUE_TYPE_UINT64: 8,
    GGUF_VALUE_TYPE_INT64: 8,
    GGUF_VALUE_TYPE_FLOAT64: 8,
}


def _align_up(offset: int, alignment: int) -> int:
    remainder = offset % alignment
    return offset if remainder == 0 else offset + alignment - remainder


def _validate_alignment(alignment: int) -> int:
    if alignment <= 0 or alignment % 8 != 0:
        raise RetagRefusal(
            f"invalid GGUF: general.alignment must be a positive multiple of 8, "
            f"got {alignment}"
        )
    return alignment


def _skip_metadata_value(
    reader: _StructureReader,
    value_type: int,
    *,
    depth: int = 0,
) -> None:
    if depth > 64:
        raise RetagRefusal("invalid GGUF: metadata array nesting exceeds 64 levels")

    fixed_size = _FIXED_VALUE_SIZES.get(value_type)
    if fixed_size is not None:
        reader.skip(fixed_size, "metadata scalar value")
        return

    if value_type == GGUF_VALUE_TYPE_STRING:
        reader.skip_gguf_string("metadata string value")
        return

    if value_type != GGUF_VALUE_TYPE_ARRAY:
        raise RetagRefusal(f"invalid GGUF: unknown metadata value type {value_type}")

    element_type = reader.u32("metadata array element type")
    element_count = reader.u64("metadata array length")

    fixed_element_size = _FIXED_VALUE_SIZES.get(element_type)
    if fixed_element_size is not None:
        reader.skip(
            element_count * fixed_element_size,
            "metadata array scalar values",
        )
        return

    for _ in range(element_count):
        _skip_metadata_value(reader, element_type, depth=depth + 1)


def _parse_gguf_layout(path: Path) -> GgufLayout:
    """
    Parse exactly the GGUF v3 header, metadata, and tensor-info table.

    No tensor-data bytes are read, decoded, reshaped, or otherwise interpreted.
    """

    with _StructureReader(path) as reader:
        if reader.file_size < _HEADER.size:
            raise RetagRefusal("invalid GGUF: file is smaller than its header")

        magic, version, tensor_count, metadata_kv_count = _HEADER.unpack(
            reader.read_exact(_HEADER.size, "GGUF header")
        )

        if magic != GGUF_MAGIC:
            raise RetagRefusal("invalid GGUF: magic is not GGUF")
        if version != GGUF_VERSION:
            raise RetagRefusal(
                f"unsupported GGUF version {version}; this tool requires v3"
            )

        metadata_start = reader.position
        alignment = DEFAULT_ALIGNMENT
        marker_entries: list[tuple[int, int | None]] = []

        for index in range(metadata_kv_count):
            key = reader.gguf_string(f"metadata key {index}", 65535)
            value_type = reader.u32(f"metadata key {index} value type")

            if key == ALIGNMENT_KEY:
                if value_type != GGUF_VALUE_TYPE_UINT32:
                    raise RetagRefusal(
                        "invalid GGUF: general.alignment is not a uint32"
                    )
                alignment = reader.u32("general.alignment value")
                continue

            if key == MARKER_KEY:
                if value_type == GGUF_VALUE_TYPE_UINT8:
                    marker_entries.append(
                        (value_type, reader.u8("GGML_Q2_0_G128 value"))
                    )
                else:
                    marker_entries.append((value_type, None))
                    _skip_metadata_value(reader, value_type)
                continue

            _skip_metadata_value(reader, value_type)

        alignment = _validate_alignment(alignment)
        tensor_table_start = reader.position
        tensors: list[TensorInfo] = []

        for index in range(tensor_count):
            name = reader.gguf_string(f"tensor {index} name", 64)
            dimension_count = reader.u32(f"tensor {index} dimension count")

            if dimension_count == 0:
                raise RetagRefusal(
                    f"invalid GGUF: tensor {name!r} has zero dimensions"
                )
            if dimension_count > 64:
                raise RetagRefusal(
                    f"invalid GGUF: tensor {name!r} has too many dimensions "
                    f"({dimension_count})"
                )

            dimensions = tuple(
                reader.u64(f"tensor {index} dimension {dimension_index}")
                for dimension_index in range(dimension_count)
            )
            ggml_type = reader.u32(f"tensor {index} ggml type")
            offset = reader.u64(f"tensor {index} data offset")

            tensors.append(
                TensorInfo(
                    name=name,
                    dimensions=dimensions,
                    ggml_type=ggml_type,
                    offset=offset,
                )
            )

        tensor_table_end = reader.position
        tensor_data_start = _align_up(tensor_table_end, alignment)

        if tensor_data_start > reader.file_size:
            raise RetagRefusal(
                "invalid GGUF: alignment padding places tensor data beyond EOF"
            )

        return GgufLayout(
            path=path,
            file_size=reader.file_size,
            tensor_count=tensor_count,
            metadata_kv_count=metadata_kv_count,
            alignment=alignment,
            metadata_start=metadata_start,
            tensor_table_start=tensor_table_start,
            tensor_table_end=tensor_table_end,
            tensor_data_start=tensor_data_start,
            tensors=tuple(tensors),
            marker_entries=tuple(marker_entries),
        )


def _q2_padded_span(
    dimensions: tuple[int, ...],
    *,
    values_per_block: int,
    bytes_per_block: int,
    alignment: int,
) -> int | None:
    """
    Return this tensor's expected padded Q2_0 span, or None when its declared
    width cannot be represented by the candidate block geometry.
    """

    columns = dimensions[0]
    if columns == 0 or columns % values_per_block != 0:
        return None
    if any(dimension == 0 for dimension in dimensions[1:]):
        return None

    rows = 1
    for dimension in dimensions[1:]:
        rows *= dimension

    raw_bytes = rows * (columns // values_per_block) * bytes_per_block
    return _align_up(raw_bytes, alignment)


def _verify_q2_spans(layout: GgufLayout) -> SpanVerification:
    """
    Classify Q2_0 entries from table offsets, dimensions, and file size only.

    This is the g64-vs-g128 discriminator:
      g64  = ceil-to-alignment(R * (C / 64)  * 18)
      g128 = ceil-to-alignment(R * (C / 128) * 34)
    """

    payload_size = layout.file_size - layout.tensor_data_start
    ordered = sorted(layout.tensors, key=lambda tensor: tensor.offset)

    previous_offset: int | None = None
    for tensor in ordered:
        if tensor.offset % layout.alignment != 0:
            raise RetagRefusal(
                f"invalid GGUF: tensor {tensor.name!r} offset {tensor.offset} "
                f"is not aligned to {layout.alignment}"
            )
        if tensor.offset > payload_size:
            raise RetagRefusal(
                f"invalid GGUF: tensor {tensor.name!r} offset is beyond tensor data"
            )
        if previous_offset is not None and tensor.offset == previous_offset:
            raise RetagRefusal(
                "invalid GGUF: two tensor entries share one data offset"
            )
        previous_offset = tensor.offset

    q2_tensor_count = 0
    g128_discriminators = 0
    ambiguous_count = 0

    for index, tensor in enumerate(ordered):
        if tensor.ggml_type != GGML_TYPE_Q2_0:
            continue

        q2_tensor_count += 1
        next_offset = (
            ordered[index + 1].offset
            if index + 1 < len(ordered)
            else payload_size
        )
        span = next_offset - tensor.offset

        g64_span = _q2_padded_span(
            tensor.dimensions,
            values_per_block=64,
            bytes_per_block=18,
            alignment=layout.alignment,
        )
        g128_span = _q2_padded_span(
            tensor.dimensions,
            values_per_block=128,
            bytes_per_block=34,
            alignment=layout.alignment,
        )

        matches_g64 = g64_span is not None and span == g64_span
        matches_g128 = g128_span is not None and span == g128_span

        if matches_g128 and not matches_g64:
            g128_discriminators += 1
            continue

        if matches_g64 and not matches_g128:
            raise RetagRefusal(
                f"g64-only span for Q2_0 tensor {tensor.name!r}: "
                f"observed {span}, expected g64 {g64_span}"
            )

        if matches_g64 and matches_g128:
            ambiguous_count += 1
            continue

        raise RetagRefusal(
            f"Q2_0 tensor {tensor.name!r} span matches neither geometry: "
            f"observed {span}, g64={g64_span}, g128={g128_span}"
        )

    if q2_tensor_count == 0:
        raise RetagRefusal("no Q2_0 tensors found; refusing to add a g128 marker")

    if g128_discriminators == 0:
        raise RetagRefusal(
            "all Q2_0 tensor spans are alignment-ambiguous; "
            "refusing unsafe g128 tagging"
        )

    return SpanVerification(
        q2_tensor_count=q2_tensor_count,
        g128_discriminators=g128_discriminators,
        ambiguous_count=ambiguous_count,
    )


def _marker_entry_bytes() -> bytes:
    return (
        _U64.pack(len(MARKER_KEY))
        + MARKER_KEY
        + _U32.pack(GGUF_VALUE_TYPE_UINT8)
        + bytes((1,))
    )


def _copy_range(
    source: BinaryIO,
    destination: BinaryIO,
    *,
    start: int,
    length: int,
    context: str,
) -> None:
    """
    Copy an exact byte range without decoding it.

    For tensor payload ranges, chunks are written directly and never parsed,
    reshaped, or converted to a tensor representation.
    """

    source.seek(start)
    remaining = length

    while remaining:
        chunk = source.read(min(COPY_CHUNK_BYTES, remaining))
        if not chunk:
            raise RetagRefusal(f"short read while streaming {context}")
        destination.write(chunk)
        remaining -= len(chunk)


def _write_zero_padding(destination: BinaryIO, count: int) -> None:
    zero_chunk = b"\x00" * min(COPY_CHUNK_BYTES, count)
    remaining = count

    while remaining:
        chunk_length = min(len(zero_chunk), remaining)
        destination.write(zero_chunk[:chunk_length])
        remaining -= chunk_length


def _write_retagged_file(
    source_path: Path,
    destination_path: Path,
    source_layout: GgufLayout,
    *,
    destination_exists_by_design: bool,
) -> tuple[int, int]:
    """
    Write a new file with one appended metadata entry and a recomputed data start.

    The tensor-info table is copied verbatim. Its offsets remain valid because
    they are relative to the newly aligned tensor-data start. Tensor payload
    bytes are streamed unmodified from the original data start to the new one.
    """

    marker = _marker_entry_bytes()
    new_tensor_table_end = source_layout.tensor_table_end + len(marker)
    new_tensor_data_start = _align_up(
        new_tensor_table_end,
        source_layout.alignment,
    )
    new_padding_length = new_tensor_data_start - new_tensor_table_end
    payload_length = source_layout.file_size - source_layout.tensor_data_start
    expected_size = new_tensor_data_start + payload_length

    if source_layout.metadata_kv_count == (1 << 64) - 1:
        raise RetagRefusal("invalid GGUF: metadata count cannot be incremented")

    mode = "wb" if destination_exists_by_design else "xb"

    with source_path.open("rb") as source, destination_path.open(mode) as destination:
        destination.write(
            _HEADER.pack(
                GGUF_MAGIC,
                GGUF_VERSION,
                source_layout.tensor_count,
                source_layout.metadata_kv_count + 1,
            )
        )

        _copy_range(
            source,
            destination,
            start=source_layout.metadata_start,
            length=source_layout.tensor_table_start - source_layout.metadata_start,
            context="original metadata",
        )
        destination.write(marker)

        _copy_range(
            source,
            destination,
            start=source_layout.tensor_table_start,
            length=source_layout.tensor_table_end - source_layout.tensor_table_start,
            context="tensor-info table",
        )
        _write_zero_padding(destination, new_padding_length)

        _copy_range(
            source,
            destination,
            start=source_layout.tensor_data_start,
            length=payload_length,
            context="tensor payload",
        )

        if destination.tell() != expected_size:
            raise RetagRefusal(
                "internal error: output size differs from the computed layout"
            )

    return new_tensor_data_start, expected_size


def _validate_output(
    output_path: Path,
    *,
    expected_tensor_data_start: int,
    expected_size: int,
) -> SpanVerification:
    """
    Post-write validation using the same header/KV/table-only parser.

    This intentionally does not use gguf-py or access tensor payload content.
    """

    output_layout = _parse_gguf_layout(output_path)

    if output_layout.file_size != expected_size:
        raise RetagRefusal(
            f"post-write validation failed: output size is {output_layout.file_size}, "
            f"expected {expected_size}"
        )

    if output_layout.tensor_data_start != expected_tensor_data_start:
        raise RetagRefusal(
            "post-write validation failed: recomputed tensor-data start differs "
            "from the write plan"
        )

    if output_layout.marker_entries != ((GGUF_VALUE_TYPE_UINT8, 1),):
        raise RetagRefusal(
            "post-write validation failed: GGML_Q2_0_G128 is not exactly uint8 1"
        )

    return _verify_q2_spans(output_layout)


def _default_output_path(source: Path) -> Path:
    suffix = source.suffix
    stem = source.name[: -len(suffix)] if suffix else source.name
    return source.with_name(f"{stem}.g128{suffix}")


def _retag_copy(source: Path, destination: Path, layout: GgufLayout) -> SpanVerification:
    if os.path.lexists(destination):
        raise RetagRefusal(f"output already exists: {destination}")

    try:
        new_data_start, expected_size = _write_retagged_file(
            source,
            destination,
            layout,
            destination_exists_by_design=False,
        )
        return _validate_output(
            destination,
            expected_tensor_data_start=new_data_start,
            expected_size=expected_size,
        )
    except BaseException:
        try:
            destination.unlink(missing_ok=True)
        except OSError:
            pass
        raise


def _retag_in_place(source: Path, layout: GgufLayout) -> SpanVerification:
    print(
        f"WARNING: --in-place will replace the original only after post-write "
        f"validation succeeds: {source}",
        file=sys.stderr,
    )

    file_descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{source.name}.",
        suffix=".retag-g128.tmp",
        dir=source.parent,
    )
    os.close(file_descriptor)
    temporary_path = Path(temporary_name)

    try:
        new_data_start, expected_size = _write_retagged_file(
            source,
            temporary_path,
            layout,
            destination_exists_by_design=True,
        )
        verification = _validate_output(
            temporary_path,
            expected_tensor_data_start=new_data_start,
            expected_size=expected_size,
        )
        os.replace(temporary_path, source)
        return verification
    finally:
        try:
            temporary_path.unlink(missing_ok=True)
        except OSError:
            pass


def _build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Fail-closed GGUF v3 retagger for verified Q2_0 g128 files. "
            "The default operation writes a separate copy."
        )
    )
    parser.add_argument("input", type=Path, help="source GGUF file")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="copy destination (default: <input>.g128.gguf)",
    )
    parser.add_argument(
        "--in-place",
        action="store_true",
        help="replace the input only after a temporary output validates",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = _build_argument_parser().parse_args(argv)

    if arguments.in_place and arguments.output is not None:
        print("REFUSED: --in-place and --output cannot be used together", file=sys.stderr)
        return 2

    source = arguments.input.expanduser().resolve()
    if not source.is_file():
        print(f"REFUSED: input is not a regular file: {source}", file=sys.stderr)
        return 2

    try:
        source_layout = _parse_gguf_layout(source)

        if source_layout.marker_entries:
            raise RetagRefusal(
                "marker key already present: GGML_Q2_0_G128; refusing to retag"
            )

        _verify_q2_spans(source_layout)

        if arguments.in_place:
            verification = _retag_in_place(source, source_layout)
            output = source
        else:
            output = (
                arguments.output.expanduser().resolve()
                if arguments.output is not None
                else _default_output_path(source)
            )

            if output == source:
                raise RetagRefusal(
                    "copy destination is the input file; use --in-place explicitly"
                )

            verification = _retag_copy(source, output, source_layout)

    except RetagRefusal as error:
        print(f"REFUSED: {error}", file=sys.stderr)
        return 2
    except OSError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print(
        f"Tagged verified g128 GGUF: {output}\n"
        f"Q2_0 tensors={verification.q2_tensor_count}, "
        f"g128 discriminators={verification.g128_discriminators}, "
        f"alignment-ambiguous={verification.ambiguous_count}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
