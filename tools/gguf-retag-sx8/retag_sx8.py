#!/usr/bin/env python3
"""Retag a MarlaLabs S-X8 v4.3 GGUF onto the ArifiLabs type id (docs/TYPE-ID-ALLOCATION.md, row 57).

MarlaLabs' llama-cpp-sx8.patch serializes S-X8 at type id 41. Our 41 (and upstream's) is Q1_0, in
files that exist. S-X8 is therefore remapped to 57, the first free serialized weight slot, and an
S-X8-authored GGUF must be retagged before it will load here.

Same shape and the same safety argument as tools/gguf-retag-tq3/retag_tq3.py: the id alone never
decides. Every id-41 tensor is measured against the S-X8 geometry (30 bytes per 32 values) from its
byte span, and the whole file is refused if any tensor does not match. A genuine Q1_0 file (18 bytes
per 128 values = 4.5 per 32) is refused, never rewritten.

REFUSES on all of:
  * an in-place write (the output is always a NEW file)
  * a file with no id-41 tensors
  * an id-41 tensor whose span matches Q1_0 geometry (the file is upstream's, not S-X8)
  * an id-41 tensor whose span matches neither geometry, or is too small to decide
  * a data section that does not make sense (overlapping offsets, truncated file)

Usage:
  python retag_sx8.py <input.gguf> <output.gguf> [--map-out map.tsv]
  python retag_sx8.py --selftest
"""

from __future__ import annotations

import argparse
import shutil
import struct
import sys
import tempfile
from pathlib import Path

SX8_SRC_ID = 41   # what the author's convert script writes (GGMLQuantizationType(41))
SX8_DST_ID = 57   # GGML_TYPE_SX8 in ggml/include/ggml.h
SX8_BYTES_PER_32 = 30

# Geometries that are NOT S-X8 but share id 41. A tensor matching one of these means the file
# is somebody else's, and the whole file is refused.
FOREIGN_41 = [
    (128, 18, "upstream GGML_TYPE_Q1_0 (18 B / 128 values)"),
]

_GGUF_MAGIC = b"GGUF"


class RetagRefusal(RuntimeError):
    pass


class _Reader:
    def __init__(self, fh):
        self.fh = fh

    def raw(self, n: int) -> bytes:
        b = self.fh.read(n)
        if len(b) != n:
            raise RetagRefusal(f"truncated header: wanted {n} bytes at {self.fh.tell()}")
        return b

    def u32(self) -> int:
        return struct.unpack("<I", self.raw(4))[0]

    def u64(self) -> int:
        return struct.unpack("<Q", self.raw(8))[0]

    def string(self) -> str:
        return self.raw(self.u64()).decode("utf-8", errors="replace")


_FIXED = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1, 10: 8, 11: 8, 12: 8}


def _skip_value(r: _Reader, vtype: int) -> None:
    if vtype in _FIXED:
        r.raw(_FIXED[vtype])
    elif vtype == 8:
        r.raw(r.u64())
    elif vtype == 9:
        itype = r.u32()
        count = r.u64()
        if itype == 9:
            raise RetagRefusal("nested arrays are not supported")
        if itype in _FIXED:
            r.raw(_FIXED[itype] * count)
        elif itype == 8:
            for _ in range(count):
                r.raw(r.u64())
        else:
            raise RetagRefusal(f"unknown array element type {itype}")
    else:
        raise RetagRefusal(f"unknown metadata value type {vtype}")


def parse_gguf(path: Path):
    """-> (tensors, data_start, file_size, alignment)."""
    out = []
    path = Path(path)
    with open(path, "rb") as fh:
        r = _Reader(fh)
        if r.raw(4) != _GGUF_MAGIC:
            raise RetagRefusal(f"{path}: not a GGUF file")
        version = r.u32()
        if version != 3:
            raise RetagRefusal(f"{path}: GGUF version {version}, only 3 is handled")
        n_tensors = r.u64()
        n_kv = r.u64()
        alignment = 32
        for _ in range(n_kv):
            key = r.string()
            vtype = r.u32()
            if key == "general.alignment" and vtype == 4:
                alignment = struct.unpack("<I", r.raw(4))[0]
            else:
                _skip_value(r, vtype)
        for _ in range(n_tensors):
            name = r.string()
            n_dims = r.u32()
            nelem = 1
            for _ in range(n_dims):
                nelem *= r.u64()
            type_at = fh.tell()
            tid = r.u32()
            offset = r.u64()
            out.append({"name": name, "tid": tid, "type_at": type_at,
                        "nelem": nelem, "offset": offset})
        header_end = fh.tell()
    data_start = (header_end + alignment - 1) // alignment * alignment
    return out, data_start, path.stat().st_size, alignment


