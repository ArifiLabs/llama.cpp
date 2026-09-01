#!/usr/bin/env python3
"""arifi-sync-check — build-time guard for hand-synced C <-> GLSL/host pairs (ArifiLabs fork).

WHY THIS EXISTS (failure-ledger F-110): a GLSL constant that mirrors a C symbol drifts SILENTLY.
A wrong centroid produces a slightly wrong number; a stale spec-constant id makes a switch fall
through and return zeros. Nothing aborts, nothing fails validation. A `// Keep in sync with ggml.h`
comment was the entire enforcement mechanism until this file. Related rows: F-109 (a hand-curated
coverage list), F-112 (a green banner over zero executed cases).

WHAT IT CHECKS (all four classes of hand-synced pair found by the lane-149 census):
  1. LUT PAIRS      - every constant array in vulkan-shaders/ is either paired with a C array whose
                      values must match, or WAIVED with a reason. A NEW unpaired array FAILS.
  2. FA_TYPE IDS    - every `#define FA_TYPE_X <n>u` must equal `GGML_TYPE_X = <n>` in ggml.h.
  3. QUANT_K SIZES  - every `#define QUANT_K_X <n>` must equal type_traits[GGML_TYPE_X].blck_size
                      (ggml.c), resolved through the block-size macros in ggml-common.h.
  4. TYPE-LIST SETS - per-type switch blocks in ggml-vulkan.cpp that must agree carry an
                      `// ARIFI-SYNC-SET: <name> [EXCEPT T=reason,...]` marker; every marker with
                      the same <name> must hold the same type set. A big per-type switch with NO
                      marker FAILS (that is how the CPY dispatch/supports_op pair drifted).
  5. FA KV TYPES    - every type the host admits as a flash-attention K/V type must have a
                      FA_TYPE_ define, or the shader switches fall through to zeros (F-110).

Run standalone:  python arifi_sync_check.py [repo-root]
Exit code 0 = in sync, 1 = DIVERGENT (the build stops, by design).
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.abspath(
    os.path.join(HERE, "..", "..", ".."))
SH = os.path.join(ROOT, "ggml", "src", "ggml-vulkan", "vulkan-shaders")
VKCPP = os.path.join(ROOT, "ggml", "src", "ggml-vulkan", "ggml-vulkan.cpp")

errors = []
def fail(msg):
    errors.append(msg)

# ----------------------------------------------------------------------------- parsing helpers
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
NUM = re.compile(r"[-+]?(?:[0-9]*\.[0-9]+(?:[eE][-+]?[0-9]+)?|[0-9]+\.?)(?:[fF])?")

def read(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read()

def numbers(text):
    text = COMMENT.sub(" ", text)
    text = re.sub(r"\b[A-Za-z_]\w*\s*\(", "(", text)   # GLSL writes int8_t(-127)
    out = []
    for m in NUM.finditer(text):
        s = m.group(0).rstrip("fF")
        if s in ("", "+", "-", "."):
            continue
        try:
            out.append(float(s))
        except ValueError:
            pass
    return out

def initialiser(text, idx):
    """The ( ... ) or { ... } initialiser starting at/after idx."""
    open_ch = None
    for i in range(idx, min(idx + 400, len(text))):
        if text[i] in "({":
            open_ch, start = text[i], i
            break
    if open_ch is None:
        return ""
    close_ch = ")" if open_ch == "(" else "}"
    depth = 0
    for i in range(start, len(text)):
        if text[i] == open_ch:
            depth += 1
        elif text[i] == close_ch:
            depth -= 1
            if depth == 0:
                return text[start + 1:i]
    return ""

# ANY type, not a fixed list: the first version enumerated (float|uint|...) and a LUT declared
# `float16_t` or `f16vec2` walked straight past completeness (lane-149 checker, finding 6).
GLSL_ARRAY = re.compile(r"(?:const\s+)?(?:[A-Za-z_]\w*)\s+"
                        r"(?P<name>[A-Za-z_]\w*)\s*\[\s*(?P<n>[0-9]+)\s*\]\s*=")
C_ARRAY = re.compile(r"(?:static\s+)?(?:__constant__\s+|constexpr\s+)?(?:const\s+)?"
                     r"(?:float|double|int8_t|uint8_t|int|uint32_t)\s+"
                     r"(?P<name>[A-Za-z_]\w*)\s*\[\s*(?P<n>[0-9]*)\s*\]\s*=")
C_TABLE = re.compile(r"GGML_TABLE_BEGIN\s*\(\s*\w+\s*,\s*(?P<name>\w+)\s*,\s*(?P<n>[0-9]+)\s*\)"
                     r"(?P<body>.*?)GGML_TABLE_END", re.S)

def glsl_arrays(fname):
    text = read(os.path.join(SH, fname))
    out = {}
    for m in GLSL_ARRAY.finditer(text):
        line = text[:m.start()].count("\n") + 1
        out.setdefault(m.group("name"), []).append(
            (line, numbers(initialiser(text, m.end()))))
    return out

def c_arrays(relpath):
    text = read(os.path.join(ROOT, relpath))
    out = {}
    for m in C_TABLE.finditer(text):
        line = text[:m.start()].count("\n") + 1
        out.setdefault(m.group("name"), []).append((line, numbers(m.group("body"))))
    for m in C_ARRAY.finditer(text):
        line = text[:m.start()].count("\n") + 1
        out.setdefault(m.group("name"), []).append(
            (line, numbers(initialiser(text, m.end()))))
    return out

def approx_equal(a, b, tol=1e-6):
    return len(a) == len(b) and all(
        abs(x - y) <= 1e-9 + tol * max(abs(x), abs(y)) for x, y in zip(a, b))

# ============================================================== 1. LUT pairs (the lane-149 census)
# (C file, C symbol, GLSL file, GLSL symbol). Every definition of the C symbol in that file must
# agree with every definition of the GLSL symbol in that shader — which also guards the C<->C
# duplicate copies (CENTROIDS_4BIT is defined twice inside ggml-turbo-quant.c).
TURBO = "ggml/src/ggml-turbo-quant.c"
WEIGHTS = "ggml/src/ggml-arifi-turbo-weights.c"
CUDA = "ggml/src/ggml-cuda/turbo-quant.cuh"
COMMON = "ggml/src/ggml-common.h"

PAIRS = [
    (WEIGHTS, "TQ3_0_SIGNS",     "copy_from_quant.comp",   "tq4_signs"),
    (TURBO,   "turbo_cpu_s1",    "copy_to_quant.comp",     "TS1"),
    (TURBO,   "turbo_cpu_s2",    "copy_to_quant.comp",     "TS2"),
    (TURBO,   "CENTROIDS_3BIT",  "copy_to_quant.comp",     "TC"),
    (CUDA,    "TURBO_MID_3BIT",  "copy_to_quant.comp",     "TM"),
    (WEIGHTS, "TQ3_0_SIGNS",     "copy_to_quant.comp",     "TQ4_SIGNS"),
    (WEIGHTS, "TQ4_0_CENTROIDS", "copy_to_quant.comp",     "TQ4_CENTROIDS"),
    (TURBO,   "turbo_cpu_s1",    "copy_to_quant.comp",     "TS1_T2"),
    (TURBO,   "turbo_cpu_s2",    "copy_to_quant.comp",     "TS2_T2"),
    (TURBO,   "CENTROIDS_2BIT",  "copy_to_quant.comp",     "TC2"),
    (CUDA,    "TURBO_MID_2BIT",  "copy_to_quant.comp",     "TM2"),
    (TURBO,   "turbo_cpu_s1",    "copy_to_quant.comp",     "TS1_T4"),
    (TURBO,   "turbo_cpu_s2",    "copy_to_quant.comp",     "TS2_T4"),
    (TURBO,   "CENTROIDS_4BIT",  "copy_to_quant.comp",     "TC4"),
    (CUDA,    "TURBO_MID_4BIT",  "copy_to_quant.comp",     "TM4"),
    (WEIGHTS, "search",          "copy_to_quant.comp",     "SCALES"),
    (TURBO,   "CENTROIDS_3BIT",  "dequant_funcs.glsl",     "centroids#0"),
    (TURBO,   "CENTROIDS_2BIT",  "dequant_funcs.glsl",     "centroids#1"),
    (TURBO,   "CENTROIDS_4BIT",  "dequant_funcs.glsl",     "centroids#2"),
    (WEIGHTS, "TQ3_0_CENTROIDS", "dequant_funcs.glsl",     "centroids#3"),
    (WEIGHTS, "TQ4_0_CENTROIDS", "dequant_funcs.glsl",     "centroids#4"),
    (TURBO,   "CENTROIDS_3BIT",  "dequant_funcs_cm2.glsl", "centroids"),
    (WEIGHTS, "TQ3_0_CENTROIDS", "dequant_tq3_1s.comp",    "centroids"),
    (WEIGHTS, "TQ3_0_SIGNS",     "dequant_tq3_1s.comp",    "signs"),
    (WEIGHTS, "TQ3_0_CENTROIDS", "dequant_tq3_4s.comp",    "centroids"),
    (WEIGHTS, "TQ3_0_SIGNS",     "dequant_tq3_4s.comp",    "signs"),
    (WEIGHTS, "TQ4_0_CENTROIDS", "dequant_tq4_1s.comp",    "centroids"),
    (WEIGHTS, "TQ3_0_SIGNS",     "dequant_tq4_1s.comp",    "signs"),
    (TURBO,   "CENTROIDS_3BIT",  "dequant_turbo3_0.comp",  "centroids"),
    (TURBO,   "CENTROIDS_2BIT",  "flash_attn_dequant.glsl", "c#0"),
    (TURBO,   "CENTROIDS_3BIT",  "flash_attn_dequant.glsl", "c#1"),
    (TURBO,   "CENTROIDS_4BIT",  "flash_attn_dequant.glsl", "c#2"),
    (WEIGHTS, "TQ3_0_CENTROIDS", "mul_mat_vec_tq3_1s.comp", "TQ3_CENTROIDS"),
    (WEIGHTS, "TQ3_0_SIGNS",     "mul_mat_vec_tq3_1s.comp", "TQ3_SIGNS"),
    (WEIGHTS, "TQ3_0_CENTROIDS", "mul_mat_vec_tq3_4s.comp", "TQ3_CENTROIDS"),
    (WEIGHTS, "TQ3_0_SIGNS",     "mul_mat_vec_tq3_4s.comp", "TQ3_SIGNS"),
    (WEIGHTS, "TQ4_0_CENTROIDS", "mul_mat_vec_tq4_1s.comp", "TQ4_CENTROIDS"),
    (WEIGHTS, "TQ3_0_SIGNS",     "mul_mat_vec_tq4_1s.comp", "TQ4_SIGNS"),
    (WEIGHTS, "TQ3_0_SIGNS",     "mul_mat_vec_tq_sg.comp",  "TQ_SIGNS"),
    (WEIGHTS, "TQ4_0_CENTROIDS", "mul_mat_vec_tq_sg.comp",  "TQ_CENTROIDS#0"),
    (WEIGHTS, "TQ3_0_CENTROIDS", "mul_mat_vec_tq_sg.comp",  "TQ_CENTROIDS#1"),
    (WEIGHTS, "TQ3_0_CENTROIDS", "mul_mm_funcs.glsl",      "centroids#0"),
    (WEIGHTS, "TQ4_0_CENTROIDS", "mul_mm_funcs.glsl",      "centroids#1"),
    (WEIGHTS, "TQ3_0_SIGNS",     "tq_rotate_act.comp",     "TQ_SIGNS"),
    (TURBO,   "turbo_cpu_s1",    "turbo_wht.comp",         "S1"),
    (TURBO,   "turbo_cpu_s2",    "turbo_wht.comp",         "S2"),
    (COMMON,  "iq3xxs_grid",     "types.glsl",             "iq3xxs_grid_const"),
    (COMMON,  "iq3s_grid",       "types.glsl",             "iq3s_grid_const"),
    (COMMON,  "kvalues_iq4nl",   "types.glsl",             "kvalues_iq4nl_const"),
    (COMMON,  "kvalues_fp4",     "types.glsl",             "kvalues_mxfp4_const"),
    ("ggml/rocmfp4/rocmfp4.c",   "rocmfp4_codebook", "types.glsl", "kvalues_rocmfp4_const"),
    ("ggml/rocmfpx/rocmfpx.c",   "values",           "types.glsl", "kvalues_rocmfpx_fp2_const"),
]

# GLSL constant arrays with NO C array to diff. Every entry carries the reason it cannot drift
# against a C literal — an empty reason is not accepted.
WAIVED = {
    ("concat.comp", "o"):
        "GLSL-local zero init (uint o[4] = {0,0,0,0}), not a mirror of anything",
    ("types.glsl", "iq2xs_grid_const"):
        "upstream iq2xs grid: GLSL packs each 8-byte C entry into a uvec2, so an elementwise "
        "diff is not defined; the C table is upstream-frozen (ggml-common.h iq2xs_grid)",
    ("mul_mat_vec_q6_k.comp", "sum"):
        "local accumulator zero-init (FLOAT_TYPE sum[4] = {0,0,0,0}), not a mirror",
    ("types.glsl", "iq2s_grid_const"):
        "upstream iq2s grid, same uvec2 packing as iq2xs_grid_const",
    ("types.glsl", "kvalues_rocmfpx_fp3_const"):
        "DERIVED mirror: the C truth is mag[4]={0,1,2,4} plus a sign bit "
        "(ggml/rocmfpx/rocmfpx.c rocmfpx_decode_fp3_code), not a literal 8-entry array",
    ("types.glsl", "kvalues_rocmfpx_fp6_const"):
        "DERIVED mirror: the C truth is the FP6 magnitude 0..31 plus a sign bit "
        "(ggml/rocmfpx/rocmfpx.c), not a literal 64-entry array",
}

def shader_files():
    """Every shader in the tree, subdirectories included (feature-tests/ was missed at first)."""
    out = []
    for dirpath, _, files in os.walk(SH):
        for fn in sorted(files):
            if fn.endswith((".comp", ".glsl")):
                out.append(os.path.relpath(os.path.join(dirpath, fn), SH).replace("\\", "/"))
    return sorted(out)

def check_lut_pairs():
    # `NAME#K` pins the K-th definition of NAME in that shader (ORDINAL, not line number: a pin by
    # line silently stopped matching as soon as anything above it moved - lane-149 checker, f.2).
    covered = set()
    for cfile, csym, gfile, gsym in PAIRS:
        gname, _, ordinal = gsym.partition("#")
        cs = c_arrays(cfile).get(csym)
        gs = glsl_arrays(gfile).get(gname)
        if not cs:
            fail("PAIR %s:%s -> %s:%s : C symbol NOT FOUND (renamed or removed?)"
                 % (cfile, csym, gfile, gname))
            continue
        if not gs:
            fail("PAIR %s:%s -> %s:%s : GLSL symbol NOT FOUND" % (cfile, csym, gfile, gname))
            continue
        if ordinal:
            k = int(ordinal)
            if k >= len(gs):
                fail("PAIR %s:%s#%d : that shader has only %d definition(s) of %s - the pin is "
                     "stale, so the pair was NOT being diffed" % (gfile, gname, k, len(gs), gname))
                continue
            targets = [(k, gs[k])]
        else:
            targets = list(enumerate(gs))
        want = cs[0][1]
        for line, vals in cs[1:]:
            if not approx_equal(want, vals):
                fail("C<->C DIVERGENT: %s:%s has copies that disagree (line %d)"
                     % (cfile, csym, line))
        for k, (line, vals) in targets:
            covered.add((gfile, gname, k))
            if not approx_equal(want, vals):
                fail("DIVERGENT: %s:%d %s does not match %s:%s\n"
                     "        C   (%d values): %s\n        GLSL(%d values): %s"
                     % (gfile, line, gname, cfile, csym,
                        len(want), want[:8], len(vals), vals[:8]))
    # completeness: EVERY definition must be covered by a row (not merely the name - deleting one
    # of several rows for the same name used to slip past: lane-149 checker, finding 4)
    for fname in shader_files():
        for name, defs in glsl_arrays(fname).items():
            for k, (line, vals) in enumerate(defs):
                if not vals:
                    continue
                if (fname, name, k) in covered or (fname, name) in WAIVED:
                    continue
                fail("UNREGISTERED constant array %s:%d %s (definition #%d) - add a PAIR row naming "
                     "its C source of truth, or a WAIVED entry with a reason, in "
                     "ggml-vulkan/arifi_sync_check.py" % (fname, line, name, k))
    for key, reason in WAIVED.items():
        if not reason.strip():
            fail("WAIVED entry %s carries no reason" % (key,))

# ============================================================== enum / block-size ground truth
def ggml_enum():
    text = read(os.path.join(ROOT, "ggml", "include", "ggml.h"))
    out = {}
    for m in re.finditer(r"^\s*GGML_TYPE_([A-Z0-9_]+)\s*=\s*([0-9]+)", text, re.M):
        out[m.group(1)] = int(m.group(2))
    return out

def blck_sizes(enum):
    """type_traits[GGML_TYPE_X].blck_size (ggml.c), resolved through the QK_* block-size macros
    (ggml-common.h plus the two imported ROCmFP headers; some are macro->macro aliases)."""
    raw = {}
    for rel in ("ggml/src/ggml-common.h", "ggml/rocmfp4/rocmfp4.h", "ggml/rocmfpx/rocmfpx.h"):
        p = os.path.join(ROOT, rel)
        if not os.path.exists(p):
            continue
        for m in re.finditer(r"^#define\s+(QK[A-Za-z0-9_]*)\s+([A-Za-z0-9_]+)", read(p), re.M):
            raw.setdefault(m.group(1), m.group(2))
    macro = {}
    for k in raw:
        v, hops = raw[k], 0
        while not v.isdigit() and v in raw and hops < 8:
            v, hops = raw[v], hops + 1
        if v.isdigit():
            macro[k] = int(v)
    text = read(os.path.join(ROOT, "ggml", "src", "ggml.c"))
    out = {}
    for m in re.finditer(r"\[GGML_TYPE_([A-Z0-9_]+)\]\s*=\s*\{(.*?)\n\s*\},", text, re.S):
        name, body = m.group(1), m.group(2)
        b = re.search(r"\.blck_size\s*=\s*([A-Za-z0-9_]+)", body)
        if not b:
            continue
        v = b.group(1)
        out[name] = int(v) if v.isdigit() else macro.get(v)
    return out

# ============================================================== 2. FA_TYPE ids
# b10680 moved the `#define FA_TYPE_*` block out of flash_attn_base.glsl into the new shared header
# fa_types.glsl (upstream extraction; lightning_indexer.comp includes it too). Reading only the old
# file silently found ZERO ids and turned checks 2 and 5 into no-ops, which is the exact failure
# class F-110 exists to catch. Read every file that can legally hold the block.
FA_TYPE_SOURCES = ("fa_types.glsl", "flash_attn_base.glsl")


def read_fa_type_defs():
    out = []
    for name in FA_TYPE_SOURCES:
        path = os.path.join(SH, name)
        if os.path.exists(path):
            out.append(read(path))
    if not out:
        fail("none of %s exists — checks 2 and 5 cannot fire" % (FA_TYPE_SOURCES,))
    return "\n".join(out)


def check_fa_type_ids(enum):
    text = read_fa_type_defs()
    n = 0
    # accept every legal spelling: `201u`, `201U`, `201`, `0xC9`, plus a trailing comment. The
    # first version demanded a lowercase `u` and a `44U` plant sailed through (checker, f.1).
    for m in re.finditer(
            r"^#define\s+FA_TYPE_([A-Z0-9_]+)\s+(0[xX][0-9a-fA-F]+|[0-9]+)\s*[uU]?\s*(?://.*)?$",
            text, re.M):
        name, lit = m.group(1), m.group(2)
        val = int(lit, 16) if lit[:2].lower() == "0x" else int(lit)
        n += 1
        if name not in enum:
            fail("FA_TYPE_%s has no GGML_TYPE_%s in ggml.h" % (name, name))
        elif enum[name] != val:
            fail("FA_TYPE_%s = %du but GGML_TYPE_%s = %d (ggml.h). The host passes the raw enum, "
                 "so a stale id makes the shader switch fall through and return zeros — silently."
                 % (name, val, name, enum[name]))
    return n

# ============================================================== 3. QUANT_K block sizes
QUANT_K_WAIVED = {}

# The shaders name a few types by their family shorthand; map it to the enum name.
QUANT_K_ALIAS = {
    "ROCMFP4":     "Q4_0_ROCMFP4",
    "ROCMFPX_FP2": "Q2_0_ROCMFPX",
    "ROCMFPX_FP3": "Q3_0_ROCMFPX",
    "ROCMFPX_FP6": "Q6_0_ROCMFPX",
    "ROCMFPX_FP8": "Q8_0_ROCMFPX",
}

def check_quant_k(enum, blck):
    text = read(os.path.join(SH, "types.glsl"))
    n = 0
    # a trailing comment is legal GLSL and ordinary style; requiring end-of-line let a wrong
    # block size hide behind `// probe` (checker, f.3).
    for m in re.finditer(r"^#define\s+QUANT_K_([A-Z0-9_]+)\s+([0-9]+)\s*(?://.*|/\*.*)?$",
                         text, re.M):
        name, val = m.group(1), int(m.group(2))
        name = QUANT_K_ALIAS.get(name, name)
        n += 1
        if name in QUANT_K_WAIVED:
            continue
        if name not in enum:
            fail("QUANT_K_%s has no GGML_TYPE_%s in ggml.h" % (name, name))
        elif blck.get(name) is None:
            fail("QUANT_K_%s: no resolvable blck_size for GGML_TYPE_%s in ggml.c type_traits"
                 % (name, name))
        elif blck[name] != val:
            fail("QUANT_K_%s = %d but type_traits[GGML_TYPE_%s].blck_size = %d"
                 % (name, val, name, blck[name]))
    return n

# ============================================================== 4. host per-type list sets
MARKER = re.compile(r"//\s*ARIFI-SYNC-(?P<kind>SET|SOLO)\s*:\s*(?P<rest>[^\n]*)")
MIN_CASES = 8   # a switch this wide over ggml_type is a hand-synced list by definition

def check_type_list_sets():
    text = read(VKCPP)
    lines = text.split("\n")
    regions = []            # (kind, name, excepts, start_line, types)
    i = 0
    while i < len(lines):
        m = MARKER.search(lines[i])
        if m:
            rest = m.group("rest")
            name, _, exc = rest.partition("EXCEPT")
            excepts = {}
            # split only where the next token is another TYPE= entry, so a reason may contain commas
            for part in re.split(r",\s*(?=[A-Z][A-Z0-9_]*\s*=)", exc):
                part = part.strip()
                if not part:
                    continue
                t, _, why = part.partition("=")
                excepts[t.strip()] = why.strip()
                if not why.strip():
                    fail("ggml-vulkan.cpp:%d EXCEPT %s carries no reason" % (i + 1, t.strip()))
            # find the switch that follows and take its case labels
            j = i + 1
            while j < len(lines) and "switch" not in lines[j]:
                j += 1
                if j - i > 6:
                    fail("ggml-vulkan.cpp:%d ARIFI-SYNC marker is not followed by a switch" % (i + 1))
                    break
            types, depth, started = [], 0, False
            while j < len(lines):
                depth += lines[j].count("{") - lines[j].count("}")
                started = started or "{" in lines[j]
                for mm in re.finditer(r"case\s+GGML_TYPE_([A-Z0-9_]+)\s*:", lines[j]):
                    types.append(mm.group(1))
                if started and depth <= 0:
                    break
                j += 1
            regions.append((m.group("kind"), name.strip(), excepts, i + 1, set(types)))
            i = j
        i += 1

    marked = {r[3] for r in regions}
    # completeness: every wide per-type switch must be marked. supports_op nests type switches
    # inside `switch (op->op)`, so track a STACK and attribute each case to the innermost switch.
    stack = []                      # [start_line, brace_depth, [types], seen_open]
    depth = 0
    for n, ln in enumerate(lines, 1):
        opens, closes = ln.count("{"), ln.count("}")
        if re.search(r"\bswitch\s*\(", ln):
            stack.append([n, depth, [], False])
        if stack:
            for mm in re.finditer(r"case\s+GGML_TYPE_([A-Z0-9_]+)\s*:", ln):
                stack[-1][2].append(mm.group(1))
        depth += opens
        if opens:
            for fr in stack:
                fr[3] = True
        depth -= closes
        while stack and stack[-1][3] and depth <= stack[-1][1]:
            start, _, types, _ = stack.pop()
            if len(types) >= MIN_CASES and not any(start - 4 <= mk <= start for mk in marked):
                fail("ggml-vulkan.cpp:%d — per-type switch with %d GGML_TYPE cases and NO "
                     "ARIFI-SYNC marker. Add `// ARIFI-SYNC-SET: <name> [EXCEPT T=why]` when "
                     "it must agree with another list, or `// ARIFI-SYNC-SOLO: <why it stands "
                     "alone>`." % (start, len(types)))

    groups = {}
    for kind, name, excepts, line, types in regions:
        if kind == "SET":
            groups.setdefault(name, []).append((line, types - set(excepts)))
    for name, members in groups.items():
        if len(members) < 2:
            fail("ARIFI-SYNC-SET '%s' has only one member (line %d) — a set of one cannot drift; "
                 "use ARIFI-SYNC-SOLO or name the sibling list." % (name, members[0][0]))
            continue
        base_line, base = members[0]
        for line, types in members[1:]:
            if types != base:
                only_a = sorted(base - types)
                only_b = sorted(types - base)
                fail("SET '%s' DIVERGENT between ggml-vulkan.cpp:%d and :%d\n"
                     "        only at :%d -> %s\n        only at :%d -> %s"
                     % (name, base_line, line, base_line, only_a, line, only_b))
    return len(regions), len(groups)

# ============================================================== 6. scalar mirrors
# Scalar constants mirroring a C value. The C side is sometimes a #define and sometimes an inline
# literal inside a function, so the rule is: the GLSL value must still appear as a float literal in
# the named C file. Change the C constant and the GLSL copy stops matching, which is the point.
SCALARS = [
    (TURBO,   "copy_to_quant.comp",      "TINV",           "1/sqrt(128) turbo WHT normaliser"),
    (TURBO,   "copy_to_quant.comp",      "TINV_T2",        "1/sqrt(128) turbo2"),
    (TURBO,   "copy_to_quant.comp",      "TINV_T4",        "1/sqrt(128) turbo4"),
    (WEIGHTS, "copy_to_quant.comp",      "TQ4_INV_SQRT32", "TQ_INV_SQRT32 = 1/sqrt(32)"),
    (WEIGHTS, "copy_from_quant.comp",    "TQ4_INV_SQRT32", "TQ_INV_SQRT32"),
    (WEIGHTS, "mul_mat_vec_tq4_1s.comp", "TQ4_INV_SQRT32", "TQ_INV_SQRT32"),
    (WEIGHTS, "tq_rotate_act.comp",      "TQ_INV_SQRT32",  "TQ_INV_SQRT32"),
]

def check_scalars():
    n = 0
    for cfile, gfile, gname, note in SCALARS:
        gtext = read(os.path.join(SH, gfile))
        m = re.search(r"(?:const\s+float\s+%s\s*=\s*|#define\s+%s\s+)([-+0-9.eE]+)"
                      % (gname, gname), gtext)
        if not m:
            fail("SCALAR %s:%s NOT FOUND (renamed?) - the pair was not being checked"
                 % (gfile, gname))
            continue
        n += 1
        val = float(m.group(1).rstrip("fF"))
        cvals = numbers(read(os.path.join(ROOT, cfile)))
        if not any(abs(v - val) <= 1e-12 + 1e-9 * abs(val) for v in cvals):
            fail("SCALAR DIVERGENT: %s:%s = %r (%s) does not appear anywhere in %s. A wrong scale "
                 "is a silently wrong number, never an abort." % (gfile, gname, val, note, cfile))
    return n

# ============================================================== 5. FA K/V types have a FA_TYPE id
def check_fa_kv_coverage():
    text = read(VKCPP)
    m = re.search(r"auto fa_kv_ok = \[\]\(ggml_type t\) \{(.*?)\n\s*\};", text, re.S)
    if not m:
        fail("fa_kv_ok lambda not found in ggml-vulkan.cpp (renamed?) — check 5 cannot fire")
        return 0
    host = {mm.group(1) for mm in re.finditer(r"case\s+GGML_TYPE_([A-Z0-9_]+)\s*:", m.group(1))}
    glsl = set(re.findall(r"^#define\s+FA_TYPE_([A-Z0-9_]+)\s", read_fa_type_defs(), re.M))
    for t in sorted(host - glsl):
        fail("host admits GGML_TYPE_%s as a flash-attention K/V type but the shaders have no "
             "FA_TYPE_%s — both FA switches fall through and V dequantises to vec4(0)." % (t, t))
    return len(host)

# ============================================================================================ main
def main():
    enum = ggml_enum()
    blck = blck_sizes(enum)
    check_lut_pairs()
    n_fa = check_fa_type_ids(enum)
    n_qk = check_quant_k(enum, blck)
    n_regions, n_sets = check_type_list_sets()
    n_kv = check_fa_kv_coverage()
    n_scalar = check_scalars()

    # EXPECTED COUNTS. Without these, every "the parser stopped seeing it" bug is invisible: the
    # summary line prints a smaller number and the build goes green (lane-149 checker, f.5).
    # Changing a count is a DELIBERATE edit that lands with the change that caused it.
    for label, got, want in (("FA_TYPE ids", n_fa, 16), ("QUANT_K sizes", n_qk, 35),
                             # +2 regions / +1 set: lane-194's cpy.quant_to_f16 pair (the FIFTH
                             # hand-synced pair) - the quant->F16 switch in
                             # ggml_vk_get_cpy_pipeline and its partner in supports_op.
                             ("marked type lists", n_regions, 18), ("SETs", n_sets, 5),
                             ("FA K/V types", n_kv, 12), ("scalar mirrors", n_scalar, len(SCALARS)),
                             ("LUT pairs", len(PAIRS), 52)):  # +3: mul_mat_vec_tq_sg.comp (lane-163)
        if got != want:
            fail("COUNT CHANGED: %s = %d, expected %d. If you added or removed one deliberately, "
                 "update the expected count in arifi_sync_check.py main(); if you did not, a mirror "
                 "just stopped being checked." % (label, got, want))

    if errors:
        sys.stderr.write("\n=== arifi-sync-check FAILED: %d divergence(s) ===\n" % len(errors))
        for e in errors:
            sys.stderr.write("  [X] %s\n" % e)
        sys.stderr.write("\nThese pairs are hand-synced by construction (failure-ledger F-110).\n"
                         "Fix the mirror, or update the C source of truth — never silence this.\n")
        return 1
    print("arifi-sync-check OK: %d LUT pairs, %d scalar mirrors, %d FA_TYPE ids, %d QUANT_K "
          "sizes, %d marked type lists in %d sets, %d FA K/V types"
          % (len(PAIRS), n_scalar, n_fa, n_qk, n_regions, n_sets, n_kv))
    return 0

if __name__ == "__main__":
    sys.exit(main())
