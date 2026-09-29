"""lane-296c: read ev12/ladder op-probe logs into a per-layer int8-vs-float table."""
import glob
import re
import statistics as s
import sys

D = sys.argv[1] if len(sys.argv) > 1 else "ev12/ladder"


def rd(p):
    m = re.search(r"^GPU .*$", open(p).read(), re.M).group(0)
    return dict(re.findall(r"(\w+)=([^\s]+)", m))


rows = []
for p in glob.glob(f"{D}/on-*.txt"):
    name = p.split("on-", 1)[1][:-len(".weight.txt")]
    a, b = rd(p), rd(p.replace("/on-", "/off-").replace("\\on-", "\\off-"))
    _, L, role = name.split(".")
    rows.append((role, int(L), float(a["rel_fro"]), float(b["rel_fro"]), float(a["rel_fro_excl_tok0"]),
                 float(b["rel_fro_excl_tok0"]), a["worst_tok"], float(a["max_row"])))
rows.sort()
print("| role | layer | int8 rel err | float rel err | int8/float | int8/float excl tok0 | int8 worst token | int8 worst row |")
print("|---|---|---|---|---|---|---|---|")
for r in rows:
    print(f"| {r[0]} | {r[1]} | {r[2]:.3e} | {r[3]:.3e} | {r[2]/r[3]:.2f} | {r[4]/r[5]:.2f} | {r[6]} | {r[7]:.2e} |")
for role in ("ssm_out", "attn_output"):
    rr = [x for x in rows if x[0] == role]
    print(f"{role}: n={len(rr)} median int8/float={s.median([x[2]/x[3] for x in rr]):.3f} "
          f"excl tok0={s.median([x[4]/x[5] for x in rr]):.3f} "
          f"sum sq int8/sum sq float={sum(x[2]**2 for x in rr)/sum(x[3]**2 for x in rr):.3f} "
          f"max int8={max(x[2] for x in rr):.3e}")
