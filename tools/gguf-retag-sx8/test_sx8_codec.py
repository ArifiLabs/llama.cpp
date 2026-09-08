#!/usr/bin/env python3
"""Cross-check the BUILT S-X8 codec (GGML_TYPE_SX8 = 57) against an independent numpy decoder.

Loads ggml-base + ggml-cpu from a build's bin directory through ctypes and checks:
  1. decode parity   - dequantize_row_sx8 on random 30-byte blocks == a numpy port of the author's
                       kernel_sx8_v43.py "decode V6" (PCA term excluded, as in every llama.cpp path)
  2. round trip      - quantize_row_sx8_ref -> dequantize_row_sx8 on random rows: small error,
                       dmin/dmax are the row block min/max, coeff == 0
  3. vec_dot         - the CPU vec_dot (the ISA variant the loader picked) against an exact float64
                       dot of dequantized weights and dequantized Q8_1 activations (y = d * qs)
  4. RED half        - the same blocks through the author's activation formula (y = d*qs + s/32),
                       reported next to (3). It is NOT what this tree computes; it is printed so the
                       deviation pre-registered in PREREG-wi1692-sx8-port.md section 4 is visible.

Usage:
  python test_sx8_codec.py --bin <dir holding ggml-base.dll and ggml-cpu.dll> [--seed N] [--json out.json]
Exit 0 on GREEN, 1 on any failed check, 3 if the type is missing from the build.
"""

from __future__ import annotations

import argparse
import ctypes
import json
import os
import sys
from pathlib import Path

import numpy as np

SX8_ID = 57
Q8_1_ID = 9
QK = 32
BLOCK_BYTES = 30
Q8_1_BYTES = 36  # d f16, s f16, qs[32]


class TypeTraits(ctypes.Structure):
    _fields_ = [
        ("type_name", ctypes.c_char_p),
        ("blck_size", ctypes.c_int64),
        ("blck_size_interleave", ctypes.c_int64),
        ("type_size", ctypes.c_size_t),
        ("is_quantized", ctypes.c_bool),
        ("to_float", ctypes.c_void_p),
        ("from_float_ref", ctypes.c_void_p),
    ]


FROM_FLOAT_T = ctypes.CFUNCTYPE(None, ctypes.POINTER(ctypes.c_float), ctypes.c_void_p, ctypes.c_int64)
VEC_DOT_T = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.POINTER(ctypes.c_float), ctypes.c_size_t,
                             ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int)


class TypeTraitsCpu(ctypes.Structure):
    _fields_ = [
        ("from_float", FROM_FLOAT_T),
        ("vec_dot", VEC_DOT_T),
        ("vec_dot_type", ctypes.c_int),
        ("nrows", ctypes.c_int64),
    ]


def load(bin_dir: Path):
    bin_dir = Path(bin_dir).resolve()
    if hasattr(os, "add_dll_directory"):
        os.add_dll_directory(str(bin_dir))
    # Windows: winmode=0 = the legacy loader search (application dir + loaded modules + PATH). The default strict
    # mode (LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) refuses WinLibs' libgomp-1.dll even from an add_dll_directory dir
    # (probed 2026-09-07: every other runtime DLL loads strict, libgomp only legacy), so the MinGW runtime must be
    # on PATH or beside the DLLs. No effect on POSIX.
    winmode = 0 if os.name == "nt" else None
    base = ctypes.CDLL(str(bin_dir / ("ggml-base.dll" if os.name == "nt" else "libggml-base.so")), winmode=winmode)
    cpu = ctypes.CDLL(str(bin_dir / ("ggml-cpu.dll" if os.name == "nt" else "libggml-cpu.so")), winmode=winmode)
    base.ggml_get_type_traits.restype = ctypes.POINTER(TypeTraits)
    base.ggml_get_type_traits.argtypes = [ctypes.c_int]
    base.dequantize_row_sx8.restype = None
    base.dequantize_row_sx8.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.c_int64]
    base.quantize_row_sx8_ref.restype = None
    base.quantize_row_sx8_ref.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.c_void_p, ctypes.c_int64]
    cpu.ggml_cpu_init.restype = None
    cpu.ggml_cpu_init()
    cpu.ggml_get_type_traits_cpu.restype = ctypes.POINTER(TypeTraitsCpu)
    cpu.ggml_get_type_traits_cpu.argtypes = [ctypes.c_int]
    return base, cpu


# ---- independent decoder: numpy port of kernel_sx8_v43.py (MarlaLabs), PCA term excluded

