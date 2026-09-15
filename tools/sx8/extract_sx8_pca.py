"""extract_sx8_pca.py -- pull the S-X8 v4.3 PCA bases/scales out of a .sx8v43 container.

The PCA correction of S-X8 v4.3 needs two per-(block-of-K) f32 arrays that live ONLY in the
container: `bases` (n_cb, 64) = [b0[0:32] | b1[32:64]] and `scales` (n_cb, 2) = [s0, s1].
The author's GGUF converter drops them, so a GGUF-only engine cannot apply the correction
(SX8_FLASH_V4_3_SPEC.md section 5).

Containers are 4-26 GB and the PCA sections are ~0.5% of that, so the default reader walks the
tensor records over HTTP RANGE requests and SKIPS each record's nb*30-byte block payload -- the
same skip the author's own indexer does (container_to_gguf_sx8_27b.py:
`f.seek(nb * 30 + 4 + ncb * 256 + ncb * 8, 1)`).

`--verify-stream` re-reads the whole container sequentially with a verbatim port of the reference
reader (sx8_container_v43.py `read_all`, which reads EVERY field and skips nothing) and asserts the
two readers produce bit-identical bases/scales. That proves the range walk without ever landing
4.38 GB on disk.

Output: an .npz with `<gguf tensor name>.pca_bases` (n_cb,64) f32 and `<name>.pca_scales` (n_cb,2) f32.
coeff is NOT extracted: it already rides in byte 29 of every GGUF S-X8 block.

Apache-2.0. Format and reference reader: MarlaLabs S-X8 v4.3.
"""

import argparse
import hashlib
import re
import struct
import sys
import time

import numpy as np
import requests

MAGIC = b"SX43FILE"
BYTES_PER_BLOCK = 30


# ---------------------------------------------------------------------------
# GGUF <- container tensor-name mapping.
# Inverse of `ct_name` in container_to_gguf_sx8_27b.py (the author's converter):
#   token_embd.weight              <- model.language_model.embed_tokens.weight
#   output.weight                  <- lm_head.weight
#   blk.N.ffn_{gate,up,down}.weight<- model.language_model.layers.N.mlp.{gate,up,down}_proj.weight
# The 4B pipeline (gguf_replace_weights_sx8.py) emits the same HF names without the
# `language_model.` segment, so both spellings are accepted.
# ---------------------------------------------------------------------------
_MLP = {"gate_proj": "ffn_gate", "up_proj": "ffn_up", "down_proj": "ffn_down"}
_LAYER_RE = re.compile(r"^model\.(?:language_model\.)?layers\.(\d+)\.mlp\.(gate_proj|up_proj|down_proj)\.weight$")


def gguf_name(container_name):
    """Container tensor name -> GGUF tensor name, or None when it is not a 1:1 S-X8 tensor."""
    if container_name in ("model.language_model.embed_tokens.weight", "model.embed_tokens.weight"):
        return "token_embd.weight"
    if container_name == "lm_head.weight":
        return "output.weight"
    m = _LAYER_RE.match(container_name)
    if m:
        return "blk.%s.%s.weight" % (m.group(1), _MLP[m.group(2)])
    return None


# ---------------------------------------------------------------------------
# HTTP range reader: a seekable, read-only file object over a URL.
# ---------------------------------------------------------------------------
class RangeReader:
    def __init__(self, url, chunk=1 << 20, session=None):
        self.url = url
        self.chunk = chunk
        self.s = session or requests.Session()
        self.pos = 0
        self._buf = b""
        self._buf_start = 0
        self.n_requests = 0
        self.n_bytes = 0
        r = self.s.head(url, allow_redirects=True, timeout=60)
        r.raise_for_status()
        if r.headers.get("Accept-Ranges") != "bytes":
            raise RuntimeError("server does not advertise byte ranges: %s" % url)
        self.size = int(r.headers["Content-Length"])

    def _fetch(self, start, length):
        end = min(start + length, self.size) - 1
        for attempt in range(5):
            try:
                r = self.s.get(self.url, headers={"Range": "bytes=%d-%d" % (start, end)},
                               allow_redirects=True, timeout=120)
                r.raise_for_status()
                if r.status_code != 206:
                    raise RuntimeError("server ignored Range (status %d)" % r.status_code)
                self.n_requests += 1
                self.n_bytes += len(r.content)
                return r.content
            except Exception:
                if attempt == 4:
                    raise
                time.sleep(1.5 * (attempt + 1))

    def read(self, n):
        out = bytearray()
        while n > 0:
            off = self.pos - self._buf_start
            if 0 <= off < len(self._buf):
                take = min(n, len(self._buf) - off)
                out += self._buf[off:off + take]
                self.pos += take
                n -= take
                continue
            want = max(n, self.chunk)
            self._buf = self._fetch(self.pos, want)
            self._buf_start = self.pos
            if not self._buf:
                break
        return bytes(out)

    def seek(self, off, whence=0):
        self.pos = off if whence == 0 else self.pos + off if whence == 1 else self.size + off

    def tell(self):
        return self.pos

    def close(self):
        self.s.close()


