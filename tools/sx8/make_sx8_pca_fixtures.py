"""make_sx8_pca_fixtures.py -- goldens for tests/test-sx8-pca.cpp.

Takes ONE real S-X8 tensor out of the 4B GGUF (a row slice, to keep the fixtures small), its PCA
companions out of the companion GGUF, and computes in float64 -- straight from
SX8_FLASH_V4_3_SPEC.md sections 3-5, with no engine code involved -- both

  y_core[n] = sum_k X[k] * W[n,k]                     (today's path)
  y_pca [n] = y_core[n] + sum_kb c0(kb,n)*Z0[kb] + c1(kb,n)*Z1[kb]

Writes raw little-endian .bin files the C++ test reads back.
"""

import argparse
import os
import sys

import numpy as np

BPB = 30


def decode_blocks(raw):
    """raw (n_rows, n_cb, 30) uint8 -> float64 weights (n_rows, n_cb, 32)."""
    n_rows, n_cb, _ = raw.shape
    lo = raw[:, :, 0:2].copy().view(np.float16).astype(np.float64).reshape(n_rows, n_cb)
    hi = raw[:, :, 2:4].copy().view(np.float16).astype(np.float64).reshape(n_rows, n_cb)
    cfg = raw[:, :, 4].astype(np.int32)
    qh = raw[:, :, 5:21]
    ql = raw[:, :, 21:29]

    q = (hi - lo) * 0.25
    w = np.empty((n_rows, n_cb, 32), dtype=np.float64)
    for j in range(32):
        sb = j >> 3
        s = (cfg >> (sb * 2)) & 3
        rlo = lo + q * (3 * (s == 2) + (s == 3))
        rhi = hi - q * (3 * (s == 1) + (s == 3))
        step = np.maximum((rhi - rlo) * 0.015873, 1e-10)
        nib = (qh[:, :, j >> 1].astype(np.int32) >> ((j & 1) * 4)) & 0xF
        qua = (ql[:, :, j >> 2].astype(np.int32) >> ((3 - (j & 3)) * 2)) & 0x3
        w[:, :, j] = rlo + step * ((nib << 2) | qua)
    return w


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gguf", required=True)
    ap.add_argument("--companion", required=True)
    ap.add_argument("--tensor", default="blk.0.ffn_down.weight")
    ap.add_argument("--rows", type=int, default=64)
    ap.add_argument("--cols", type=int, default=3)
    ap.add_argument("--out", required=True)
    ap.add_argument("--gguf-py", default=None)
    a = ap.parse_args()

    if a.gguf_py:
        sys.path.insert(0, a.gguf_py)
    from gguf.gguf_reader import GGUFReader

    os.makedirs(a.out, exist_ok=True)
    r = GGUFReader(a.gguf)
    t = next(x for x in r.tensors if x.name == a.tensor)
    K, N = int(t.shape[0]), int(t.shape[1])
    n_cb = K // 32
    n_rows = min(a.rows, N)
    raw = np.asarray(t.data).view(np.uint8).reshape(N, n_cb, BPB)[:n_rows].copy()

    stem = a.tensor[:-len(".weight")] if a.tensor.endswith(".weight") else a.tensor
    c = GGUFReader(a.companion)
    got = {x.name: np.asarray(x.data) for x in c.tensors
           if x.name.startswith(stem + ".sx8_pca_")}
    b0 = got[stem + ".sx8_pca_b0"].reshape(n_cb, 32).astype(np.float64)
    b1 = got[stem + ".sx8_pca_b1"].reshape(n_cb, 32).astype(np.float64)
    c0 = got[stem + ".sx8_pca_c0"].reshape(N, n_cb)[:n_rows]
    c1 = got[stem + ".sx8_pca_c1"].reshape(N, n_cb)[:n_rows]

    # Cross-check the coefficient planes against the blocks they came from, so a mistake in
    # add_sx8_pca_to_gguf.py cannot hide behind a fixture built from its own output.
    coeff = raw[:, :, 29]
    ref0 = np.where((coeff & 0xF) < 8, coeff & 0xF, (coeff & 0xF).astype(np.int16) - 16)
    ref1 = np.where((coeff >> 4) < 8, coeff >> 4, (coeff >> 4).astype(np.int16) - 16)
    assert np.array_equal(c0.astype(np.int16), ref0), "c0 plane does not match block byte 29"
    assert np.array_equal(c1.astype(np.int16), ref1), "c1 plane does not match block byte 29"
    print("coeff planes match block byte 29 for %d x %d blocks" % (n_rows, n_cb))

    rng = np.random.default_rng(20260915)
    x = rng.standard_normal((a.cols, K)).astype(np.float32)
    xd = x.astype(np.float64)

    w = decode_blocks(raw).reshape(n_rows, K)                      # (n_rows, K)
    y_core = xd @ w.T                                              # (cols, n_rows)

    xb = xd.reshape(a.cols, n_cb, 32)
    z0 = np.einsum("mct,ct->mc", xb, b0)                           # scales already folded in
    z1 = np.einsum("mct,ct->mc", xb, b1)
    y_pca = y_core + z0 @ c0.astype(np.float64).T + z1 @ c1.astype(np.float64).T

    def dump(name, arr, dtype):
        p = os.path.join(a.out, name)
        np.ascontiguousarray(arr, dtype=dtype).tofile(p)
        return p

    dump("w.bin", raw, np.uint8)
    dump("b0.bin", got[stem + ".sx8_pca_b0"].reshape(n_cb, 32), np.float32)
    dump("b1.bin", got[stem + ".sx8_pca_b1"].reshape(n_cb, 32), np.float32)
    dump("c0.bin", c0, np.float16)
    dump("c1.bin", c1, np.float16)
    dump("x.bin", x, np.float32)
    dump("y_core.bin", y_core, np.float32)
    dump("y_pca.bin", y_pca, np.float32)
    with open(os.path.join(a.out, "dims.txt"), "w") as f:
        f.write("%d %d %d %d\n" % (K, n_rows, n_cb, a.cols))

    rel = np.abs(y_pca - y_core).max() / max(np.abs(y_core).max(), 1e-9)
    print("tensor=%s K=%d n_cb=%d rows=%d cols=%d" % (a.tensor, K, n_cb, n_rows, a.cols))
    print("PCA term moves the output by up to %.3f%% of |y_core|max -> %s" % (100 * rel, a.out))


if __name__ == "__main__":
    main()
