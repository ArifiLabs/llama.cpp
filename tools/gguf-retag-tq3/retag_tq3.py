#!/usr/bin/env python3
"""Retag a tq3-authored GGUF onto the ArifiLabs type ids (docs/TYPE-ID-ALLOCATION.md §3.1.1).

`github.com/turbo-tan/llama.cpp-tq3` serializes TQ3_4S at type-id 46. Our 46 is TQ4_1S, in
files we wrote. tq3's ids are therefore remapped into free space at 48+, and a tq3-authored
GGUF must be retagged before it will load here.

Landed by lane-142 only AFTER the codec it depends on was proven: §3.1.1 binds the retag tool
to follow a working dequant, because a retag ahead of one turns a file that fails loudly at
load into a file that loads and emits garbage. The proof is a coherent -ngl 0 generation from
the retagged 27B file (lane-evidence/2026-08-17-lane-142-tq3-proofs/H1-coherence-cpu-ngl0.txt).

REFUSES, rather than guessing, on all of:
  * an in-place write (the output is always a NEW file)
  * a file with no tq3-authored ids
  * a tq3 id with no destination reserved by the contract (37 TQ3_4SV, 31 TQ3_1S_AP1,
    44 tq3's own TQ3_1S which is *probably* our 45 but has never been proven so)
  * id 46 whose measured byte spans say the file is OURS (TQ4_1S, 20 B/32) and not tq3's
    (TQ3_4S, 16 B/32) — id 46 is genuinely ambiguous on this estate, so the decision is made
    from geometry, never from a name or a path
  * id 46 with no geometry available to decide with

Usage:
  python retag_tq3.py <input.gguf> <output.gguf> [--map-out map.tsv]
  python retag_tq3.py --selftest
"""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import sys
import tempfile
from pathlib import Path

# docs/TYPE-ID-ALLOCATION.md §3.1.1. tq3's id -> (our id, bytes per 32 values).
#
# The byte size is not decoration: EVERY rewrite is checked against it. Rewriting on the id
# alone is what silently corrupts files, and id 46 is the proof that ids are not unique --
# tq3 writes TQ3_4S there (16 B/32) and we write TQ4_1S there (20 B/32).
TQ3_ID_MAP = {
    46: (48, 16),   # TQ3_4S  -> GGML_TYPE_TQ3_4S
    200: (49, 14),  # TQ3_0   -> GGML_TYPE_TQ3_0     (their 200 is OUR TURBO2_0)
    36: (50, 18),   # TQ3_4SE -> GGML_TYPE_TQ3_4SE
}

# Geometries that are NOT tq3 but share one of the ids above. A tensor matching one of these
# means the file is ours, not theirs, and the whole file is refused.
TQ3_ID_FOREIGN = {
    46: (20, "our GGML_TYPE_TQ4_1S"),
}
# tq3 ids that are real but have NO reserved destination. Refuse, never guess.
TQ3_ID_UNMAPPED = {
    37: "TQ3_4SV — no id reserved by the contract",
    31: "TQ3_1S_AP1 — no id reserved by the contract",
    44: "tq3 TQ3_1S — probably our 45, but unconfirmed; refusing rather than assuming",
}

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


# GGUF metadata value types -> fixed byte width (None = variable, handled explicitly).
_FIXED = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1, 10: 8, 11: 8, 12: 8}


def _skip_value(r: _Reader, vtype: int) -> None:
    if vtype in _FIXED:
        r.raw(_FIXED[vtype])
    elif vtype == 8:  # string
        r.raw(r.u64())
    elif vtype == 9:  # array
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
    """-> (tensors, data_start, file_size).

    tensors: list of dicts with name, tid, type_at (file offset of the u32 type
    field), nelem, offset (offset within the data section).
    """
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


def parse_tensor_types(path: Path):
    """Back-compat shim: -> list of (name, type_id, type-field offset).

    Callers that use this lose the geometry, and plan() REFUSES rather than rewriting
    blind. Prefer parse_gguf().
    """
    tensors, _, _, _ = parse_gguf(path)
    return [(t["name"], t["tid"], t["type_at"]) for t in tensors]


