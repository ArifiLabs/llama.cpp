"""add_sx8_pca_to_gguf.py -- attach the S-X8 v4.3 PCA correction data to a GGUF.

Input: a GGUF holding GGML_TYPE_SX8 (57) weights, plus the .npz produced by extract_sx8_pca.py.
Output (default `companion` mode): a small `<model>.sx8pca.gguf` holding only the PCA tensors.
`inplace` mode instead copies the whole model GGUF and adds the same tensors to it.

Per S-X8 weight `W` (ne = [K, N], n_cb = K/32) four tensors are emitted:

  <W>.sx8_pca_b0   f32  ne=[32, n_cb]   s0[kb] * b0[kb][0:32]    (the scale is FOLDED IN)
  <W>.sx8_pca_b1   f32  ne=[32, n_cb]   s1[kb] * b1[kb][32:64]
  <W>.sx8_pca_c0   f16  ne=[n_cb, N]    signed4(coeff & 0xF)
  <W>.sx8_pca_c1   f16  ne=[n_cb, N]    signed4(coeff >> 4)

Folding s into b is exact -- Z0(kb) = s0(kb) * sum_t X * b0 (SX8_FLASH_V4_3_SPEC.md section 5) -- and
saves a broadcast multiply per matmul. The unfolded bases and scales stay in the .npz.

c0/c1 come from byte 29 of each 30-byte block, which is ALREADY in the GGUF: the container walk never
downloads it. Nibble order and sign follow sx8_decode1_v3.cu:120-121
(`c0 = coeff & 0xF; if (c0 >= 8) c0 -= 16;` and `c1 = (coeff >> 4) & 0xF; ...`).

GGUF block layout for S-X8: row-major (N, n_cb) blocks of 30 bytes, coeff at byte 29.

`companion` mode also stamps the source model's identity into the companion --
`sx8pca.source.name`, `sx8pca.source.tensor`, `sx8pca.source.head` (hex of the first 1024 raw
bytes of that tensor) -- which llama_model_loader compares against the model it is attaching to,
and refuses on mismatch. Without it a companion from a different quantization of the same
architecture loads silently and corrupts every S-X8 matmul.

Apache-2.0. Format: MarlaLabs S-X8 v4.3.
"""

import argparse
import hashlib
import os
import sys
import time

import numpy as np

SX8 = 57
BYTES_PER_BLOCK = 30
COEFF_OFF = 29


def signed4(x):
    """signed4(x) = x < 8 ? x : x - 16  -- SX8_FLASH_V4_3_SPEC.md section 2."""
    return np.where(x < 8, x, x.astype(np.int16) - 16).astype(np.int16)


