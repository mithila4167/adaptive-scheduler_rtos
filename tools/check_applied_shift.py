import csv

m = list(csv.DictReader(open(r"C:\Users\thand\OneDrive\Desktop\rtosproj\rtos-sim\metrics.csv")))
p = list(csv.DictReader(open(r"C:\Users\thand\OneDrive\Desktop\rtosproj\rtos-sim\new_priorities.csv")))

pp = {(int(r["tick"]), int(r["task_id"])): int(r["new_priority"]) for r in p}

errors = 0
for r in m:
    t1 = int(r["tick"])          # metrics tick
    tid = int(r["task_id"])
    cur = int(r.get("current_priority", 5))
    want = pp.get((t1 - 1, tid))  # AI from previous tick
    if want is not None and cur != want:
        print(f"Mismatch: metrics_tick={t1} task={tid} cur={cur} ai_from_tick={t1-1} ai={want}")
        errors += 1
print("Done. mismatches:", errors)