def _spans(tensors, data_start, file_size):
    ordered = sorted(tensors, key=lambda t: t["offset"])
    spans = {}
    for i, t in enumerate(ordered):
        end = ordered[i + 1]["offset"] if i + 1 < len(ordered) else file_size - data_start
        span = end - t["offset"]
        if span <= 0:
            raise RetagRefusal(
                f"REFUSED: tensor {t['name']!r} has a non-positive data span ({span}) - "
                f"the tensor table has overlapping or duplicate offsets, or the file is truncated"
            )
        spans[t["name"]] = span
    return spans


def _in_band(span: int, expected: int, alignment: int) -> bool:
    """[expected, expected + slack): padding only ADDS bytes, and slack is capped at a quarter so
    it can never reach a competing geometry."""
    if expected <= 0:
        return False
    slack = min(alignment, max(expected // 4, 1))
    return expected <= span < expected + slack


def verify_geometry(tensors, data_start, file_size, alignment) -> int:
    """Every id-41 tensor must measure as S-X8. Returns how many were checked."""
    spans = _spans(tensors, data_start, file_size)
    n = 0
    for t in tensors:
        if t["tid"] != SX8_SRC_ID:
            continue
        nelem = t["nelem"]
        span = spans[t["name"]]
        if nelem % 32:
            raise RetagRefusal(
                f"REFUSED: tensor {t['name']!r} has {nelem} elements, not a whole number of 32-value blocks"
            )
        expected = (nelem // 32) * SX8_BYTES_PER_32
        if _in_band(span, expected, alignment):
            n += 1
            continue
        for f_blk, f_bytes, f_name in FOREIGN_41:
            if nelem % f_blk == 0 and _in_band(span, (nelem // f_blk) * f_bytes, alignment):
                raise RetagRefusal(
                    f"REFUSED: tensor {t['name']!r} at id 41 measures {span} bytes, which is "
                    f"{f_name}, not S-X8's {SX8_BYTES_PER_32} B/32. This file is NOT an S-X8 file - nothing to retag."
                )
        raise RetagRefusal(
            f"REFUSED: tensor {t['name']!r} at id 41 measures {span} bytes but S-X8 geometry says "
            f"{expected} ({SX8_BYTES_PER_32} B/32 over {nelem} elements). Undecidable or corrupt - refusing rather than guessing."
        )
    return n


def plan(tensors, data_start, file_size, alignment):
    if not tensors:
        raise RetagRefusal("REFUSED: this GGUF declares no tensors")
    hist = {}
    for t in tensors:
        hist[t["tid"]] = hist.get(t["tid"], 0) + 1
    if SX8_SRC_ID not in hist:
        raise RetagRefusal("REFUSED: no id-41 tensors in this file - nothing to retag")
    if SX8_DST_ID in hist:
        raise RetagRefusal(
            f"REFUSED: this file already carries id {SX8_DST_ID} tensors next to id-41 ones; "
            f"a mixed file is not something this tool will guess about"
        )
    verify_geometry(tensors, data_start, file_size, alignment)
    rewrites = [(t["name"], t["tid"], SX8_DST_ID, t["type_at"]) for t in tensors if t["tid"] == SX8_SRC_ID]
    return rewrites, hist


def retag(src: Path, dst: Path, map_out: Path | None = None) -> int:
    src = Path(src).resolve()
    dst = Path(dst).resolve()
    if src == dst:
        raise RetagRefusal("REFUSED: in-place retag. Give a NEW output path.")
    if dst.exists():
        raise RetagRefusal(f"REFUSED: {dst} already exists")

    tensors, data_start, file_size, alignment = parse_gguf(src)
    rewrites, hist = plan(tensors, data_start, file_size, alignment)
    n_checked = verify_geometry(tensors, data_start, file_size, alignment)

    print(f"source     : {src}")
    print(f"tensors    : {len(tensors)}   id histogram: {dict(sorted(hist.items()))}")
    print(f"geometry   : {n_checked} id-41 tensors measured as S-X8 ({SX8_BYTES_PER_32} B/32) from byte spans")
    print(f"rewrites   : {len(rewrites)}  map: {{{SX8_SRC_ID}: {SX8_DST_ID}}}")

    shutil.copyfile(src, dst)
    with open(dst, "r+b") as fh:
        for _, _, new, at in rewrites:
            fh.seek(at)
            fh.write(struct.pack("<I", new))

    hist_after = {}
    for x in parse_gguf(dst)[0]:
        hist_after[x["tid"]] = hist_after.get(x["tid"], 0) + 1
    print(f"output     : {dst}")
    print(f"id histogram after: {dict(sorted(hist_after.items()))}")

    if map_out is not None:
        with open(map_out, "w", encoding="utf-8") as fh:
            fh.write("tensor\told_id\tnew_id\n")
            for name, old, new, _ in rewrites:
                fh.write(f"{name}\t{old}\t{new}\n")
        print(f"map        : {map_out}")
        embd = [n for n, _, _, _ in rewrites if "token_embd" in n]
        print(f"token_embd retagged: {embd if embd else 'NO - get_rows is NOT exercised by this file'}")
    return 0


def _build_gguf(tmp: Path, spec) -> Path:
    """Minimal valid GGUF v3. `spec` = list of (type_id, nelem, nbytes)."""
    n = len(spec)
    body = bytearray()
    body += _GGUF_MAGIC + struct.pack("<I", 3)
    body += struct.pack("<Q", n) + struct.pack("<Q", 1)
    key = b"general.alignment"
    body += struct.pack("<Q", len(key)) + key + struct.pack("<I", 4) + struct.pack("<I", 32)
    offset = 0
    total = 0
    for i, (tid, nelem, nbytes) in enumerate(spec):
        name = f"t{i}.weight".encode()
        body += struct.pack("<Q", len(name)) + name
        body += struct.pack("<I", 1) + struct.pack("<Q", nelem)
        body += struct.pack("<I", tid) + struct.pack("<Q", offset)
        offset += nbytes
        total += nbytes
    pad = (-len(body)) % 32
    tmp.write_bytes(bytes(body) + b"\0" * pad + b"\0" * total)
    return tmp


def selftest() -> int:
    ok = True
    with tempfile.TemporaryDirectory() as d:
        d = Path(d)
        # 256 values: S-X8 = 8 blocks x 30 = 240 B; Q1_0 = 2 blocks x 18 = 36 B.
        good   = _build_gguf(d / "good.gguf",   [(41, 256, 240), (0, 64, 256), (41, 512, 480)])
        plain  = _build_gguf(d / "plain.gguf",  [(0, 64, 256), (8, 32, 34)])
        q1_0   = _build_gguf(d / "q1_0.gguf",   [(41, 256, 36), (0, 64, 256)])
        mixed  = _build_gguf(d / "mixed.gguf",  [(41, 256, 240), (41, 256, 36)])
        junk   = _build_gguf(d / "junk.gguf",   [(41, 256, 240), (41, 256, 100)])
        odd    = _build_gguf(d / "odd.gguf",    [(41, 250, 240)])
        done   = _build_gguf(d / "done.gguf",   [(41, 256, 240), (57, 256, 240)])
        empty  = _build_gguf(d / "empty.gguf",  [])
        # a truncated copy of a good file: the last span comes up short
        trunc = d / "trunc.gguf"
        trunc.write_bytes(good.read_bytes()[:-100])

        for label, fn in [
            ("in-place", lambda: retag(good, good)),
            ("no id-41 tensors", lambda: retag(plain, d / "o1.gguf")),
            ("genuine Q1_0 at id 41", lambda: retag(q1_0, d / "o2.gguf")),
            ("mixed S-X8 + Q1_0 spans at id 41", lambda: retag(mixed, d / "o3.gguf")),
            ("id 41 span matching neither", lambda: retag(junk, d / "o4.gguf")),
            ("nelem not a multiple of 32", lambda: retag(odd, d / "o5.gguf")),
            ("already carries id 57", lambda: retag(done, d / "o6.gguf")),
            ("GGUF declaring no tensors", lambda: retag(empty, d / "o7.gguf")),
            ("truncated file", lambda: retag(trunc, d / "o8.gguf")),
        ]:
            try:
                fn()
                print(f"SELFTEST FAIL - {label} was NOT refused")
                ok = False
            except RetagRefusal as exc:
                print(f"SELFTEST ok - {label} refused: {exc}")

        out = d / "out.gguf"
        retag(good, out, map_out=d / "m.tsv")
        ids = [t["tid"] for t in parse_gguf(out)[0]]
        if ids == [57, 0, 57]:
            print(f"SELFTEST ok - GREEN half: 41 -> 57 rewritten, others untouched {ids}")
        else:
            print(f"SELFTEST FAIL - GREEN half wrong: {ids}")
            ok = False

        a, b = good.read_bytes(), out.read_bytes()
        diffs = [i for i in range(len(a)) if a[i] != b[i]]
        type_offsets = {t["type_at"] for t in parse_gguf(good)[0] if t["tid"] == 41}
        allowed = {o + k for o in type_offsets for k in range(4)}
        if len(a) == len(b) and set(diffs) <= allowed:
            print(f"SELFTEST ok - byte-level: only type fields changed ({len(diffs)} bytes in {len(type_offsets)} u32 slots)")
        else:
            print(f"SELFTEST FAIL - bytes changed outside the type fields: {sorted(set(diffs) - allowed)[:8]}")
            ok = False
    print("SELFTEST OK" if ok else "SELFTEST FAILED")
    return 0 if ok else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", nargs="?")
    ap.add_argument("output", nargs="?")
    ap.add_argument("--map-out")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest()
    if not a.input or not a.output:
        ap.error("input and output are required (or use --selftest)")
    try:
        return retag(Path(a.input), Path(a.output), Path(a.map_out) if a.map_out else None)
    except RetagRefusal as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
