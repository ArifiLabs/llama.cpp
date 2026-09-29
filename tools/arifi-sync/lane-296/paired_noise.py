"""lane-296c: paired per-chunk PPL differences between S-X8 KL legs (same 16 chunks, c512, seed 1).

Per-chunk ln PPL is recovered from the running PPL column: l_k = k ln P_k - (k-1) ln P_(k-1).
Pairs share the chunks, so d_k = l_k(A) - l_k(B) removes the chunk-to-chunk text variance; its SE is the
instrument's paired noise. Output: mean dPPL, paired SE in PPL units, and the sign count.
"""
import math
import re
import sys
from pathlib import Path

N = Path(__file__).parent
LEGS = {
    "off": "ev6/sx8-off.txt", "all-on": "ev6/sx8-on.txt",
    "A(skip ssm_out)": "ev10/a-sx8.txt", "B1": "ev10/b1-sx8.txt", "B2": "ev10/b2-sx8.txt", "B3": "ev11/b3-sx8.txt",
    "only attn_q": "ev8/g-onlyq.txt", "rows>=6145": "ev6/sx8-r6145.txt", "only alpha,beta": "ev8/g-onlyab.txt",
    "skip ssm_out+attn_out": "ev8/g-skipout.txt", "skip attn_out": "ev8/g-skipattnout.txt",
    "only ssm_out+attn_out": "ev8/g-onlyout.txt", "skip alpha,beta": "ev8/g-skipab.txt",
    "B5": "ev13/b5-sx8.txt",
}
ROW = re.compile(r"^\s*(\d+)\s+([0-9.]+)\s+\+/-|^\s*(\d+)\s+([0-9.]+)\s+±")


def chunks(p):
    f = N / p
    if not f.exists():
        return None
    txt = f.read_text(encoding="utf-8", errors="replace")
    err = f.with_suffix(".err")
    if err.exists():
        txt += err.read_text(encoding="utf-8", errors="replace")
    run = {}
    for line in txt.splitlines():
        m = re.match(r"^\s*(\d+)\s+([0-9.]+)\s+±", line)
        if m and 1 <= int(m.group(1)) <= 16:
            run[int(m.group(1))] = float(m.group(2))
    if len(run) != 16:
        return None
    out, prev = [], 0.0
    for k in range(1, 17):
        cur = k * math.log(run[k])
        out.append(cur - prev)
        prev = cur
    return out, run[16]


def pair(a, b, data):
    la, pa = data[a]
    lb, pb = data[b]
    d = [x - y for x, y in zip(la, lb)]
    m = sum(d) / 16
    se = math.sqrt(sum((x - m) ** 2 for x in d) / 15 / 16)
    pos = sum(1 for x in d if x > 0)
    return pa - pb, pb * se, pos


data = {k: v for k, v in ((k, chunks(p)) for k, p in LEGS.items()) if v}
missing = [k for k in LEGS if k not in data]
print("legs read:", ", ".join(f"{k} {v[1]:.4f}" for k, v in data.items()))
if missing:
    print("missing or incomplete:", ", ".join(missing))
PAIRS = [("B3", "A(skip ssm_out)"), ("B2", "off"), ("all-on", "off"), ("A(skip ssm_out)", "off"), ("B1", "all-on"),
         ("only attn_q", "rows>=6145"), ("only attn_q", "off"), ("only alpha,beta", "off"), ("all-on", "A(skip ssm_out)"),
         ("B5", "A(skip ssm_out)"), ("B5", "B3"), ("B5", "off"), ("B1", "B3")]
print("| pair | dPPL | paired SE (PPL) | dPPL / SE | chunks where first is worse |")
print("|---|---|---|---|---|")
for a, b in PAIRS:
    if a in data and b in data:
        dp, se, pos = pair(a, b, data)
        print(f"| {a} - {b} | {dp:+.4f} | {se:.4f} | {dp / se:+.1f} | {pos}/16 |")


def chunks64(p):
    """llama-perplexity PPL-only output: running '[k]ppl' values."""
    f = N / p
    if not f.exists():
        return None
    txt = f.read_text(encoding="utf-8", errors="replace")  # stdout only: a stray [k]x in stderr must not overwrite
    run ={int(k): float(v) for k, v in re.findall(r"\[(\d+)\]([0-9.]+)", txt)}
    n = max(run) if run else 0
    if n == 0 or any(k not in run for k in range(1, n + 1)):
        return None
    out, prev = [], 0.0
    for k in range(1, n + 1):
        cur = k * math.log(run[k])
        out.append(cur - prev)
        prev = cur
    return out, run[n]


a64, b64 = chunks64("ev14/a64.txt"), chunks64("ev14/b564.txt")
if a64 and b64 and len(a64[0]) == len(b64[0]):
    n = len(a64[0])
    d = [x - y for x, y in zip(b64[0], a64[0])]
    m = sum(d) / n
    se = math.sqrt(sum((x - m) ** 2 for x in d) / (n - 1) / n)
    print(f"64-chunk: A {a64[1]:.4f}, B5 {b64[1]:.4f}, B5 - A = {b64[1] - a64[1]:+.4f}, paired SE {a64[1] * se:.4f}, "
          f"{(b64[1] - a64[1]) / (a64[1] * se):+.1f} SE, B5 worse in {sum(1 for x in d if x > 0)}/{n} chunks")
else:
    print("64-chunk pair not complete yet (ev14/a64.txt, ev14/b564.txt)")
if len(sys.argv) > 1:
    print(data)
