"""lane-296c stage 8 reader: paired 64-chunk deltas and 16-chunk values from night/ev15s8 (+ ev14 A, B5).
usage: python night/s8_read.py"""
import math
import re
from pathlib import Path

from p64_dist import per_chunk

N = Path(__file__).parent
LEGS = {"A": "ev14/a64.txt", "B5": "ev14/b564.txt", "C(A+ssm_out f32acc)": "ev15s8/c64.txt",
        "D(cand: B5+f32acc all)": "ev15s8/d64.txt", "r86i": "ev15s8/r64.txt"}
PAIRS = [("C(A+ssm_out f32acc)", "A"), ("B5", "C(A+ssm_out f32acc)"), ("D(cand: B5+f32acc all)", "A"),
         ("D(cand: B5+f32acc all)", "r86i"), ("D(cand: B5+f32acc all)", "B5"), ("A", "r86i")]

data = {}
for k, p in LEGS.items():
    try:
        data[k] = per_chunk(p)
    except (FileNotFoundError, ValueError):
        pass
for k, (l, final) in data.items():
    run16 = math.exp(sum(l[:16]) / 16) if len(l) >= 16 else float("nan")
    print(f"{k:24s} chunks={len(l):2d} final {final:.4f} chunk-16 {run16:.4f}")
print("| pair | dPPL | paired SE | dPPL/SE | first worse |")
for a, b in PAIRS:
    if a not in data or b not in data or len(data[a][0]) != len(data[b][0]):
        print(f"| {a} - {b} | missing |")
        continue
    la, pa = data[a]
    lb, pb = data[b]
    d = [x - y for x, y in zip(la, lb)]
    n = len(d)
    m = sum(d) / n
    se = math.sqrt(sum((x - m) ** 2 for x in d) / (n - 1) / n) * pb
    print(f"| {a} - {b} | {pa - pb:+.4f} | {se:.4f} | {(pa - pb) / se:+.1f} | {sum(1 for x in d if x > 0)}/{n} |")
# x16 paired over the same 16 chunks (KL-leg rows) against the 16-chunk legs; pre-registered: x16 - off <= -2 SE = exact helps
import contextlib  # noqa: E402
import io  # noqa: E402

with contextlib.redirect_stdout(io.StringIO()):  # paired_noise prints its own table at import
    import paired_noise as pn  # noqa: E402

x = pn.chunks("ev15s8/x16.txt")
for name, p in (("off", "ev6/sx8-off.txt"), ("A", "ev10/a-sx8.txt"), ("B5", "ev13/b5-sx8.txt")):
    o = pn.chunks(p)
    if not x or not o:
        print(f"| x16 - {name} (16ch) | missing |")
        continue
    d = [a - b for a, b in zip(x[0], o[0])]
    m = sum(d) / 16
    se = math.sqrt(sum((v - m) ** 2 for v in d) / 15 / 16) * o[1]
    print(f"| x16 - {name} (16ch) | {x[1] - o[1]:+.4f} | {se:.4f} | {(x[1] - o[1]) / se:+.1f} | {sum(1 for v in d if v > 0)}/16 |")
for leg in ("x16", "d-q4", "d-gsq"):
    f = N / "ev15s8" / f"{leg}.txt"
    if not f.exists():
        print(f"{leg}: missing")
        continue
    t = f.read_text(errors="replace") + (N / "ev15s8" / f"{leg}.err").read_text(errors="replace")
    v = {k: re.search(rf"^{k}\s*:\s*([0-9.]+)", t, re.M) for k in ("Mean PPL\\(Q\\)", "Mean\\s+KLD", "Median\\s+KLD")}
    print(leg, {k.replace("\\", "").replace("s+", " "): (m.group(1) if m else None) for k, m in v.items()})
print((N / "ev15s8" / "window.txt").read_text(errors="replace"))
