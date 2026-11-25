import argparse
import csv
from bisect import bisect_right
from collections import Counter

parser = argparse.ArgumentParser()
parser.add_argument("snapshots")
parser.add_argument("--horizon-us", type=int, default=1000)
args = parser.parse_args()
if args.horizon_us <= 0:
    parser.error("--horizon-us must be positive")

with open(args.snapshots, newline="") as snapshots:
    # Multiple events may share a timestamp. Keep the final state at that time.
    rows = {int(row["timestamp_us"]): row for row in csv.DictReader(snapshots)}

times = sorted(rows)
results = {label: Counter() for label in ("sell-heavy", "balanced", "buy-heavy")}
for now in times:
    target = now + args.horizon_us
    if target > times[-1]:
        break
    current = rows[now]
    future = rows[times[bisect_right(times, target) - 1]]
    if not all(row["bid"] and row["ask"] for row in (current, future)):
        continue
    bid_size = int(current["bid_qty"])
    ask_size = int(current["ask_qty"])
    imbalance = (bid_size - ask_size) / (bid_size + ask_size)
    category = "balanced"
    if imbalance <= -0.2:
        category = "sell-heavy"
    elif imbalance >= 0.2:
        category = "buy-heavy"

    # Compare twice the midprice to stay in integer ticks.
    change = sum(int(future[side]) - int(current[side]) for side in ("bid", "ask"))
    direction = "up" if change > 0 else "down" if change < 0 else "flat"
    results[category][direction] += 1

total = sum(results.values(), Counter())
print("bucket,n,up,flat,down,up_share,flat_share,down_share")
for category, counts in [*results.items(), ("all", total)]:
    count = sum(counts.values())
    directions = ("up", "flat", "down")
    shares = [f"{counts[direction] / count:.1%}" if count else "" for direction in directions]
    print(category, count, *(counts[direction] for direction in directions), *shares, sep=",")
