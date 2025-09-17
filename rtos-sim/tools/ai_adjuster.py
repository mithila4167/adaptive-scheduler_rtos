#!/usr/bin/env python3
import csv
import os
import time
from collections import defaultdict

METRICS_PATH = "metrics.csv"
OUTPUT_TMP = "new_priorities.tmp"
OUTPUT_CSV = "new_priorities.csv"

# Priority bounds
MIN_PRIO = 0
MAX_PRIO = 10

# Heuristic weights (tune these)
W_WAIT = -0.2   # more waiting => decrease number (higher priority)
W_REMAIN = 0.05 # longer remaining => slightly higher number (lower priority)
W_CONGEST = -0.5 # more queue => raise all priorities (lower numbers)
W_CPU = 0.2     # if CPU overutilized, relax priorities (raise numbers)

# Anti-starvation aging (applied if waiting exceeds this)
AGING_WAIT_THRESHOLD = 5
AGING_BOOST = -1  # one level higher (lower number)

POLL_SECS = 0.5

def clamp(val, lo, hi):
    return max(lo, min(hi, val))

def read_last_tick(rows):
    if not rows:
        return None, []
    last_tick = max(int(r["tick"]) for r in rows)
    last_rows = [r for r in rows if int(r["tick"]) == last_tick]
    return last_tick, last_rows

def load_metrics(path):
    if not os.path.exists(path):
        return []
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        return list(reader)

def compute_cpu_aggregate(last_rows):
    if not last_rows:
        return 0.0, 0
    cpu_usage = float(last_rows[0]["cpu_usage"])
    queue_len = int(last_rows[0]["queue_len"])
    return cpu_usage, queue_len

def adjust_priorities(last_rows):
    cpu_usage, queue_len = compute_cpu_aggregate(last_rows)
    new_prios = {}
    for r in last_rows:
        task_id = int(r["task_id"])
        cur_prio = int(r["current_priority"])
        remaining = int(r["remaining_time"])
        waiting = int(r["waiting_time"])

        delta = 0.0
        delta += W_WAIT * waiting
        delta += W_REMAIN * remaining
        delta += W_CONGEST * queue_len
        delta += W_CPU * cpu_usage

        if waiting >= AGING_WAIT_THRESHOLD:
            delta += AGING_BOOST

        new_prio = int(round(cur_prio + delta))
        new_prio = clamp(new_prio, MIN_PRIO, MAX_PRIO)
        new_prios[task_id] = new_prio
    return new_prios

def atomic_write_priorities(tick, prios, valid_until):
    with open(OUTPUT_TMP, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["tick", "task_id", "new_priority", "valid_until"])
        for task_id, p in sorted(prios.items()):
            writer.writerow([tick, task_id, p, valid_until])
    if os.path.exists(OUTPUT_CSV):
        try:
            os.replace(OUTPUT_TMP, OUTPUT_CSV)
        except Exception:
            try:
                os.remove(OUTPUT_CSV)
            except Exception:
                pass
            os.rename(OUTPUT_TMP, OUTPUT_CSV)
    else:
        os.rename(OUTPUT_TMP, OUTPUT_CSV)

def main():
    last_processed_tick = -1
    print("AI adjuster watching metrics.csv...")
    while True:
        try:
            rows = load_metrics(METRICS_PATH)
            tick, last_rows = read_last_tick(rows)
            if tick is not None and tick > last_processed_tick and last_rows:
                prios = adjust_priorities(last_rows)
                atomic_write_priorities(tick, prios, tick + 1)
                last_processed_tick = tick
                print(f"Produced priorities for tick {tick}: {prios}")
        except Exception as e:
            print(f"AI adjuster error: {e}")
        time.sleep(POLL_SECS)

if __name__ == "__main__":
    main()