class StreamReader:
    """Sequential-only file object over a streaming HTTP GET. read() never seeks backwards."""

    def __init__(self, url, session=None):
        self.s = session or requests.Session()
        self.r = self.s.get(url, stream=True, allow_redirects=True, timeout=120)
        self.r.raise_for_status()
        self.size = int(self.r.headers["Content-Length"])
        self.it = self.r.iter_content(chunk_size=1 << 22)
        self._buf = b""
        self.pos = 0

    def read(self, n):
        while len(self._buf) < n:
            try:
                self._buf += next(self.it)
            except StopIteration:
                break
        out, self._buf = self._buf[:n], self._buf[n:]
        self.pos += len(out)
        return out

    def seek(self, off, whence=0):
        target = off if whence == 0 else self.pos + off
        if target < self.pos:
            raise RuntimeError("StreamReader cannot seek backwards (%d < %d)" % (target, self.pos))
        while target > self.pos:
            if not self.read(min(1 << 22, target - self.pos)):
                break

    def tell(self):
        return self.pos

    def close(self):
        self.r.close()
        self.s.close()


# ---------------------------------------------------------------------------
# Readers.
# ---------------------------------------------------------------------------
def _read_header(f):
    assert f.read(8) == MAGIC, "not a .sx8v43 container"
    f.read(1)  # VERSION
    (ml,) = struct.unpack("<I", f.read(4))
    f.read(ml)  # meta blob
    (nt,) = struct.unpack("<I", f.read(4))
    return nt


def walk_ranges(f, verbose=True):
    """Range walk: read name/shape/n_cb, SKIP the nb*30 block payload, read bases+scales."""
    nt = _read_header(f)
    out = {}
    for i in range(nt):
        (nl,) = struct.unpack("<I", f.read(4))
        name = f.read(nl).decode()
        sh = struct.unpack("<II", f.read(8))
        (n_os,) = struct.unpack("<B", f.read(1))
        f.read(4 * n_os)
        nb, ncb = struct.unpack("<II", f.read(8))
        f.seek(nb * BYTES_PER_BLOCK, 1)                       # the payload we never download
        (ncb2,) = struct.unpack("<I", f.read(4))
        data = np.frombuffer(f.read(ncb2 * 64 * 4), np.float32).reshape(ncb2, 64)
        scales = np.frombuffer(f.read(ncb2 * 2 * 4), np.float32).reshape(ncb2, 2)
        out[name] = {"shape": sh, "n_blocks": nb, "n_cb": ncb, "bases": data, "scales": scales}
        if verbose and (i < 2 or i == nt - 1):
            print("  [%d/%d] %-56s shape=%s nb=%d n_cb=%d" % (i + 1, nt, name[:56], sh, nb, ncb),
                  flush=True)
    end = f.tell()
    # A v1.1 container appends an SXT1 section (JSON config + small 1D tensors) that v1.0 readers
    # ignore -- sx8_container_v43.py `read_small_section`. Consume it so the EOF check is exact.
    end = _skip_small_section(f, end)
    return out, end


SMALL_MAGIC = b"SXT1"


def _skip_small_section(f, end):
    """Consume the optional trailing SXT1 section; return the new end offset."""
    f.seek(end)
    if f.read(4) != SMALL_MAGIC:
        return end
    (cl,) = struct.unpack("<I", f.read(4))
    f.read(cl)
    (n,) = struct.unpack("<I", f.read(4))
    for _ in range(n):
        (nl,) = struct.unpack("<I", f.read(4))
        f.read(nl)
        (nd,) = struct.unpack("<B", f.read(1))
        shape = struct.unpack("<%dI" % nd, f.read(4 * nd))
        (dt,) = struct.unpack("<B", f.read(1))
        f.seek(int(np.prod(shape)) * (2 if dt == 0 else 4), 1)
    print("  SXT1 section: %d small tensors, %d bytes" % (n, f.tell() - end), flush=True)
    return f.tell()


