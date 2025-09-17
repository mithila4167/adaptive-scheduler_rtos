#!/usr/bin/env python3
"""
AI priority adjuster (rules-based, lightweight)

- Reads metrics.csv periodically
- Uses simple rules to adjust task priorities:
  - If a task has waited "too long", boost its priority (lower number)
  - Slightly nudge based on remaining burst (optional)
- Writes new_priorities.csv atomically so the C scheduler can read it safely

Assumptions:
- metrics.csv has a header with at least:
  tick, task_id, waiting_time, remaining_time (or remaining_burst), cpu_usage, current_priority
- Lower priority number means higher priority
"""

import csv
import os
import time

METRICS_PATH = "metrics.csv"
OUTPUT_TMP = "new_priorities.tmp"
OUTPUT_CSV = "new_priorities.csv"

POLL_SECS = 0.1  # tighter polling for closer sync

# Priority bounds (tune to your system)
MIN_PRIO = 0
MAX_PRIO = 10

# Rule parameters (tune as needed)
WAIT_TOO_LONG = 5       # if waiting_time >= this, boost priority
WAIT_BOOST = -1         # decrease number (higher priority)
REMAINING_WEIGHT = 0.05 # small penalty per remaining burst unit

import os
DEBUG = os.getenv("AI_DEBUG", "0") == "1"

def dbg(msg):
    if DEBUG:
        print(f"[AI-DBG] {msg}")

def clamp(val, lo, hi):
    return max(lo, min(hi, val))

def load_rows(path):
    if not os.path.exists(path):
        return []
    with open(path, newline="") as f:
        return list(csv.DictReader(f))

def latest_tick_rows(rows):
    if not rows:
        return None, []
    ticks = [int(r["tick"]) for r in rows if r.get("tick", "").isdigit()]
    if not ticks:
        return None, []
    t = max(ticks)
    latest = [r for r in rows if r.get("tick", "").isdigit() and int(r["tick"]) == t]
    return t, latest

def parse_int(row, key, default=0):
    try:
        return int(row.get(key, default))
    except Exception:
        return default

def parse_float(row, key, default=0.0):
    try:
        return float(row.get(key, default))
    except Exception:
        return default

def compute_priorities(rows):
    prios = {}
    for r in rows:
        tid = parse_int(r, "task_id", -1)
        if tid < 0:
            continue
        remaining = parse_int(r, "remaining_time", parse_int(r, "remaining_burst", 0))
        waiting = parse_int(r, "waiting_time", 0)
        current_prio = parse_int(r, "current_priority", 5)
        queue_len = parse_int(r, "queue_len", 0)
        cpu_usage = parse_float(r, "cpu_usage", 0.0)

        delta = 0.0
        reason = []
        if waiting >= WAIT_TOO_LONG:
            delta += WAIT_BOOST
            reason.append(f"aging(wait={waiting}>= {WAIT_TOO_LONG} -> {WAIT_BOOST})")
        if REMAINING_WEIGHT != 0.0:
            adj = REMAINING_WEIGHT * remaining
            delta += adj
            reason.append(f"remain({remaining})*{REMAINING_WEIGHT}={adj:+.2f}")
        # Optionally react to congestion/cpu:
        # if queue_len > 5: delta += -0.5; reason.append("congest_boost")

        new_prio = int(round(current_prio + delta))
        new_prio = clamp(new_prio, MIN_PRIO, MAX_PRIO)
        prios[tid] = new_prio

        dbg(f"tick=? task={tid} cur={current_prio} wait={waiting} rem={remaining} qlen={queue_len} cpu={cpu_usage:.2f} -> delta={delta:+.2f} new={new_prio} reasons={'; '.join(reason) or 'none'}")
    return prios

import csv, os

def write_atomic_new_priorities(path_csv: str, tick: int, prios: dict[int, int]) -> None:
    tmp = path_csv + ".tmp"
    with open(tmp, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["tick", "task_id", "new_priority"])
        for tid in sorted(prios):
            w.writerow([tick, tid, prios[tid]])
        f.flush()
        try:
            os.fsync(f.fileno())
        except Exception:
            pass
    os.replace(tmp, path_csv)

def main():
    print("AI adjuster watching metrics.csv ...")
    last = -1
    while True:
        try:
            rows = load_rows(METRICS_PATH)
            t, latest = latest_tick_rows(rows)
            if t is not None and t > last and latest:
                pr = compute_priorities(latest)
                write_atomic_new_priorities(OUTPUT_CSV, t, pr)
                last = t
                dbg(f"wrote priorities for tick {t}: {pr}")
        except Exception as e:
            print(f"[AI] Error: {e}")
        time.sleep(POLL_SECS)

if __name__ == "__main__":
    main()
