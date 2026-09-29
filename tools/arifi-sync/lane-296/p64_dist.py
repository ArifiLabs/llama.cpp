"""lane-296c stage 8: distribution of per-chunk ln-PPL deltas over 64 chunks (paired legs, same chunks).
usage: p64_dist.py <legB.txt> <legA.txt>  (paths relative to night/); prints B - A stats."""
import math
import re
import statistics as st
import sys
from pathlib import Path

N = Path(__file__).parent


def per_chunk(p):
    f = N / p
    txt = f.read_text(encoding="utf-8", errors="replace")
    e = f.with_suffix(".err")
    if e.exists():
        txt += e.read_text(encoding="utf-8", errors="replace")
    run = {}
    for k, v in re.findall(r"\[(\d+)\]([0-9]+\.[0-9]+)", txt):
        run[int(k)] = float(v)
    n = max(run)
    out, prev = [], 0.0
    for k in range(1, n + 1):
        cur = k * math.log(run[k])
        out.append(cur - prev)
        prev = cur
    return out, run[n]


if __name__ == "__main__":
    b, pb = per_chunk(sys.argv[1])
    a, pa = per_chunk(sys.argv[2])
    d = [x - y for x, y in zip(b, a)]
    n = len(d)
    m = sum(d) / n
    se = math.sqrt(sum((x - m) ** 2 for x in d) / (n - 1) / n)
    s = sorted(d)
    k = max(1, n // 10)
    trim = s[k:n - k]
    top3 = sorted(d, key=abs, reverse=True)[:3]
    print(f"n={n} PPL B {pb:.4f} A {pa:.4f} dPPL {pb - pa:+.4f}")
    print(f"mean dln {m:+.5f} SE {se:.5f} ({m / se:+.2f} SE) median {st.median(d):+.5f} trimmed10 {st.mean(trim):+.5f}")
    print(f"B worse {sum(1 for x in d if x > 0)}/{n}; top3 |d| {[round(x, 5) for x in top3]} carry {sum(top3) / sum(d):.2f} of the sum")
    print(f"sum without top3 {sum(d) - sum(top3):+.5f} vs total {sum(d):+.5f}")
    print(f"quartiles {[round(x, 5) for x in st.quantiles(d, n=4)]} sd {st.stdev(d):.5f}")
