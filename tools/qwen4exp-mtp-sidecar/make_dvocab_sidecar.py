"""Write a qwen4exp MTP sidecar whose output head covers only a Strata draft_vocab.bin token subset (+ d2t I64 map).

usage: make_dvocab_sidecar.py <in.gguf> <draft_vocab.bin> <out.gguf> [--red]
--red rolls d2t by one (each subset row labelled with its neighbour's id): drafts become wrong, output must not change.
"""
import sys
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'gguf-py'))
import gguf

src, dv_path, dst = sys.argv[1:4]
red = '--red' in sys.argv

ids = np.fromfile(dv_path, dtype='<i4')
r = gguf.GGUFReader(src)
out_t = next(t for t in r.tensors if t.name == 'output.weight')
n_vocab = int(out_t.shape[1])
assert ids.ndim == 1 and len(ids) > 0
assert ids.min() >= 0 and ids.max() < n_vocab, (ids.min(), ids.max(), n_vocab)
assert len(np.unique(ids)) == len(ids), 'duplicate ids in draft vocab'

w = gguf.GGUFWriter(dst, arch=r.fields['general.architecture'].contents(), endianess=r.endianess)
for f in r.fields.values():
    if f.name == gguf.Keys.General.ARCHITECTURE or f.name.startswith('GGUF.'):
        continue
    vt = f.types[0]
    st = f.types[-1] if vt == gguf.GGUFValueType.ARRAY else None
    w.add_key_value(f.name, f.contents(), vt, sub_type=st)

d2t = ids.astype(np.int64)
if red:
    d2t = np.roll(d2t, 1)

tensors = []
for t in r.tensors:
    data = t.data
    if t.name == 'output.weight':
        # quantized rows are byte-contiguous (Q6_K: 2560/256 = 10 blocks per row), so a row gather is exact
        data = np.ascontiguousarray(data[ids.astype(np.int64)])
        assert data.shape[0] == len(ids)
    tensors.append((t.name, data, t.tensor_type))
tensors.append(('d2t', d2t, gguf.GGMLQuantizationType.I64))

for name, data, tt in tensors:
    w.add_tensor_info(name, data.shape, data.dtype, data.nbytes, tt)
w.write_header_to_file()
w.write_kv_data_to_file()
w.write_ti_data_to_file()
for name, data, tt in tensors:
    w.write_tensor_data(data, tensor_endianess=r.endianess)
w.close()

# readback self-check: subset row k == source row ids[k]; d2t round-trips
r2 = gguf.GGUFReader(dst)
o2 = next(t for t in r2.tensors if t.name == 'output.weight')
m2 = next(t for t in r2.tensors if t.name == 'd2t')
for k in (0, len(ids) // 2, len(ids) - 1):
    assert np.array_equal(o2.data[k], out_t.data[ids[k]]), k
assert np.array_equal(np.asarray(m2.data).reshape(-1), d2t)
print(f'OK {dst}: {len(ids)} of {n_vocab} rows, head {o2.n_bytes/1e6:.1f} MB (was {out_t.n_bytes/1e6:.1f}), red={red}')
