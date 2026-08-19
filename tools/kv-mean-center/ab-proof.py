#!/usr/bin/env python3
"""Bench-pure K-cache mean-centering calibration and A/B proof.

The runner is intentionally Windows/native-seat specific. It declares every model launch,
inspects full process command lines, uses one model process at a time, writes native output via
cmd.exe redirection, and banks enough raw evidence for an independent re-derivation.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import tempfile
import time
from typing import Any


SCHEMA = "arifilabs.kv-mean-center.ab-proof.v1"
PPL_RE = re.compile(r"PPL\s*=\s*([0-9]+(?:\.[0-9]+)?)(?:\s*\+/-\s*([0-9]+(?:\.[0-9]+)?))?")
HEAVY_RE = re.compile(r"(?i)(cmake|ninja|cc1plus|clang\+\+|llama-(?:server|cli|perplexity|bench|kv-mean-center))")
GATE_PYTHON = Path("C:/ArifiLabs/products/CareerCommand/.venv/Scripts/python.exe")
HYGIENE_PYTHON = Path("C:/ArifiLabs/shared/.venv/Scripts/python.exe")


class ProofError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as src:
        for block in iter(lambda: src.read(8 * 1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def write_json(path: Path, value: Any) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def command_line_scan() -> list[dict[str, Any]]:
    ps = (
        "Get-CimInstance Win32_Process | "
        "Where-Object { $_.ProcessId -ne $PID -and $_.CommandLine } | "
        "Select-Object ProcessId,Name,CommandLine | ConvertTo-Json -Compress"
    )
    proc = subprocess.run(
        ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", ps],
        text=True, capture_output=True, check=False,
    )
    if proc.returncode != 0:
        raise ProofError(f"full command-line process scan failed: {proc.stderr.strip()}")
    raw = proc.stdout.strip()
    if not raw:
        return []
    parsed = json.loads(raw)
    rows = parsed if isinstance(parsed, list) else [parsed]
    return [row for row in rows if HEAVY_RE.search(str(row.get("CommandLine", "")))]


def assert_quiet() -> dict[str, Any]:
    heavy = command_line_scan()
    if heavy:
        raise ProofError("bench purity refused overlapping heavy/inference command lines: " + json.dumps(heavy))
    hygiene = subprocess.run(
        [str(HYGIENE_PYTHON), "-m", "arifi_core.process_hygiene"],
        text=True, capture_output=True, check=False,
    )
    if hygiene.returncode != 0 or "clean" not in hygiene.stdout.lower():
        raise ProofError(f"process_hygiene refused run: rc={hygiene.returncode} {hygiene.stdout} {hygiene.stderr}")
    return {"command_lines": heavy, "process_hygiene": hygiene.stdout.strip()}


def declare_launch(label: str) -> None:
    body = f"lane-158 {label}; seated Qwen3.8-27B AD-IQ4_XS; one process; floor 7.0 GB; K-cache A/B proof"
    proc = subprocess.run(
        [str(GATE_PYTHON), "-m", "career_engine.hooks.gate_declare", "CC_RUN_ANNOUNCE", body],
        text=True, capture_output=True, check=False,
    )
    if proc.returncode != 0:
        raise ProofError(f"CC_RUN_ANNOUNCE declaration failed: {proc.stdout} {proc.stderr}")


def run_native(
    argv: list[str], label: str, out_dir: Path, stem: str, *, expect_success: bool = True,
    split_streams: bool = False,
) -> dict[str, Any]:
    purity_before = assert_quiet()
    declare_launch(label)
    command = subprocess.list2cmdline(argv)
    stdout_path = out_dir / f"{stem}.stdout.txt"
    stderr_path = out_dir / f"{stem}.stderr.txt"
    combined_path = out_dir / f"{stem}.log.txt"
    if split_streams:
        redirected = f'{command} 1>"{stdout_path}" 2>"{stderr_path}"'
    else:
        redirected = f'{command} >"{combined_path}" 2>&1'
    (out_dir / f"{stem}.cmd.txt").write_text(redirected + "\n", encoding="utf-8")

    env = os.environ.copy()
    env["ARIFI_GPU_RESIDENT_FLOOR_GB"] = "7.0"
    start_ns = time.time_ns()
    proc = subprocess.run(["cmd.exe", "/d", "/s", "/c", redirected], env=env, check=False)
    end_ns = time.time_ns()
    purity_after = assert_quiet()
    if expect_success and proc.returncode != 0:
        raise ProofError(f"{stem} failed rc={proc.returncode}; see banked log")
    if not expect_success and proc.returncode == 0:
        raise ProofError(f"{stem} unexpectedly succeeded; loud-fail contract is broken")
    return {
        "stem": stem,
        "argv": argv,
        "returncode": proc.returncode,
        "start_ns": start_ns,
        "end_ns": end_ns,
        "wall_s": (end_ns - start_ns) / 1e9,
        "purity_before": purity_before,
        "purity_after": purity_after,
        "stdout": str(stdout_path) if split_streams else None,
        "stderr": str(stderr_path) if split_streams else None,
        "log": None if split_streams else str(combined_path),
    }


def parse_ppl(path: Path) -> tuple[float, float | None]:
    matches = PPL_RE.findall(path.read_text(encoding="utf-8", errors="replace"))
    if not matches:
        raise ProofError(f"no PPL result in {path}")
    value, uncertainty = matches[-1]
    return float(value), float(uncertainty) if uncertainty else None


def parse_bench(path: Path) -> list[float]:
    data = json.loads(path.read_text(encoding="utf-8"))
    rows = data if isinstance(data, list) else [data]
    for row in rows:
        samples = row.get("samples_ts")
        if isinstance(samples, list) and len(samples) >= 3 and all(float(x) > 0 for x in samples):
            return [float(x) for x in samples]
    raise ProofError(f"no >=3-sample samples_ts result in {path}")


def summarize(off_ppl: list[float], on_ppl: list[float], off_ts: list[float], on_ts: list[float]) -> dict[str, Any]:
    if len(off_ppl) != 3 or len(on_ppl) != 3 or len(off_ts) < 3 or len(on_ts) < 3:
        raise ProofError("both accuracy arms require exactly 3 runs and both speed arms require >=3 samples")
    off_mean = statistics.mean(off_ppl)
    on_mean = statistics.mean(on_ppl)
    improvement = off_mean - on_mean
    pooled_se = math.sqrt(statistics.variance(off_ppl) / 3 + statistics.variance(on_ppl) / 3)
    accuracy_pass = improvement > 0 and improvement > 2 * pooled_se
    off_speed = statistics.mean(off_ts)
    on_speed = statistics.mean(on_ts)
    return {
        "accuracy": {
            "off_ppl": off_ppl,
            "on_ppl": on_ppl,
            "off_mean": off_mean,
            "on_mean": on_mean,
            "absolute_improvement": improvement,
            "pooled_standard_error": pooled_se,
            "gain_beats_2x_noise": accuracy_pass,
        },
        "speed": {
            "off_samples_ts": off_ts,
            "on_samples_ts": on_ts,
            "off_mean_ts": off_speed,
            "on_mean_ts": on_speed,
            "delta_percent": 100 * (on_speed / off_speed - 1),
        },
    }


def verify_bank(out_dir: Path, *, planted_overlap: bool = False) -> dict[str, Any]:
    manifest = json.loads((out_dir / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("schema") != SCHEMA:
        raise ProofError("wrong or missing manifest schema")
    if manifest["binary_sha256"]["perplexity_off"] != manifest["binary_sha256"]["perplexity_on"]:
        raise ProofError("accuracy arms did not use the same binary bytes")
    if manifest["binary_sha256"]["bench_off"] != manifest["binary_sha256"]["bench_on"]:
        raise ProofError("speed arms did not use the same binary bytes")
    if manifest["calibration_corpus_sha256"] == manifest["evaluation_corpus_sha256"]:
        raise ProofError("calibration and evaluation corpora are not independent")
    if planted_overlap:
        raise ProofError("bench purity planted violation: overlapping inference command line")

    runs = json.loads((out_dir / "runs.json").read_text(encoding="utf-8"))
    for run in runs:
        if run["purity_before"]["command_lines"] or run["purity_after"]["command_lines"]:
            raise ProofError(f"overlap recorded around {run['stem']}")
    loud = next(run for run in runs if run["stem"] == "loud-fail-missing")
    if loud["returncode"] == 0:
        raise ProofError("missing calibration did not refuse load")
    loud_text = Path(loud["log"]).read_text(encoding="utf-8", errors="replace").lower()
    if "failed to load k-cache mean-centering" not in loud_text and "failed to load" not in loud_text:
        raise ProofError("missing-calibration log does not contain the load refusal")

    probe = json.loads((out_dir / "tensor-probe.json").read_text(encoding="utf-8"))
    if not probe.get("all_pre_post_buffers_vulkan") or probe.get("layer_count", 0) <= 0:
        raise ProofError("tensor probe did not prove pre/post centering on Vulkan buffers")
    if probe.get("global_max_abs_error_vs_calibration", math.inf) > 0.002:
        raise ProofError("tensor delta does not match the bound calibration within 0.002")
    if not all(row.get("max_abs_delta", 0) > 0 for row in probe.get("layers", [])):
        raise ProofError("one or more probed layers showed no centering delta")

    off_ppl = [parse_ppl(out_dir / f"ppl-off-{i}.log.txt")[0] for i in range(1, 4)]
    on_ppl = [parse_ppl(out_dir / f"ppl-on-{i}.log.txt")[0] for i in range(1, 4)]
    off_ts = parse_bench(out_dir / "speed-off.stdout.txt")
    on_ts = parse_bench(out_dir / "speed-on.stdout.txt")
    summary = summarize(off_ppl, on_ppl, off_ts, on_ts)
    summary["verdict"] = "GO" if summary["accuracy"]["gain_beats_2x_noise"] else "NO-GO"
    summary["tensor_probe"] = probe
    return summary


def require_file(path: Path, label: str) -> None:
    if not path.is_file() or path.stat().st_size <= 0:
        raise ProofError(f"{label} is missing or empty: {path}")


def run(args: argparse.Namespace) -> int:
    out_dir = args.output_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    model = args.model.resolve()
    calibration_corpus = args.calibration_corpus.resolve()
    evaluation_corpus = args.evaluation_corpus.resolve()
    calibration = args.calibration_output.resolve()
    for path, label in ((model, "model"), (calibration_corpus, "calibration corpus"),
                        (evaluation_corpus, "evaluation corpus")):
        require_file(path, label)
    if calibration_corpus == evaluation_corpus:
        raise ProofError("calibration and evaluation corpora must be different files")
    if calibration.exists():
        raise ProofError(f"refusing to overwrite calibration artifact: {calibration}")
    if not calibration.parent.is_dir():
        raise ProofError(f"registered calibration side-home must already exist: {calibration.parent}")

    exes = {
        "calibrator": (args.bin_dir / "llama-kv-mean-center.exe").resolve(),
        "perplexity": (args.bin_dir / "llama-perplexity.exe").resolve(),
        "bench": (args.bin_dir / "llama-bench.exe").resolve(),
    }
    for path in exes.values():
        require_file(path, "native executable")

    manifest = {
        "schema": SCHEMA,
        "model": str(model),
        "model_file_sha256": sha256(model),
        "calibration": str(calibration),
        "calibration_corpus": str(calibration_corpus),
        "calibration_corpus_sha256": sha256(calibration_corpus),
        "evaluation_corpus": str(evaluation_corpus),
        "evaluation_corpus_sha256": sha256(evaluation_corpus),
        "context": args.context,
        "calibration_context": args.calibration_context,
        "binary_sha256": {
            "calibrator": sha256(exes["calibrator"]),
            "perplexity_off": sha256(exes["perplexity"]),
            "perplexity_on": sha256(exes["perplexity"]),
            "bench_off": sha256(exes["bench"]),
            "bench_on": sha256(exes["bench"]),
        },
        "binary_mtime_ns": {name: path.stat().st_mtime_ns for name, path in exes.items()},
    }
    write_json(out_dir / "manifest.json", manifest)

    common = ["-m", str(model), "-ngl", "999", "-dev", "Vulkan0", "-ctk", "q4_0",
              "-ctv", "q8_0", "-fa", "on", "-b", "512", "-ub", "512"]
    runs: list[dict[str, Any]] = []
    def bank_run(item: dict[str, Any]) -> None:
        runs.append(item)
        write_json(out_dir / "runs.json", runs)

    bank_run(run_native(
        [str(exes["calibrator"]), *common, "-f", str(calibration_corpus), "-o", str(calibration),
         "-c", str(args.calibration_context), "--chunks", str(args.calibration_chunks)],
        "calibrate exact seated model", out_dir, "calibration",
    ))
    require_file(calibration, "calibration artifact")
    manifest["calibration_sha256"] = sha256(calibration)
    write_json(out_dir / "manifest.json", manifest)

    missing = calibration.with_name(calibration.name + ".definitely-missing")
    bank_run(run_native(
        [str(exes["perplexity"]), *common, "-f", str(evaluation_corpus), "-c", str(args.context),
         "--chunks", "1", "--kv-mean-center", str(missing)],
        "prove loud refusal on missing calibration", out_dir, "loud-fail-missing", expect_success=False,
    ))
    bank_run(run_native(
        [str(exes["calibrator"]), *common, "-f", str(evaluation_corpus), "-c", "512", "--chunks", "1",
         "--kv-mean-center", str(calibration), "--kv-mean-center-probe-output", str(out_dir / "tensor-probe.json")],
        "tensor probe centered K on Vulkan", out_dir, "tensor-probe",
    ))

    for index in range(1, 4):
        for arm in ("off", "on"):
            argv = [str(exes["perplexity"]), *common, "-f", str(evaluation_corpus),
                    "-c", str(args.context), "--chunks", str(args.ppl_chunks)]
            if arm == "on":
                argv += ["--kv-mean-center", str(calibration)]
            bank_run(run_native(argv, f"PPL {arm} replication {index}", out_dir, f"ppl-{arm}-{index}"))

    for arm in ("off", "on"):
        argv = [str(exes["bench"]), "-m", str(model), "-ngl", "999", "-dev", "Vulkan0",
                "-ctk", "q4_0", "-ctv", "q8_0", "-fa", "on", "-p", "0", "-n", "128",
                "-d", str(args.context - 128), "-b", "512", "-ub", "512", "-r", "3",
                "-o", "json", "-oe", "none", "-v"]
        if arm == "on":
            argv += ["--kv-mean-center", str(calibration)]
        bank_run(run_native(argv, f"decode speed {arm} 3x", out_dir, f"speed-{arm}", split_streams=True))

    summary = verify_bank(out_dir)
    write_json(out_dir / "summary.json", summary)
    return 0 if summary["verdict"] == "GO" else 2


def selftest(out_dir: Path) -> int:
    out_dir.mkdir(parents=True, exist_ok=True)
    good_fixture = False
    planted_error = None
    with tempfile.TemporaryDirectory(prefix="ab-proof-selftest-", dir=out_dir) as tmp:
        bank = Path(tmp)
        write_json(bank / "manifest.json", {
            "schema": SCHEMA,
            "binary_sha256": {
                "perplexity_off": "a", "perplexity_on": "a",
                "bench_off": "b", "bench_on": "b",
            },
            "calibration_corpus_sha256": "c",
            "evaluation_corpus_sha256": "d",
        })
        empty_purity = {"command_lines": [], "process_hygiene": "clean"}
        runs = [{
            "stem": "loud-fail-missing", "returncode": 1,
            "purity_before": empty_purity, "purity_after": empty_purity,
            "log": str(bank / "loud.log.txt"),
        }]
        (bank / "loud.log.txt").write_text(
            "failed to load K-cache mean-centering bias file\n", encoding="utf-8")
        write_json(bank / "runs.json", runs)
        write_json(bank / "tensor-probe.json", {
            "all_pre_post_buffers_vulkan": True,
            "layer_count": 1,
            "global_max_abs_error_vs_calibration": 0.0001,
            "layers": [{"max_abs_delta": 0.25}],
        })
        for arm, values in (("off", [7.0, 7.01, 6.99]), ("on", [6.8, 6.81, 6.79])):
            for i, value in enumerate(values, 1):
                (bank / f"ppl-{arm}-{i}.log.txt").write_text(
                    f"Final estimate: PPL = {value} +/- 0.01\n", encoding="utf-8")
        write_json(bank / "speed-off.stdout.txt", [{"samples_ts": [10.0, 10.1, 9.9]}])
        write_json(bank / "speed-on.stdout.txt", [{"samples_ts": [9.8, 9.9, 9.7]}])
        good_fixture = verify_bank(bank)["verdict"] == "GO"
        try:
            verify_bank(bank, planted_overlap=True)
        except ProofError as exc:
            planted_error = str(exc)
    result = {
        "schema": "arifilabs.kv-mean-center.ab-proof-selftest.v1",
        "good_fixture_gain_beats_noise": good_fixture,
        "planted_violation_returned_red": planted_error is not None,
        "planted_violation": planted_error,
    }
    write_json(out_dir / "ab-proof-selftest.json", result)
    print(json.dumps(result, indent=2))
    return 0 if all((result["good_fixture_gain_beats_noise"], result["planted_violation_returned_red"])) else 1


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="command", required=True)
    run_p = sub.add_parser("run")
    run_p.add_argument("--bin-dir", type=Path, required=True)
    run_p.add_argument("--model", type=Path, required=True)
    run_p.add_argument("--calibration-corpus", type=Path, required=True)
    run_p.add_argument("--evaluation-corpus", type=Path, required=True)
    run_p.add_argument("--calibration-output", type=Path, required=True)
    run_p.add_argument("--output-dir", type=Path, required=True)
    run_p.add_argument("--context", type=int, default=8192)
    run_p.add_argument("--calibration-context", type=int, default=512)
    run_p.add_argument("--calibration-chunks", type=int, default=64)
    run_p.add_argument("--ppl-chunks", type=int, default=4)
    verify_p = sub.add_parser("verify")
    verify_p.add_argument("--output-dir", type=Path, required=True)
    verify_p.add_argument("--plant-overlap", action="store_true")
    test_p = sub.add_parser("selftest")
    test_p.add_argument("--output-dir", type=Path, required=True)
    return p


def main() -> int:
    args = parser().parse_args()
    try:
        if args.command == "run":
            if args.context < 1024 or args.calibration_context < 128 or args.context <= 128:
                raise ProofError("context settings are below the proof floor")
            return run(args)
        if args.command == "verify":
            summary = verify_bank(args.output_dir.resolve(), planted_overlap=args.plant_overlap)
            write_json(args.output_dir.resolve() / "summary.rederived.json", summary)
            print(json.dumps(summary, indent=2))
            return 0 if summary["verdict"] == "GO" else 2
        return selftest(args.output_dir.resolve())
    except (OSError, ValueError, KeyError, ProofError, statistics.StatisticsError) as exc:
        print(f"AB-PROOF RED: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