def ref_decode(blocks: np.ndarray) -> np.ndarray:
    """blocks: (nb, 30) uint8 -> (nb, 32) float32, computed in float32 like the author's kernel."""
    nb = blocks.shape[0]
    dmin = blocks[:, 0:2].copy().view(np.float16).astype(np.float32).reshape(nb)
    dmax = blocks[:, 2:4].copy().view(np.float16).astype(np.float32).reshape(nb)
    cfg = blocks[:, 4].astype(np.int32)
    qh = blocks[:, 5:21].astype(np.int32)
    ql = blocks[:, 21:29].astype(np.int32)
    j = np.arange(QK)
    hi = (qh[:, j >> 1] >> ((j & 1) * 4)) & 0xF
    lo = (ql[:, j >> 2] >> ((3 - (j & 3)) * 2)) & 0x3
    lv = ((hi << 2) | lo).astype(np.float32)
    q = ((dmax - dmin) * np.float32(0.25)).astype(np.float32)
    s = (cfg[:, None] >> ((j >> 3) * 2)) & 3
    rlo = (dmin[:, None] + q[:, None] * (3 * (s == 2) + (s == 3)).astype(np.float32)).astype(np.float32)
    rhi = (dmax[:, None] - q[:, None] * (3 * (s == 1) + (s == 3)).astype(np.float32)).astype(np.float32)
    step = ((rhi - rlo) * np.float32(0.015873)).astype(np.float32)
    step = np.maximum(step, np.float32(1e-10))
    return (rlo + step * lv).astype(np.float32)


def random_blocks(rng: np.random.Generator, nb: int) -> np.ndarray:
    out = np.zeros((nb, BLOCK_BYTES), dtype=np.uint8)
    lo = rng.uniform(-4.0, 2.0, nb).astype(np.float16)
    hi = (lo.astype(np.float32) + rng.uniform(0.01, 6.0, nb)).astype(np.float16)
    out[:, 0:2] = lo.view(np.uint8).reshape(nb, 2)
    out[:, 2:4] = hi.view(np.uint8).reshape(nb, 2)
    out[:, 4:] = rng.integers(0, 256, (nb, BLOCK_BYTES - 4), dtype=np.uint8)
    return out


def c_dequant(base, blocks: np.ndarray) -> np.ndarray:
    nb = blocks.shape[0]
    buf = np.ascontiguousarray(blocks).tobytes()
    out = (ctypes.c_float * (nb * QK))()
    base.dequantize_row_sx8(ctypes.c_char_p(buf), out, nb * QK)
    return np.frombuffer(out, dtype=np.float32).reshape(nb, QK).copy()


