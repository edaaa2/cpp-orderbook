"""Compare every CSV snapshot from the market replay and independent reference."""

import argparse
from itertools import zip_longest
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("market_replay")
    parser.add_argument("reference_replay")
    parser.add_argument("events", nargs="+")
    args = parser.parse_args()

    total = 0
    for events in args.events:
        actual = subprocess.check_output([args.market_replay, events], text=True).splitlines()
        expected = subprocess.check_output([args.reference_replay, events], text=True).splitlines()
        for line, (got, want) in enumerate(zip_longest(actual, expected), start=1):
            if got != want:
                location = "header" if line == 1 else f"event {line - 1}"
                parser.exit(1, f"{events}: {location} differs\n"
                            f"  MarketReplay: {got!r}\n  ReferenceReplay: {want!r}\n")
        count = len(expected) - 1
        total += count
        print(f"{events}: {count:,} matching snapshots")
    print(f"Total: {total:,} matching snapshots")


if __name__ == "__main__":
    main()
