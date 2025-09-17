import csv

# Load metrics and new priorities from the correct absolute paths
m = list(csv.DictReader(open(r"C:\Users\thand\OneDrive\Desktop\rtosproj\rtos-sim\metrics.csv")))
p = list(csv.DictReader(open(r"C:\Users\thand\OneDrive\Desktop\rtosproj\rtos-sim\new_priorities.csv")))

# Convert new priorities into a dictionary for easy lookup
pp = {(int(r["tick"]), int(r["task_id"])): int(r["new_priority"]) for r in p}

errors = 0
for r in m:
    t = int(r["tick"])
    tid = int(r["task_id"])
    cur = int(r.get("current_priority", 5))
    want = pp.get((t, tid))
    if want is not None and cur != want:
        print(f"Mismatch: tick={t} task={tid} cur={cur} ai={want}")
        errors += 1

print("Done. mismatches:", errors)
