#!/usr/bin/env python3
import argparse
import re
import statistics
import sys
from pathlib import Path

PATTERN = re.compile(
    r"detectValidRatio=(?P<detect>[0-9.]+).*?"
    r"unityReturnRatio=(?P<unity>[0-9.]+).*?"
    r"lockSwitchRateHz=(?P<lock>[0-9.]+).*?"
    r"trackingLostRatio=(?P<lost>[0-9.]+).*?"
    r"largeRatioStepRatio=(?P<step>[0-9.]+).*?"
    r"avgAppliedCents=(?P<applied>[0-9.]+).*?"
    r"avgInputRms=(?P<rms>[0-9.]+).*?"
    r"speedNorm=(?P<speed>[0-9.]+)"
)


def parse_diag(path: Path):
    rows = []
    for line in path.read_text(errors="ignore").splitlines():
        if "Nova Pitch DIAG" not in line:
            continue
        m = PATTERN.search(line)
        if not m:
            continue
        row = {k: float(v) for k, v in m.groupdict().items()}
        rows.append(row)
    return rows


def pctl(values, q):
    if not values:
        return 0.0
    s = sorted(values)
    idx = int(round((len(s) - 1) * q))
    return s[max(0, min(len(s) - 1, idx))]


def main():
    ap = argparse.ArgumentParser(description="Nova Pitch DIAG regression gate")
    ap.add_argument("diag_file", type=Path, help="Path to nova_pitch_diag.log")
    ap.add_argument("--hard-speed-min", type=float, default=0.90)
    ap.add_argument("--max-tracking-lost-p95", type=float, default=0.35)
    ap.add_argument("--max-large-step-p95", type=float, default=0.05)
    ap.add_argument("--max-lock-switch-p95", type=float, default=0.35)
    ap.add_argument("--min-applied-cents-median", type=float, default=15.0)
    ap.add_argument("--min-detect-valid-median", type=float, default=0.60)
    args = ap.parse_args()

    rows = parse_diag(args.diag_file)
    hard = [r for r in rows if r["speed"] >= args.hard_speed_min]

    if len(hard) < 8:
        print(f"FAIL: not enough hard-mode DIAG rows ({len(hard)} found, need >= 8)")
        sys.exit(2)

    # Exclude true silence windows from quality metrics.
    voiced = [r for r in hard if r["rms"] >= 0.004]
    if len(voiced) < 6:
        print(f"FAIL: not enough voiced hard-mode rows ({len(voiced)} found, need >= 6)")
        sys.exit(2)

    detect_med = statistics.median(r["detect"] for r in voiced)
    applied_med = statistics.median(r["applied"] for r in voiced)
    lost_p95 = pctl([r["lost"] for r in voiced], 0.95)
    step_p95 = pctl([r["step"] for r in voiced], 0.95)
    lock_p95 = pctl([r["lock"] for r in voiced], 0.95)

    failures = []
    if detect_med < args.min_detect_valid_median:
        failures.append(
            f"detectValid median too low: {detect_med:.3f} < {args.min_detect_valid_median:.3f}"
        )
    if applied_med < args.min_applied_cents_median:
        failures.append(
            f"avgAppliedCents median too low: {applied_med:.1f} < {args.min_applied_cents_median:.1f}"
        )
    if lost_p95 > args.max_tracking_lost_p95:
        failures.append(
            f"trackingLostRatio p95 too high: {lost_p95:.3f} > {args.max_tracking_lost_p95:.3f}"
        )
    if step_p95 > args.max_large_step_p95:
        failures.append(
            f"largeRatioStepRatio p95 too high: {step_p95:.3f} > {args.max_large_step_p95:.3f}"
        )
    if lock_p95 > args.max_lock_switch_p95:
        failures.append(
            f"lockSwitchRateHz p95 too high: {lock_p95:.3f} > {args.max_lock_switch_p95:.3f}"
        )

    print("Nova Pitch DIAG gate summary")
    print(f"rows_hard={len(hard)} voiced_hard={len(voiced)}")
    print(f"detect_median={detect_med:.3f}")
    print(f"applied_cents_median={applied_med:.1f}")
    print(f"tracking_lost_p95={lost_p95:.3f}")
    print(f"large_step_p95={step_p95:.3f}")
    print(f"lock_switch_p95={lock_p95:.3f}")

    if failures:
        print("FAIL")
        for f in failures:
            print(f"- {f}")
        sys.exit(1)

    print("PASS")


if __name__ == "__main__":
    main()
