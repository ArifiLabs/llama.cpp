"""Build the qwen4exp MTP sidecar GGUF (loaded with -md <sidecar> --spec-type draft-mtp). See README.md here.

Sources (all read-only):
  --main   ISTA-DASLab Qwen3.8-Flash-Next-GSQ-RCO IQ3_S shard 00001  (hparams, tokenizer, token_embd, output)
  --head   ashbash Qwen3.8-Flash-Next-mtp-drafter-Q8_0.gguf           (the MTP block, Q8_0 + F32)
  --bf16   mtp_fetch.py fetch --out dir                               (SHA-pinned BF16 tensors, the verify reference)

  python build_sidecar.py --main M --head H --bf16 B --verify-only   # dequant compare head vs BF16, no write
  python build_sidecar.py --main M --head H --bf16 B --out <file>    # verify, then write
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "gguf-py"))
import gguf  # noqa: E402
from gguf.quants import dequantize  # noqa: E402

MAIN = HEAD = ""
BF16 = Path(".")
# written into the sidecar as a constant (not the local path), so the output sha256 does not depend on where files live
HEAD_SOURCE = "ashbash/Qwen3.8-Flash-Next-MTP-Drafter-GGUF@c82053e8db1eff3f5fcc219c3146fc2ed34258d6/Qwen3.8-Flash-Next-mtp-drafter-Q8_0.gguf"
L = 48  # the MTP block index = n_layer of the trunk

# ashbash name -> (sidecar name, bf16 tensor, bf16 slice key, norm (+1 expected))
MAP: dict[str, tuple] = {
    "mtp.hyper_connection_mixer.norm.weight": ("output_hc_norm.weight", "mtp.hyper_connection_mixer.hc_norm.weight", None, True),
    "mtp.hyper_connection_mixer.down.weight": ("output_hc_down.weight", "mtp.hyper_connection_mixer.input_mix_weight_down.weight", None, False),
    "mtp.hyper_connection_mixer.up.weight":   ("output_hc_up.weight",   "mtp.hyper_connection_mixer.input_mix_weight_up.weight", None, False),
}
for part, hf in (("hc_attn", "attn_hyper_connection"), ("hc_ffn", "mlp_hyper_connection")):
    MAP[f"mtp.layers.0.{part}.inject.weight"] = (f"blk.{L}.{part}_inject.weight", f"mtp.layers.0.{hf}.block_inject_weight.weight", None, False)
    MAP[f"mtp.layers.0.{part}.norm.weight"]   = (f"blk.{L}.{part}_norm.weight",   f"mtp.layers.0.{hf}.hc_norm.weight", None, True)
    MAP[f"mtp.layers.0.{part}.down.weight"]   = (f"blk.{L}.{part}_down.weight",   f"mtp.layers.0.{hf}.input_mix_weight_down.weight", None, False)
    MAP[f"mtp.layers.0.{part}.up.weight"]     = (f"blk.{L}.{part}_up.weight",     f"mtp.layers.0.{hf}.input_mix_weight_up.weight", None, False)
MAP.update({
    "mtp.layers.0.mlp.switch_mlp.gate_proj.weight": (f"blk.{L}.ffn_gate_exps.weight", "mtp.layers.0.mlp.experts.gate_up_proj", "gate", False),
    "mtp.layers.0.mlp.switch_mlp.up_proj.weight":   (f"blk.{L}.ffn_up_exps.weight",   "mtp.layers.0.mlp.experts.gate_up_proj", "up", False),
    "mtp.layers.0.mlp.switch_mlp.down_proj.weight": (f"blk.{L}.ffn_down_exps.weight", "mtp.layers.0.mlp.experts.down_proj", "exp", False),
    "mtp.layers.0.mlp.gate.weight":                 (f"blk.{L}.ffn_gate_inp.weight",  "mtp.layers.0.mlp.gate.weight", None, False),
    "mtp.layers.0.mlp.shared_expert_gate.weight":   (f"blk.{L}.ffn_gate_inp_shexp.weight", "mtp.layers.0.mlp.shared_expert_gate.weight", None, False),
    "mtp.layers.0.mlp.shared_expert.gate_proj.weight": (f"blk.{L}.ffn_gate_shexp.weight", "mtp.layers.0.mlp.shared_expert.gate_proj.weight", None, False),
    "mtp.layers.0.mlp.shared_expert.up_proj.weight":   (f"blk.{L}.ffn_up_shexp.weight",   "mtp.layers.0.mlp.shared_expert.up_proj.weight", None, False),
    "mtp.layers.0.mlp.shared_expert.down_proj.weight": (f"blk.{L}.ffn_down_shexp.weight", "mtp.layers.0.mlp.shared_expert.down_proj.weight", None, False),
    "mtp.layers.0.self_attn.q_proj.weight": (f"blk.{L}.attn_q.weight",      "mtp.layers.0.self_attn.q_proj.weight", None, False),
    "mtp.layers.0.self_attn.k_proj.weight": (f"blk.{L}.attn_k.weight",      "mtp.layers.0.self_attn.k_proj.weight", None, False),
    "mtp.layers.0.self_attn.v_proj.weight": (f"blk.{L}.attn_v.weight",      "mtp.layers.0.self_attn.v_proj.weight", None, False),
    "mtp.layers.0.self_attn.o_proj.weight": (f"blk.{L}.attn_output.weight", "mtp.layers.0.self_attn.o_proj.weight", None, False),
    "mtp.layers.0.self_attn.q_norm.weight": (f"blk.{L}.attn_q_norm.weight", "mtp.layers.0.self_attn.q_norm.weight", None, True),
    "mtp.layers.0.self_attn.k_norm.weight": (f"blk.{L}.attn_k_norm.weight", "mtp.layers.0.self_attn.k_norm.weight", None, True),
    "mtp.pre_fc_norm_embedding.weight": (f"blk.{L}.nextn.enorm.weight", "mtp.pre_fc_norm_embedding.weight", None, True),
    "mtp.pre_fc_norm_hidden.weight":    (f"blk.{L}.nextn.hnorm.weight", "mtp.pre_fc_norm_hidden.weight", None, True),
    "mtp.fc_embedding.weight": (None, "mtp.fc_embedding.weight", None, False),   # fused into eh_proj
    "mtp.fc_hidden.weight":    (None, "mtp.fc_hidden.weight", None, False),
})
SKIP = ("mtp.layers.0.self_attn.indexer.",)   # Strata runs the MTP block with dense attention (mtp.cpp:215)
EXPERTS_CHECKED = (0, 257, 511)
DROP_KV = ("split.", "qwen4exp.ple.", "qwen4exp.embedding_length_per_layer_input", "GGUF.", "general.architecture")


def bf16_load(name: str, shape: tuple, index=None) -> np.ndarray:
    """BF16 .bin -> f32; index = leading-axis row (one expert) to keep RAM bounded."""
    path = BF16 / "tensors" / f"{name}.bin"
    row = int(np.prod(shape[1:])) if index is not None else int(np.prod(shape))
    raw = np.fromfile(path, dtype=np.uint16, count=row, offset=(index or 0) * row * 2)
    out = (raw.astype(np.uint32) << 16).view(np.float32)
    return out.reshape(shape[1:] if index is not None else shape)


def deq(t, index=None) -> np.ndarray:
    data = np.asarray(t.data)
    if index is not None:
        data = data[index]
    if t.tensor_type in (gguf.GGMLQuantizationType.F32, gguf.GGMLQuantizationType.F16):
        return np.asarray(data, dtype=np.float32)
    return dequantize(data, t.tensor_type)


def rel(a: np.ndarray, b: np.ndarray) -> float:
    return float(np.linalg.norm((a - b).ravel()) / max(np.linalg.norm(b.ravel()), 1e-30))


def verify(head: dict, shapes: dict) -> list[str]:
    """Each ashbash tensor vs the BF16 truth, in the ggml layout the sidecar writes (numpy shape = reversed ne)."""
    bad = []
    for an, (sn, hf, sl, is_norm) in MAP.items():
        t = head[an]
        shp = tuple(shapes[hf])
        if sl in ("gate", "up", "exp"):
            errs = []
            for e in EXPERTS_CHECKED:
                ref = bf16_load(hf, shp, e)
                if sl == "gate":
                    ref = ref[: ref.shape[0] // 2]
                elif sl == "up":
                    ref = ref[ref.shape[0] // 2:]
                errs.append(rel(deq(t, e).reshape(ref.shape), ref))
            err = max(errs)
        else:
            ref = bf16_load(hf, shp)
            got = deq(t).reshape(ref.shape) if deq(t).size == ref.size else None
            if got is None:
                bad.append(f"{an}: size {deq(t).size} != {ref.size}")
                continue
            if is_norm:
                e_plus, e_raw = rel(got, ref + 1.0), rel(got, ref)
                err = e_plus
                if e_plus > 1e-3:
                    bad.append(f"{an}: norm not (1+w): rel vs 1+w {e_plus:.2e}, vs w {e_raw:.2e}")
            else:
                err = rel(got, ref)
        limit = 1e-3 if t.tensor_type == gguf.GGMLQuantizationType.F32 else 1.5e-2
        flag = "OK " if err <= limit else "BAD"
        if err > limit:
            bad.append(f"{an}: rel err {err:.3e} > {limit}")
        print(f"{flag} {an:58s} {t.tensor_type.name:5s} rel_err={err:.3e}")
    return bad


def write(out: str, main: gguf.GGUFReader, head: dict) -> None:
    w = gguf.GGUFWriter(out, "qwen4exp")
    for k, f in main.fields.items():
        if k.startswith(DROP_KV):
            continue
        vt = f.types[0]
        val = f.contents()
        if k == "qwen4exp.block_count":
            val = L + 1
        if k == "qwen4exp.attention.compress_ratios":
            val = list(val) + [0]          # one entry per layer incl. the MTP block (dense: 0)
        sub = f.types[-1] if vt == gguf.GGUFValueType.ARRAY else None
        w.add_key_value(k, val, vt, sub_type=sub)
    w.add_uint32("qwen4exp.nextn_predict_layers", 1)
    w.add_string("general.name", "Qwen3.8-Flash-Next MTP sidecar (lane-298)")
    w.add_string("arifi.mtp_sidecar.head_source", HEAD_SOURCE)
    w.add_string("arifi.mtp_sidecar.note", "mtp_only: trunk absent; output_hc_* = the MTP hyper_connection_mixer")

    mt = {t.name: t for t in main.tensors}
    for n in ("token_embd.weight", "output.weight"):
        t = mt[n]
        w.add_tensor(n, np.asarray(t.data), raw_dtype=t.tensor_type)

    fe, fh = head["mtp.fc_embedding.weight"], head["mtp.fc_hidden.weight"]
    assert fe.tensor_type == fh.tensor_type == gguf.GGMLQuantizationType.Q8_0
    # each row: [fc_embedding | fc_hidden] along ne0, so eh_proj @ concat(e_norm, h_norm) = fe@e + fh@h
    w.add_tensor(f"blk.{L}.nextn.eh_proj.weight",
                 np.concatenate([np.asarray(fe.data), np.asarray(fh.data)], axis=-1), raw_dtype=fe.tensor_type)

    for an, (sn, _, _, _) in MAP.items():
        if sn is None:
            continue
        t = head[an]
        data = np.asarray(t.data)
        if sn.endswith("ffn_gate_inp_shexp.weight"):
            data = data.reshape(-1)           # [2560,1] -> {n_embd}
        if t.tensor_type == gguf.GGMLQuantizationType.F32:
            w.add_tensor(sn, data.astype(np.float32))
        else:
            w.add_tensor(sn, data, raw_dtype=t.tensor_type)

    w.write_header_to_file()
    w.write_kv_data_to_file()
    w.write_tensors_to_file(progress=False)
    w.close()


def check_written(out: str) -> None:
    r = gguf.GGUFReader(out)
    t = {x.name: x for x in r.tensors}
    eh = t[f"blk.{L}.nextn.eh_proj.weight"]
    assert [int(x) for x in eh.shape] == [5120, 2560], eh.shape
    full = dequantize(np.asarray(eh.data), eh.tensor_type)          # (2560 rows, 5120)
    head = {x.name: x for x in gguf.GGUFReader(HEAD).tensors}
    assert np.array_equal(full[:, :2560], deq(head["mtp.fc_embedding.weight"])), "eh_proj first half != fc_embedding"
    assert np.array_equal(full[:, 2560:], deq(head["mtp.fc_hidden.weight"])), "eh_proj second half != fc_hidden"
    kv = {k: f for k, f in r.fields.items()}
    assert kv["qwen4exp.block_count"].contents() == L + 1
    assert kv["qwen4exp.nextn_predict_layers"].contents() == 1
    assert len(kv["qwen4exp.attention.compress_ratios"].contents()) == L + 1
    assert not any(k.startswith("qwen4exp.ple.") for k in kv)
    print(f"written OK: {len(r.tensors)} tensors, eh_proj halves exact, kv block_count={L + 1}")


def main() -> int:
    global MAIN, HEAD, BF16
    ap = argparse.ArgumentParser()
    ap.add_argument("--main", required=True, help="GSQ-RCO IQ3_S shard 00001 gguf")
    ap.add_argument("--head", required=True, help="ashbash mtp-drafter Q8_0 gguf")
    ap.add_argument("--bf16", required=True, help="mtp_fetch.py --out dir (tensors/ + mtp-inventory.json)")
    ap.add_argument("--out")
    ap.add_argument("--verify-only", action="store_true")
    a = ap.parse_args()
    if not a.verify_only and not a.out:
        ap.error("--out is required unless --verify-only")
    MAIN, HEAD, BF16 = a.main, a.head, Path(a.bf16)
    inv = json.load(open(BF16 / "mtp-inventory.json"))
    rows = inv["tensors"] if isinstance(inv, dict) else inv
    shapes = {r["name"]: r["shape"] for r in rows}
    hr = gguf.GGUFReader(HEAD)
    head = {t.name: t for t in hr.tensors}
    unmapped = [n for n in head if n not in MAP and not n.startswith(SKIP)]
    if unmapped:
        print("UNMAPPED:", unmapped)
        return 2
    bad = verify(head, shapes)
    if bad:
        print("VERIFY FAILED:\n  " + "\n  ".join(bad))
        return 1
    print("VERIFY PASS")
    if a.verify_only:
        return 0
    write(a.out, gguf.GGUFReader(MAIN), head)
    check_written(a.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
