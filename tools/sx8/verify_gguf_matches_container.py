"""verify_gguf_matches_container.py -- do the GGUF's S-X8 blocks and the container's come from the
same quantization run?

The PCA bases/scales only correspond to the GGUF's coefficient bytes if both files are the same
quantization. This pulls ONE tensor's block payload out of the container over HTTP ranges and
compares it, field by field, with the same tensor's blocks in the GGUF.
"""

import argparse
import struct
import sys

import numpy as np

sys.path.insert(0, __file__.rsplit("/", 1)[0] if "/" in __file__ else ".")
from extract_sx8_pca import RangeReader, MAGIC, gguf_name   # noqa: E402

BPB = 30


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", required=True)
    ap.add_argument("--gguf", required=True)
    ap.add_argument("--tensor", default="blk.0.ffn_down.weight", help="GGUF tensor name")
    ap.add_argument("--gguf-py", default=None)
    a = ap.parse_args()

    if a.gguf_py:
        sys.path.insert(0, a.gguf_py)
    from gguf.gguf_reader import GGUFReader

    r = GGUFReader(a.gguf)
    t = next(x for x in r.tensors if x.name == a.tensor)
    K, N = int(t.shape[0]), int(t.shape[1])
    n_cb = K // 32
    g = np.asarray(t.data).view(np.uint8).reshape(N, n_cb, BPB)
    print("gguf %s: K=%d N=%d n_cb=%d blocks=%d" % (a.tensor, K, N, n_cb, N * n_cb))

    f = RangeReader(a.url)
    assert f.read(8) == MAGIC
    f.read(1)
    (ml,) = struct.unpack("<I", f.read(4)); f.read(ml)
    (nt,) = struct.unpack("<I", f.read(4))
    for _ in range(nt):
        (nl,) = struct.unpack("<I", f.read(4))
        name = f.read(nl).decode()
        sh = struct.unpack("<II", f.read(8))
        (n_os,) = struct.unpack("<B", f.read(1)); f.read(4 * n_os)
        nb, ncb = struct.unpack("<II", f.read(8))
        if gguf_name(name) != a.tensor:
            f.seek(nb * BPB + 4 + ncb * 256 + ncb * 8, 1)
            continue
        print("container %s: shape=%s nb=%d n_cb=%d" % (name, sh, nb, ncb))
        assert nb == N * n_cb and ncb == n_cb, "geometry differs!"
        dmin = np.frombuffer(f.read(nb * 2), np.uint8).reshape(nb, 2)
        dmax = np.frombuffer(f.read(nb * 2), np.uint8).reshape(nb, 2)
        cfg = np.frombuffer(f.read(nb), np.uint8)
        hi = np.frombuffer(f.read(nb * 16), np.uint8).reshape(nb, 16)
        lo = np.frombuffer(f.read(nb * 8), np.uint8).reshape(nb, 8)
        coeff = np.frombuffer(f.read(nb), np.uint8)
        gg = g.reshape(nb, BPB)
        checks = [
            ("dmin", gg[:, 0:2], dmin), ("dmax", gg[:, 2:4], dmax),
            ("config", gg[:, 4], cfg), ("levels_hi", gg[:, 5:21], hi),
            ("levels_lo", gg[:, 21:29], lo), ("coeff", gg[:, 29], coeff),
        ]
        allok = True
        for nm, a_, b_ in checks:
            same = np.array_equal(np.asarray(a_), np.asarray(b_))
            allok &= same
            extra = ""
            if not same:
                d = (np.asarray(a_).reshape(nb, -1) != np.asarray(b_).reshape(nb, -1)).any(axis=1)
                extra = "  (%d/%d blocks differ)" % (d.sum(), nb)
            print("  %-10s %s%s" % (nm, "IDENTICAL" if same else "DIFFERENT", extra))
        print("VERDICT: %s" % ("SAME quantization run" if allok else "DIFFERENT FILES -- the PCA arrays do NOT belong to this GGUF"))
        f.close()
        return 0 if allok else 3
    f.close()
    print("tensor not found in container")
    return 2


if __name__ == "__main__":
    sys.exit(main())