def pca_tensors_for(name, raw, ne0, ne1, npz):
    """Build the four PCA arrays for one S-X8 weight. `raw` is its uint8 block payload."""
    n_cb = ne0 // 32
    n_rows = ne1
    assert raw.size == n_rows * n_cb * BYTES_PER_BLOCK, (
        "%s: %d bytes, expected %d" % (name, raw.size, n_rows * n_cb * BYTES_PER_BLOCK))

    bases = npz[name + ".pca_bases"]     # (n_cb, 64) f32
    scales = npz[name + ".pca_scales"]   # (n_cb, 2)  f32
    assert bases.shape == (n_cb, 64), "%s: bases %s, n_cb=%d" % (name, bases.shape, n_cb)
    assert scales.shape == (n_cb, 2)

    b0 = np.ascontiguousarray(bases[:, 0:32] * scales[:, 0:1], dtype=np.float32)   # (n_cb, 32)
    b1 = np.ascontiguousarray(bases[:, 32:64] * scales[:, 1:2], dtype=np.float32)  # (n_cb, 32)

    coeff = raw.reshape(n_rows, n_cb, BYTES_PER_BLOCK)[:, :, COEFF_OFF]            # (N, n_cb)
    c0 = signed4(coeff & 0x0F).astype(np.float16)
    c1 = signed4(coeff >> 4).astype(np.float16)
    # The engine creates these with LLM_TN(tensor, "sx8_pca_c0", bid), which spells the parent
    # WITHOUT its ".weight" suffix -- "blk.0.ffn_down.sx8_pca_c0" (same shape as the fork's
    # escha_aux sidecars). Strip ".weight" so the names line up.
    stem = name[:-len(".weight")] if name.endswith(".weight") else name
    return {
        stem + ".sx8_pca_b0": b0,
        stem + ".sx8_pca_b1": b1,
        stem + ".sx8_pca_c0": np.ascontiguousarray(c0),
        stem + ".sx8_pca_c1": np.ascontiguousarray(c1),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gguf", required=True, help="source GGUF with S-X8 (type 57) weights")
    ap.add_argument("--npz", required=True, help="output of extract_sx8_pca.py")
    ap.add_argument("--out", required=True)
    ap.add_argument("--mode", choices=("companion", "inplace"), default="companion",
                    help="companion: PCA tensors only (small). inplace: full model copy + PCA tensors.")
    ap.add_argument("--gguf-py", default=None, help="path to the fork's gguf-py (for type 57)")
    a = ap.parse_args()

    if a.gguf_py:
        sys.path.insert(0, a.gguf_py)
    from gguf import GGUFWriter, GGMLQuantizationType, GGUFValueType
    from gguf.gguf_reader import GGUFReader

    t0 = time.time()
    npz = np.load(a.npz)
    r = GGUFReader(a.gguf)
    sx8 = [t for t in r.tensors if int(t.tensor_type) == SX8]
    print("source: %s | %d tensors, %d S-X8" % (a.gguf, len(r.tensors), len(sx8)), flush=True)

    missing = [t.name for t in sx8 if (t.name + ".pca_bases") not in npz.files]
    if missing:
        print("FAIL: %d S-X8 tensors have no PCA record in the npz, e.g. %s"
              % (len(missing), missing[:3]))
        return 2

    if os.path.exists(a.out):
        os.remove(a.out)
    w = GGUFWriter(a.out, "qwen35" if a.mode == "inplace" else "sx8pca")

    if a.mode == "companion":
        # Identity stamp (CHECK-R53-FABLE finding 2). The loader matches the companion's tensors
        # by name and shape alone, so a companion built from a DIFFERENT quantization of the same
        # architecture would attach silently and pair these b0/b1 with foreign c0/c1. Stamp the
        # source model's identity so llama_model_loader can refuse that companion.
        #
        # `head` is the RAW first bytes of the first S-X8 tensor's block payload, hex-encoded:
        # the loader compares the bytes themselves (libllama does not link a sha256), which is
        # strictly stronger than comparing a hash of the same bytes.
        gn = r.fields.get("general.name")
        src_name = bytes(gn.parts[-1]).decode() if gn is not None else os.path.basename(a.gguf)
        head = np.asarray(sx8[0].data).view(np.uint8).reshape(-1)[:1024].tobytes()
        w.add_string("sx8pca.source.name", src_name)
        w.add_string("sx8pca.source.tensor", sx8[0].name)
        w.add_string("sx8pca.source.head", head.hex())
        print("identity: source=%r tensor=%s head=%d bytes (%s...)"
              % (src_name, sx8[0].name, len(head), head.hex()[:32]), flush=True)

    if a.mode == "inplace":
        # Copy every KV of the source verbatim (same method as the author's converter:
        # f.parts, never f.data).
        for name, f in sorted(r.fields.items()):
            t = f.types[0]
            if name.startswith("GGUF.") or name == "general.architecture":
                continue
            if t == GGUFValueType.STRING:
                w.add_string(name, bytes(f.parts[-1]).decode())
            elif t == GGUFValueType.UINT32:
                w.add_uint32(name, int(np.asarray(f.parts[-1]).reshape(-1)[0]))
            elif t == GGUFValueType.INT32:
                w.add_int32(name, int(np.asarray(f.parts[-1]).reshape(-1)[0]))
            elif t == GGUFValueType.FLOAT32:
                w.add_float32(name, float(np.asarray(f.parts[-1]).reshape(-1)[0]))
            elif t == GGUFValueType.BOOL:
                w.add_bool(name, bool(np.asarray(f.parts[-1]).reshape(-1)[0]))
            elif t == GGUFValueType.ARRAY:
                arr_t = f.types[1]
                if arr_t == GGUFValueType.STRING:
                    w.add_array(name, [bytes(f.parts[i]).decode(errors="replace") for i in f.data])
                elif arr_t in (GGUFValueType.UINT32, GGUFValueType.INT32):
                    w.add_array(name, [int(np.asarray(p).reshape(-1)[0]) for p in f.parts[5:]])
                else:
                    w.add_array(name, [float(np.asarray(p).reshape(-1)[0]) for p in f.parts[5:]])
            else:
                print("  [skip kv] %s type %s" % (name, t))
        for ti in r.tensors:
            arr = ti.data
            w.add_tensor(ti.name, np.ascontiguousarray(arr),
                         raw_dtype=GGMLQuantizationType(int(ti.tensor_type)))

    n_pca = 0
    nbytes = 0
    digest = hashlib.sha256()
    for ti in sx8:
        ne0, ne1 = int(ti.shape[0]), int(ti.shape[1])
        raw = np.asarray(ti.data).view(np.uint8).reshape(-1)
        for tn, arr in pca_tensors_for(ti.name, raw, ne0, ne1, npz).items():
            w.add_tensor(tn, arr)
            nbytes += arr.nbytes
            n_pca += 1
            digest.update(tn.encode())
            digest.update(arr.tobytes()[:4096])
        del raw

    w.write_header_to_file()
    w.write_kv_data_to_file()
    w.write_tensors_to_file()
    w.close()
    print("wrote %s | mode=%s | %d PCA tensors (%d weights) | %.1f MB of PCA data | %.0f s"
          % (a.out, a.mode, n_pca, len(sx8), nbytes / 1e6, time.time() - t0), flush=True)
    print("pca-digest(sha256 of names + first 4 KB of each): %s" % digest.hexdigest(), flush=True)
    print("file size: %d bytes" % os.path.getsize(a.out), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