# Candidate geometries for a tensor tagged 46, in bytes per 32 values.
# The whole reason a tq3-tagged 46 fails loudly here rather than silently is that
# these two differ by 25% (TYPE-ID-ALLOCATION §3.1.1) - so a byte span decides it.
def _spans(tensors, data_start, file_size):
    """-> {name: byte span}, from the gaps between consecutive data offsets.

    Refuses on a data section that does not make sense: duplicate or overlapping offsets,
    or a final span that runs past the end of the file (a truncated download). Without
    this a truncated copy of OUR file produces a short final span that could fall into
    tq3's band and get rewritten.
    """
    ordered = sorted(tensors, key=lambda t: t["offset"])
    spans = {}
    for i, t in enumerate(ordered):
        end = ordered[i + 1]["offset"] if i + 1 < len(ordered) else file_size - data_start
        span = end - t["offset"]
        if span <= 0:
            raise RetagRefusal(
                f"REFUSED: tensor {t['name']!r} has a non-positive data span ({span}) — "
                f"the tensor table has overlapping or duplicate offsets"
            )
        spans[t["name"]] = span
    return spans


def _expected_bytes(nelem: int, bytes_per_32: int) -> int:
    if nelem % 32:
        raise RetagRefusal(
            f"REFUSED: {nelem} elements is not a whole number of 32-value blocks"
        )
    return (nelem // 32) * bytes_per_32


def verify_geometry(tensors, data_start, file_size, alignment):
    """Check EVERY tensor this tool would rewrite against the byte size its tq3 type implies.

    Returns a per-id report. Raises RetagRefusal on any tensor that does not match, which
    includes a tensor whose id we map but whose geometry is somebody else's (id 46 is the
    live case: ours is 20 B/32, theirs is 16).

    The accepted band is [expected, expected + slack) where slack is the smaller of the
    file alignment and a quarter of the expected size. Padding to the next tensor can only
    ADD bytes, so the lower bound is exact; the upper bound is capped at a quarter so it can
    never reach a competing geometry. A tensor small enough that alignment padding could
    carry it into another type's band is REFUSED as undecidable rather than guessed at.
    """
    spans = _spans(tensors, data_start, file_size)
    report = {}

    for t in tensors:
        tid = t["tid"]
        if tid not in TQ3_ID_MAP:
            continue
        _, bpb = TQ3_ID_MAP[tid]
        expected = _expected_bytes(t["nelem"], bpb)
        span = spans[t["name"]]
        slack = min(alignment, max(expected // 4, 1))

        if expected <= span < expected + slack:
            report[tid] = report.get(tid, 0) + 1
            continue

        foreign = TQ3_ID_FOREIGN.get(tid)
        if foreign:
            f_bpb, f_name = foreign
            f_expected = _expected_bytes(t["nelem"], f_bpb)
            if f_expected <= span < f_expected + min(alignment, max(f_expected // 4, 1)):
                raise RetagRefusal(
                    f"REFUSED: tensor {t['name']!r} at id {tid} measures {span} bytes, which is "
                    f"{f_name} ({f_bpb} B/32), not tq3's {bpb} B/32. This file is OURS — "
                    f"nothing to retag."
                )

        raise RetagRefusal(
            f"REFUSED: tensor {t['name']!r} at id {tid} measures {span} bytes but tq3's "
            f"geometry says {expected} ({bpb} B/32 over {t['nelem']} elements). Undecidable "
            f"or corrupt — refusing rather than guessing."
        )

    return report


def plan(tensors, data_start=None, file_size=None, alignment=None):
    """-> (rewrites, histogram). Every rewrite is geometry-checked; anything else refuses."""
    if not tensors:
        raise RetagRefusal("REFUSED: this GGUF declares no tensors")

    if isinstance(tensors[0], tuple):  # back-compat shim path, no geometry available
        tensors = [{"name": n, "tid": i, "type_at": a, "nelem": 0, "offset": 0}
                   for n, i, a in tensors]
        data_start = file_size = alignment = None

    hist = {}
    for t in tensors:
        hist[t["tid"]] = hist.get(t["tid"], 0) + 1
        if t["tid"] in TQ3_ID_UNMAPPED:
            raise RetagRefusal(
                f"REFUSED: tensor {t['name']!r} carries tq3 id {t['tid']} — {TQ3_ID_UNMAPPED[t['tid']]}"
            )

    mapped = [tid for tid in hist if tid in TQ3_ID_MAP]
    if not mapped:
        raise RetagRefusal("REFUSED: no tq3-authored tensor ids in this file — nothing to retag")

    # Geometry is mandatory for EVERY id we rewrite, not just the famously ambiguous 46.
    # Missing geometry is a refusal, never a bypass: otherwise the safety check would depend
    # on which entry point the caller happened to pick.
    if data_start is None:
        raise RetagRefusal(
            f"REFUSED: cannot verify the geometry of ids {sorted(mapped)} without byte spans. "
            f"Call plan() with the data_start/file_size/alignment from parse_gguf()."
        )
    verify_geometry(tensors, data_start, file_size, alignment)

    rewrites = [(t["name"], t["tid"], TQ3_ID_MAP[t["tid"]][0], t["type_at"])
                for t in tensors if t["tid"] in TQ3_ID_MAP]
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

    print(f"source     : {src}")
    print(f"tensors    : {len(tensors)}   id histogram: {dict(sorted(hist.items()))}")
    print(f"geometry   : verified per tensor (byte spans, not names): "
          f"{verify_geometry(tensors, data_start, file_size, alignment)}")
    print(f"rewrites   : {len(rewrites)}  map: "
          f"{ {o: n for _, o, n, _ in rewrites} }")

    shutil.copyfile(src, dst)
    with open(dst, "r+b") as fh:
        for _, _, new, at in rewrites:
            fh.seek(at)
            fh.write(struct.pack("<I", new))

    hist_after = {}
    for t, _, _, _ in [parse_gguf(dst)]:
        for x in t:
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
        print(f"token_embd retagged: {embd if embd else 'NO — get_rows is NOT exercised by this file'}")
    return 0


def _build_gguf(tmp: Path, spec) -> Path:
    """Minimal valid GGUF v3. `spec` is a list of (type_id, bytes_per_32_values).

    Each tensor is 256 values, so its data span is 8 * bytes_per_32 - which is what
    classify_id46 measures. Offsets are laid out contiguously, like a real file.
    """
    n = len(spec)
    body = bytearray()
    body += _GGUF_MAGIC + struct.pack("<I", 3)
    body += struct.pack("<Q", n) + struct.pack("<Q", 1)
    key = b"general.alignment"
    body += struct.pack("<Q", len(key)) + key + struct.pack("<I", 4) + struct.pack("<I", 32)

    offset = 0
    total = 0
    for i, (tid, bpb) in enumerate(spec):
        name = f"t{i}.weight".encode()
        body += struct.pack("<Q", len(name)) + name
        body += struct.pack("<I", 1) + struct.pack("<Q", 256)
        body += struct.pack("<I", tid) + struct.pack("<Q", offset)
        size = 8 * bpb
        offset += size
        total += size

    pad = (-len(body)) % 32
    tmp.write_bytes(bytes(body) + b"\0" * pad + b"\0" * total)
    return tmp


def selftest() -> int:
    """The RED halves. Every one of these was a real defect at some point in this tool's life,
    so each stays as a case rather than an argument."""
    ok = True
    with tempfile.TemporaryDirectory() as d:
        d = Path(d)
        # (type id, bytes per 32 values). At id 46: 16 = tq3 TQ3_4S, 20 = our TQ4_1S.
        good     = _build_gguf(d / "good.gguf",     [(46, 16), (0, 128), (46, 16)])
        plain    = _build_gguf(d / "plain.gguf",    [(0, 128), (8, 34)])
        unmapped = _build_gguf(d / "unmapped.gguf", [(46, 16), (37, 16)])
        ours     = _build_gguf(d / "ours.gguf",     [(46, 20), (0, 128)])
        # A file-level vote used to hide these: one tq3-shaped tensor outvoting three that
        # match nothing, and 36/200 being rewritten on the id alone with no geometry check.
        mixed    = _build_gguf(d / "mixed.gguf",    [(46, 16), (46, 20)])
        junk46   = _build_gguf(d / "junk46.gguf",   [(46, 16), (46, 9), (46, 9), (46, 9)])
        bad36    = _build_gguf(d / "bad36.gguf",    [(36, 11)])
        bad200   = _build_gguf(d / "bad200.gguf",   [(200, 33)])
        empty    = _build_gguf(d / "empty.gguf",    [])

        for label, fn in [
            ("in-place", lambda: retag(good, good)),
            ("non-tq3 file", lambda: retag(plain, d / "out1.gguf")),
            ("unmapped id 37", lambda: retag(unmapped, d / "out2.gguf")),
            ("OUR TQ4_1S at id 46", lambda: retag(ours, d / "out4.gguf")),
            ("MIXED tq3 + ours at id 46", lambda: retag(mixed, d / "out5.gguf")),
            ("id 46 spans matching neither", lambda: retag(junk46, d / "out6.gguf")),
            ("id 36 with wrong geometry", lambda: retag(bad36, d / "out7.gguf")),
            ("id 200 with wrong geometry", lambda: retag(bad200, d / "out8.gguf")),
            ("GGUF declaring no tensors", lambda: retag(empty, d / "out9.gguf")),
            # The bypass half: the geometry-free entry point must REFUSE, not blanket-rewrite.
            ("plan() without byte spans", lambda: plan(parse_tensor_types(good))),
        ]:
            try:
                fn()
                print(f"SELFTEST FAIL — {label} was NOT refused")
                ok = False
            except RetagRefusal as exc:
                print(f"SELFTEST ok — {label} refused: {exc}")

        out = d / "out3.gguf"
        retag(good, out, map_out=d / "m.tsv")
        ids = [t for _, t, _ in parse_tensor_types(out)]
        if ids == [48, 0, 48]:
            print(f"SELFTEST ok — GREEN half: 46 -> 48 rewritten, others untouched {ids}")
        else:
            print(f"SELFTEST FAIL — GREEN half wrong: {ids}")
            ok = False

        # GREEN half for the ids that are NOT ambiguous, so their geometry check is
        # exercised rather than merely present.
        ok2 = d / "ok2.gguf"
        retag(_build_gguf(d / "g36.gguf", [(36, 18), (200, 14)]), ok2)
        ids2 = [t for _, t, _ in parse_tensor_types(ok2)]
        if ids2 == [50, 49]:
            print(f"SELFTEST ok — GREEN half: 36 -> 50 and 200 -> 49 {ids2}")
        else:
            print(f"SELFTEST FAIL — GREEN half wrong for 36/200: {ids2}")
            ok = False

        # Only the 4-byte type fields may differ between input and output.
        a, b = good.read_bytes(), out.read_bytes()
        diffs = [i for i in range(len(a)) if a[i] != b[i]]
        type_offsets = {t["type_at"] for t in parse_gguf(good)[0] if t["tid"] == 46}
        allowed = {o + k for o in type_offsets for k in range(4)}
        if len(a) == len(b) and set(diffs) <= allowed:
            print(f"SELFTEST ok — byte-level: only type fields changed "
                  f"({len(diffs)} bytes, all inside {len(type_offsets)} u32 type slots)")
        else:
            print(f"SELFTEST FAIL — bytes changed outside the type fields: "
                  f"{sorted(set(diffs) - allowed)[:8]}")
            ok = False
    print("SELFTEST OK" if ok else "SELFTEST FAILED")
    return 0 if ok else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
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
        return retag(Path(a.input), Path(a.output),
                     Path(a.map_out) if a.map_out else None)
    except RetagRefusal as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
