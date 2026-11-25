"""Summarize completed simulation windows from MarketReplay's order report."""

import argparse
import csv
from statistics import mean


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("orders")
    args = parser.parse_args()
    with open(args.orders, newline="") as file:
        orders = [row for row in csv.DictReader(file) if row["status"] == "expired"]

    print("model,orders,orders_with_fills,any_fill_rate,full_fill_rate,filled_shares,"
          "shares_on_partially_filled_orders,mean_first_fill_wait_us")
    for model in ("same_price", "fifo"):
        shares = [int(row[f"{model}_shares"]) for row in orders]
        full = sum(filled == int(row["size"]) for filled, row in zip(shares, orders))
        partial_shares = sum(filled for filled, row in zip(shares, orders)
                             if filled < int(row["size"]))
        waits = [int(row[f"{model}_first_wait_us"]) for filled, row in zip(shares, orders)
                 if filled]
        any_rate = f"{len(waits) / len(orders):.2%}" if orders else ""
        full_rate = f"{full / len(orders):.2%}" if orders else ""
        wait = f"{mean(waits):.1f}" if waits else ""
        print(model, len(orders), len(waits), any_rate, full_rate, sum(shares), partial_shares, wait,
              sep=",")


if __name__ == "__main__":
    main()
