"""check_pca_reduces_error.py -- does the PCA term move the S-X8 weights TOWARD the reference?

The decisive test of orientation. A rank-2 correction fitted to cancel the quantization residual is
small in norm by construction, so its norm says nothing about whether it helps. What settles it is
whether adding it REDUCES the distance to the unquantized weight.

Reference: the same base model's Q8_0 GGUF. Q8_0's own error is well under the S-X8 PCA term, so it
stands in for FP16 here. The format's own check is CosSim vs FP16 = 1.000000 with PCA
(SX8_FLASH_V4_3_SPEC.md section 7), which is what this reproduces.

  core          = S-X8 decode without PCA
  core + pca    = with the correction
  ref           = Q8_0 dequantized

If ||core+pca - ref|| < ||core - ref||, the correction is oriented correctly.
"""

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_sx8_pca_fixtures import decode_blocks   # noqa: E402

BPB = 30


def signed4(x):
    return np.where(x < 8, x, x.astype(np.int16) - 16).astype(np.int16)


def dequant_q8_0(raw, n_rows, K):
    """Q8_0: per 32 weights, f16 d + 32 int8. 34 bytes per block."""
    nb = K // 32
    b = raw.reshape(n_rows, nb, 34)
    d = b[:, :, 0:2].copy().view(np.float16).astype(np.float64).reshape(n_rows, nb, 1)
    q = b[:, :, 2:34].view(np.int8).astype(np.float64)
    return (d * q).reshape(n_rows, K)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sx8-gguf", required=True)
    ap.add_argument("--companion", required=True)
    ap.add_argument("--ref-gguf", required=True, help="Q8_0 GGUF of the same base model")
    ap.add_argument("--tensor", default="blk.0.ffn_down.weight")
    ap.add_argument("--rows", type=int, default=256)
    ap.add_argument("--gguf-py", default=None)
    a = ap.parse_args()

    if a.gguf_py:
        sys.path.insert(0, a.gguf_py)
    from gguf.gguf_reader import GGUFReader

    rs = GGUFReader(a.sx8_gguf)
    t = next(x for x in rs.tensors if x.name == a.tensor)
    K, N = int(t.shape[0]), int(t.shape[1])
    n_cb = K // 32
    n_rows = min(a.rows, N)
    raw = np.asarray(t.data).view(np.uint8).reshape(N, n_cb, BPB)[:n_rows].copy()

    rr = GGUFReader(a.ref_gguf)
    tr = next(x for x in rr.tensors if x.name == a.tensor)
    assert int(tr.shape[0]) == K and int(tr.shape[1]) == N, \
        "reference geometry %s vs %s" % (tr.shape, t.shape)
    ref = dequant_q8_0(np.asarray(tr.data).view(np.uint8).reshape(N, -1)[:n_rows].copy(), n_rows, K)

    stem = a.tensor[:-len(".weight")] if a.tensor.endswith(".weight") else a.tensor
    c = GGUFReader(a.companion)
    got = {x.name: np.asarray(x.data) for x in c.tensors if x.name.startswith(stem + ".sx8_pca_")}
    b0 = got[stem + ".sx8_pca_b0"].reshape(n_cb, 32).astype(np.float64)
    b1 = got[stem + ".sx8_pca_b1"].reshape(n_cb, 32).astype(np.float64)
    c0 = got[stem + ".sx8_pca_c0"].reshape(N, n_cb)[:n_rows].astype(np.float64)
    c1 = got[stem + ".sx8_pca_c1"].reshape(N, n_cb)[:n_rows].astype(np.float64)

    core = decode_blocks(raw)                                        # (rows, n_cb, 32)
    pca = c0[:, :, None] * b0[None, :, :] + c1[:, :, None] * b1[None, :, :]

    core2 = core.reshape(n_rows, K)
    with_pca = (core + pca).reshape(n_rows, K)

    def stats(w, label):
        e = w - ref
        rms = np.sqrt((e ** 2).mean())
        cos = float((w * ref).sum() / (np.linalg.norm(w) * np.linalg.norm(ref)))
        print("  %-22s RMS err = %.6e   CosSim = %.8f" % (label, rms, cos))
        return rms

    print("tensor %s  K=%d N=%d n_cb=%d rows=%d" % (a.tensor, K, N, n_cb, n_rows))
    print("  reference              Q8_0 dequantized, RMS = %.6e" % np.sqrt((ref ** 2).mean()))
    e0 = stats(core2, "core (no PCA)")
    e1 = stats(with_pca, "core + PCA")
    # also try the two obvious orientation mistakes, so a wrong answer names itself
    e_neg = stats((core - pca).reshape(n_rows, K), "core - PCA (sign flip)")
    swp = (c1[:, :, None] * b0[None, :, :] + c0[:, :, None] * b1[None, :, :])
    e_swp = stats((core + swp).reshape(n_rows, K), "core + PCA (c0/c1 swap)")

    print()
    print("  PCA changes the error by %+.3f%%" % (100 * (e1 - e0) / e0))
    best = min([(e0, "core"), (e1, "core+PCA"), (e_neg, "core-PCA"), (e_swp, "swapped")])[1]
    print("  closest to the reference: %s" % best)
    print("VERDICT: %s" % ("PCA REDUCES the error -- orientation correct"
                           if e1 < e0 else
                           "PCA does NOT reduce the error -- orientation or indexing is wrong"))
    return 0 if e1 < e0 else 3


if __name__ == "__main__":
    sys.exit(main())