def c_quant(base, x: np.ndarray) -> np.ndarray:
    n = x.size
    assert n % QK == 0
    xin = np.ascontiguousarray(x, dtype=np.float32)
    out = (ctypes.c_uint8 * (n // QK * BLOCK_BYTES))()
    base.quantize_row_sx8_ref(xin.ctypes.data_as(ctypes.POINTER(ctypes.c_float)), out, n)
    return np.frombuffer(out, dtype=np.uint8).reshape(n // QK, BLOCK_BYTES).copy()


def q8_1_quant(cpu, y: np.ndarray) -> np.ndarray:
    tr = cpu.ggml_get_type_traits_cpu(Q8_1_ID).contents
    n = y.size
    yin = np.ascontiguousarray(y, dtype=np.float32)
    out = (ctypes.c_uint8 * (n // QK * Q8_1_BYTES))()
    tr.from_float(yin.ctypes.data_as(ctypes.POINTER(ctypes.c_float)), ctypes.cast(out, ctypes.c_void_p), n)
    return np.frombuffer(out, dtype=np.uint8).reshape(n // QK, Q8_1_BYTES).copy()


def q8_1_fields(blocks: np.ndarray):
    nb = blocks.shape[0]
    d = blocks[:, 0:2].copy().view(np.float16).astype(np.float64).reshape(nb)
    s = blocks[:, 2:4].copy().view(np.float16).astype(np.float64).reshape(nb)
    qs = blocks[:, 4:36].view(np.int8).astype(np.float64)
    return d, s, qs


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", required=True)
    ap.add_argument("--seed", type=int, default=1234)
    ap.add_argument("--json")
    a = ap.parse_args(argv)

    base, cpu = load(Path(a.bin))
    tr = base.ggml_get_type_traits(SX8_ID).contents
    name = tr.type_name.decode() if tr.type_name else ""
    if name != "sx8" or tr.blck_size != QK or tr.type_size != BLOCK_BYTES:
        print(f"MISSING: type {SX8_ID} in this build is name={name!r} blck={tr.blck_size} size={tr.type_size}")
        return 3
    trc = cpu.ggml_get_type_traits_cpu(SX8_ID).contents
    if trc.vec_dot_type != Q8_1_ID or not trc.vec_dot:
        print(f"MISSING: CPU traits for {SX8_ID}: vec_dot_type={trc.vec_dot_type}")
        return 3

    rng = np.random.default_rng(a.seed)
    report = {"bin": str(Path(a.bin).resolve()), "seed": a.seed, "checks": {}}
    ok = True

    # 1. decode parity on random blocks (every config, every level pattern)
    blocks = random_blocks(rng, 4096)
    ref = ref_decode(blocks)
    got = c_dequant(base, blocks)
    scale = np.maximum(np.abs(ref).max(axis=1, keepdims=True), 1e-6)
    rel = np.abs(got - ref) / scale
    c1 = float(rel.max())
    report["checks"]["decode_parity_max_rel"] = c1
    passed = c1 < 1e-5
    ok &= passed
    print(f"[1] decode parity vs numpy port of kernel_sx8_v43: max rel diff {c1:.3e} over {blocks.shape[0]} blocks -> {'OK' if passed else 'FAIL'}")

    # 2. quantize round trip
    x = rng.normal(0.0, 1.0, 64 * QK).astype(np.float32)
    qb = c_quant(base, x)
    xr = c_dequant(base, qb).reshape(-1)
    rmse = float(np.sqrt(np.mean((xr - x) ** 2)))
    rng_span = float(x.max() - x.min())
    dmin = qb[:, 0:2].copy().view(np.float16).astype(np.float32).reshape(-1)
    dmax = qb[:, 2:4].copy().view(np.float16).astype(np.float32).reshape(-1)
    xb = x.reshape(-1, QK)
    minmax_ok = bool(np.allclose(dmin, xb.min(axis=1).astype(np.float16).astype(np.float32)) and
                     np.allclose(dmax, xb.max(axis=1).astype(np.float16).astype(np.float32)))
    coeff_ok = bool((qb[:, 29] == 0).all())
    # 6-bit uniform over at most the block range: RMSE well under range/63
    passed = rmse < rng_span / 63.0 and minmax_ok and coeff_ok
    ok &= passed
    report["checks"]["roundtrip_rmse"] = rmse
    report["checks"]["roundtrip_range"] = rng_span
    report["checks"]["roundtrip_minmax_ok"] = minmax_ok
    report["checks"]["roundtrip_coeff_zero"] = coeff_ok
    print(f"[2] quantize round trip: rmse {rmse:.4e} (range {rng_span:.2f}, range/63 = {rng_span/63:.4e}), minmax {minmax_ok}, coeff==0 {coeff_ok} -> {'OK' if passed else 'FAIL'}")

    # 3 + 4. vec_dot vs exact, and the author's formula next to it
    worst_ours = 0.0
    worst_author = 0.0
    n = 2048
    for trial in range(8):
        w = rng.normal(0.0, 1.0, n).astype(np.float32)
        y = (0.1 + 2.0 * np.cos(np.arange(n) + trial)).astype(np.float32)  # non-zero mean like test-quantize-fns
        wq = c_quant(base, w)
        wd = c_dequant(base, wq).reshape(-1).astype(np.float64)
        yq = q8_1_quant(cpu, y)
        d, s, qs = q8_1_fields(yq)
        yd = (qs * d[:, None]).reshape(-1)
        exact = float(np.dot(wd, yd))
        author = float(np.dot(wd, (qs * d[:, None] + (s / QK)[:, None]).reshape(-1)))
        out = ctypes.c_float(0.0)
        trc.vec_dot(n, ctypes.byref(out), 0, ctypes.c_char_p(wq.tobytes()), 0, ctypes.c_char_p(yq.tobytes()), 0, 1)
        denom = max(float(np.abs(wd).sum() * np.abs(yd).max()), 1e-9)
        worst_ours = max(worst_ours, abs(out.value - exact) / denom)
        worst_author = max(worst_author, abs(author - exact) / denom)
    passed = worst_ours < 1e-5
    ok &= passed
    report["checks"]["vec_dot_max_rel_err_ours"] = worst_ours
    report["checks"]["vec_dot_max_rel_err_author_formula"] = worst_author
    print(f"[3] built vec_dot vs exact float64 dot (n={n}, 8 trials): max rel err {worst_ours:.3e} -> {'OK' if passed else 'FAIL'}")
    print(f"[4] RED half, author's y = d*qs + s/32 on the same blocks: max rel err {worst_author:.3e} (NOT what this tree computes; the term is dropped, see PREREG section 4)")

    report["verdict"] = "GREEN" if ok else "RED"
    if a.json:
        Path(a.json).write_text(json.dumps(report, indent=1), encoding="utf-8")
    print(report["verdict"])
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
