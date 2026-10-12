"""HQ88 recheck read: v0.1.3.0 (side a, build-integ) vs v0.1.3.2 (side b, build-v0132rev), ABBA loads, lite plan.
Usage: recheck_read.py <run-id> [<run-id> ...] | --selfcheck
Per mode x prompt: samples = r1+r2 of both loads per side (4 a side). decode/prompt: mean a -> mean b, delta %, band =
larger within-side spread (floor 1%). REGRESSION = b below a by more than the band AND every b sample below every a sample.
WIN = b above a by more than the band AND every b sample above every a sample. Else TIE (inside noise).
Plain greedy ids: every b sample vs a1 same round/prompt; a diff = BLOCKER. Drafted ids diff = printed note."""
import json, os, statistics, sys

CELLS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'cells')
NAME = {'P': 'plain', 'O': 'plain, v0.1.3.0 README flags (mmap -c 2048)', 'M': 'MTP n3', 'F': 'MTP n3 + 40K vocab', 'D': 'DFlash2'}


def first_diff(a, b):
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return i
    return None if len(a) == len(b) else min(len(a), len(b))


def judge(a, b):
    band = max((max(a) - min(a)) / statistics.mean(a), (max(b) - min(b)) / statistics.mean(b), 0.01)
    pct = (statistics.mean(b) - statistics.mean(a)) / statistics.mean(a)
    if pct < -band and max(b) < min(a): v = 'REGRESSION'
    elif pct > band and min(b) > max(a): v = 'WIN'
    else: v = 'tie'
    return pct, band, v


def verdict(st, reqs):
    rows, blockers = [], []
    modes = []
    for s in st.get('slots', []):
        if s['mode'] not in modes: modes.append(s['mode'])
    for mode in modes:
        arms = [s for s in st['slots'] if s['mode'] == mode]
        bad = [(s['arm'], s['result']) for s in arms if s['result'] != 'ok']
        if bad or {s['side'] for s in arms} != {'a', 'b'}:
            blockers.append(f"{st['model']} {NAME.get(mode, mode)}: load missing/failed {bad}"); continue
        for p in ('short', 'long'):
            got = {sd: [reqs.get((s['arm'], r, p)) for s in arms if s['side'] == sd for r in ('r1', 'r2')] for sd in 'ab'}
            if any(d is None or not (d.get('timings') or {}).get('predicted_per_second') for v in got.values() for d in v):
                blockers.append(f"{st['model']} {NAME.get(mode, mode)}/{p}: missing request/timings"); continue
            cells = []
            for k, nm in (('predicted_per_second', 'decode'), ('prompt_per_second', 'prompt')):
                a = [d['timings'][k] for d in got['a']]; b = [d['timings'][k] for d in got['b']]
                pct, band, v = judge(a, b)
                cells.append(f"{nm} {statistics.mean(a):.2f} -> {statistics.mean(b):.2f} ({pct:+.2%}, band {band:.1%}, {v})")
                if v == 'REGRESSION' and not (nm == 'prompt' and p == 'short'):
                    blockers.append(f"{st['model']} {NAME.get(mode, mode)}/{p}/{nm} REGRESSION {pct:+.2%}")
            ref = got['a'][0].get('tokens') or []
            diffs = [first_diff(ref, d.get('tokens') or []) for d in got['b']]
            ids = 'same' if all(x is None for x in diffs) else f'diff@{diffs}'
            if ids != 'same' and mode in 'PO':
                blockers.append(f"{st['model']} {NAME.get(mode, mode)}/{p}: plain greedy ids differ {diffs}")
            rows.append(f"| {st['model']} | {NAME.get(mode, mode)} | {p} | {'; '.join(cells)} | ids {ids} |")
    if st.get('state') != 'pointgate-gpu-complete':
        blockers.append(f"{st['model']} state {st.get('state')} {st.get('error', '')}")
    return rows, blockers


def load_run(run):
    st = json.load(open(os.path.join(CELLS, f'{run}-state.json'), encoding='utf-8'))
    reqs = {}
    for s in st.get('slots', []):
        for r in ('r1', 'r2'):
            for p in ('short', 'long'):
                f = os.path.join(CELLS, f"{run}-{s['arm']}-{r}-{p}.json")
                if os.path.exists(f):
                    reqs[(s['arm'], r, p)] = json.load(open(f, encoding='utf-8'))
    return st, reqs


def selfcheck():
    def mk(b_dec, b_ids=(1, 2, 3)):
        st = {'model': 'x', 'state': 'pointgate-gpu-complete', 'slots': []}; reqs = {}
        for sd, tags in (('a', ('a1', 'a2')), ('b', ('b1', 'b2'))):
            for t in tags:
                arm = f"P-{t}-{'p' if sd == 'b' else 'g'}"
                st['slots'].append({'mode': 'P', 'side': sd, 'arm': arm, 'result': 'ok'})
                for i, r in enumerate(('r1', 'r2')):
                    for p in ('short', 'long'):
                        dec = b_dec[i] if sd == 'b' else (10.0, 10.05)[i]
                        reqs[(arm, r, p)] = {'timings': {'predicted_per_second': dec, 'prompt_per_second': 100.0},
                                             'tokens': list(b_ids) if sd == 'b' else [1, 2, 3]}
        return verdict(st, reqs)
    ok = True
    for name, kw, want_block, want_word in (('tie', dict(b_dec=(10.02, 10.0)), False, 'tie'), ('win', dict(b_dec=(10.6, 10.7)), False, 'WIN'),
                                            ('regression', dict(b_dec=(9.4, 9.5)), True, 'REGRESSION'),
                                            ('ids', dict(b_dec=(10.0, 10.0), b_ids=(1, 2, 4)), True, 'tie')):
        rows, bl = mk(**kw)
        good = (bool(bl) == want_block) and want_word in rows[0]
        ok &= good
        print(f"{'GREEN' if good else 'FAIL'} {name}: blockers={len(bl)} row={rows[0][:110]}")
    print('SELFCHECK', 'PASS' if ok else 'FAIL'); return 0 if ok else 1


if __name__ == '__main__':
    if sys.argv[1:] == ['--selfcheck']:
        sys.exit(selfcheck())
    allrows, allblock = [], []
    for run in sys.argv[1:]:
        st, reqs = load_run(run)
        r, b = verdict(st, reqs); allrows += r; allblock += b
    print('| model | mode | prompt | v0.1.3.0 -> v0.1.3.2 (decode t/s; prompt t/s) | greedy ids |\n|---|---|---|---|---|')
    print('\n'.join(allrows))
    print('VERDICT', 'BLOCKER' if allblock else 'CLEAN'); [print(' -', x) for x in allblock]
