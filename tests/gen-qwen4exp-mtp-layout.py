#!/usr/bin/env python3
# Writes the qwen4exp MTP layout files that test-qwen4exp-mtp-layout compares, from a random trunk written by
# `test-llama-archs -a qwen4exp -o <dir>` (qwen4exp-moe.gguf, 2 layers, layer 1 = attention).
#   ours-sidecar.gguf   : our sidecar - no trunk, MTP block at blk.2, output_hc_* = the MTP block's own mixer M
#   ours-wrongmix.gguf  : the same sidecar with output_hc_* = the trunk mixer T (control: logits must differ)
#   up-full.gguf        : upstream #29761 layout - trunk + output_hc_* = T + MTP block + blk.2.nextn.hc_head_* = M
#                         + the block's QSA indexer tensors (our dense MTP drops them)
#   up-mtponly.gguf     : upstream mtp_only layout - no trunk, no output_hc_*, no PLE keys/table, nextn.hc_head_* = M
# usage: gen-qwen4exp-mtp-layout.py <qwen4exp-moe.gguf> <out dir>
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "gguf-py"))
from gguf import GGUFReader, GGUFWriter, GGUFValueType  # noqa: E402

src, out = sys.argv[1], sys.argv[2]
r = GGUFReader(src)
A = "qwen4exp"
n_layer = int(r.fields[f"{A}.block_count"].contents())
mtp = n_layer  # MTP block index
rng = np.random.default_rng(29761)


def rand(ne):  # ggml ne order -> numpy (reversed) shape
    return (rng.standard_normal(tuple(reversed(ne))) * 0.1).astype(np.float32)


tensors = {t.name: np.array(t.data) for t in r.tensors}
shape = {t.name: [int(x) for x in t.shape] for t in r.tensors}

n_embd = shape["token_embd.weight"][0]
hc_dim = shape["output_hc_down.weight"][0]
T = {k: tensors[f"output_hc_{k}.weight"] for k in ("norm", "down", "up")}
M = {k: rand(shape[f"output_hc_{k}.weight"]) for k in ("norm", "down", "up")}

# the MTP block (ours): layer 1's attention block shape, fresh weights, plus the nextn inputs
block = {}
for name, ne in shape.items():
    if name.startswith(f"blk.{n_layer - 1}.") and ".indexer." not in name:
        block[f"blk.{mtp}." + name.split(".", 2)[2]] = rand(ne)
block[f"blk.{mtp}.nextn.eh_proj.weight"] = rand([2 * n_embd, n_embd])
block[f"blk.{mtp}.nextn.enorm.weight"] = rand([n_embd])
block[f"blk.{mtp}.nextn.hnorm.weight"] = rand([hc_dim])  # upstream's converter writes it flat too
indexer = {f"blk.{mtp}." + n.split(".", 2)[2]: rand(ne) for n, ne in shape.items()
           if n.startswith(f"blk.{n_layer - 1}.indexer.")}


def write(path, tens, drop_ple=False):
    w = GGUFWriter(path, A)
    for k, f in r.fields.items():
        if k.startswith("GGUF.") or k == "general.architecture":
            continue
        if drop_ple and k.startswith(f"{A}.ple.") or (drop_ple and k == f"{A}.embedding_length_per_layer_input"):
            continue
        vt = f.types[0]
        val = f.contents()
        if vt == GGUFValueType.ARRAY and len(f.types) < 2:
            continue  # empty array: gguf-py refuses to write one, and absent reads as empty
        sub = f.types[1] if vt == GGUFValueType.ARRAY else None
        if k == f"{A}.block_count":
            val = n_layer + 1
        elif k == f"{A}.nextn_predict_layers":
            val = 1
        elif vt == GGUFValueType.ARRAY and k.startswith(f"{A}.") and len(val) == n_layer:
            val = list(val) + [0 if k.endswith("recurrent_layers") else val[-1]]  # MTP block = attention
        w.add_key_value(k, val, vt, sub_type=sub)
    for n, a in tens.items():
        w.add_tensor(n, a)
    w.write_header_to_file()
    w.write_kv_data_to_file()
    w.write_tensors_to_file()
    w.close()
    print("wrote", path, len(tens), "tensors")


head = {"token_embd.weight": tensors["token_embd.weight"], "output.weight": tensors["output.weight"]}
ple = {"per_layer_token_embd.weight": tensors["per_layer_token_embd.weight"]}
mix = lambda m: {f"output_hc_{k}.weight": m[k] for k in m}  # noqa: E731
nextn_mix = {f"blk.{mtp}.nextn.hc_head_{k}.weight": M[k] for k in M}

write(os.path.join(out, "ours-sidecar.gguf"), {**head, **ple, **mix(M), **block})
write(os.path.join(out, "ours-wrongmix.gguf"), {**head, **ple, **mix(T), **block})
write(os.path.join(out, "up-full.gguf"), {**tensors, **block, **nextn_mix, **indexer})
write(os.path.join(out, "up-mtponly.gguf"), {**head, **block, **nextn_mix, **indexer}, drop_ple=True)