def read_all_sequential(f, verbose=True):
    """Verbatim port of sx8_container_v43.py `read_all` (reads EVERY field, skips nothing),
    taking an already-open file object instead of a path. Returns the same PCA dict shape."""
    nt = _read_header(f)
    out = {}
    for i in range(nt):
        (nl,) = struct.unpack("<I", f.read(4))
        name = f.read(nl).decode()
        sh = struct.unpack("<II", f.read(8))
        (n_os,) = struct.unpack("<B", f.read(1))
        struct.unpack("<%dI" % n_os, f.read(4 * n_os))
        nb, ncb = struct.unpack("<II", f.read(8))
        np.frombuffer(f.read(nb * 2), np.float16)    # dmin
        np.frombuffer(f.read(nb * 2), np.float16)    # dmax
        np.frombuffer(f.read(nb), np.uint8)          # config
        np.frombuffer(f.read(nb * 16), np.uint8)     # levels_hi
        np.frombuffer(f.read(nb * 8), np.uint8)      # levels_lo
        np.frombuffer(f.read(nb), np.uint8)          # coeff
        (ncb2,) = struct.unpack("<I", f.read(4))
        data = np.frombuffer(f.read(ncb2 * 64 * 4), np.float32).reshape(ncb2, 64)
        scales = np.frombuffer(f.read(ncb2 * 2 * 4), np.float32).reshape(ncb2, 2)
        out[name] = {"shape": sh, "n_blocks": nb, "n_cb": ncb, "bases": data, "scales": scales}
        if verbose and (i % 100 == 0):
            print("  stream %d/%d  %.2f GB" % (i + 1, nt, f.tell() / 1e9), flush=True)
    return out, _skip_small_section(f, f.tell())


def _sig(rec):
    h = hashlib.sha256()
    h.update(np.ascontiguousarray(rec["bases"]).tobytes())
    h.update(np.ascontiguousarray(rec["scales"]).tobytes())
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", required=True)
    ap.add_argument("--out", required=True, help="output .npz")
    ap.add_argument("--verify-stream", action="store_true",
                    help="re-read the whole container sequentially with the reference reader "
                         "and assert bit-identical bases/scales (no disk used)")
    a = ap.parse_args()

    t0 = time.time()
    f = RangeReader(a.url)
    print("container: %s (%.2f GB)" % (a.url, f.size / 1e9), flush=True)
    recs, end = walk_ranges(f)
    print("range walk: %d tensors | %d HTTP requests | %.1f MB downloaded | walk ended at %d, "
          "file size %d | %s" % (len(recs), f.n_requests, f.n_bytes / 1e6, end, f.size,
                                 "EXACT EOF" if end == f.size else "MISMATCH"), flush=True)
    if end != f.size:
        # A wrong record length desynchronises the walk; landing exactly on EOF after n_tensors
        # records is a whole-file structural check of every length we computed.
        print("FAIL: walk did not consume the file exactly", flush=True)
        return 2
    f.close()

    if a.verify_stream:
        print("verify-stream: re-reading %.2f GB sequentially with the reference reader..."
              % (f.size / 1e9), flush=True)
        g = StreamReader(a.url)
        ref, end2 = read_all_sequential(g)
        g.close()
        assert end2 == f.size, "reference reader ended at %d, size %d" % (end2, f.size)
        assert set(ref) == set(recs), "tensor name sets differ"
        bad = 0
        for n in ref:
            for k in ("bases", "scales"):
                if not np.array_equal(np.asarray(ref[n][k]), np.asarray(recs[n][k])):
                    print("  MISMATCH %s.%s" % (n, k))
                    bad += 1
            for k in ("shape", "n_blocks", "n_cb"):
                assert ref[n][k] == recs[n][k], "%s.%s" % (n, k)
        print("verify-stream: %d tensors | %s" % (len(ref), "BIT-IDENTICAL" if not bad else "%d DIFFS" % bad),
              flush=True)
        if bad:
            return 3

    save = {}
    mapped = unmapped = 0
    for cname, rec in sorted(recs.items()):
        gn = gguf_name(cname)
        if gn is None:
            unmapped += 1
            continue
        save[gn + ".pca_bases"] = np.ascontiguousarray(rec["bases"], dtype=np.float32)
        save[gn + ".pca_scales"] = np.ascontiguousarray(rec["scales"], dtype=np.float32)
        mapped += 1
    np.savez(a.out, **save)
    nbytes = sum(v.nbytes for v in save.values())
    print("mapped %d container tensors to GGUF names (%d unmapped) | %.1f MB of f32 | -> %s | %.0fs"
          % (mapped, unmapped, nbytes / 1e6, a.out, time.time() - t0), flush=True)
    for gn in sorted(save)[:2]:
        print("  %-44s %s  sha256=%s" % (gn, save[gn].shape, hashlib.sha256(save[gn].tobytes()).hexdigest()[:16]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
