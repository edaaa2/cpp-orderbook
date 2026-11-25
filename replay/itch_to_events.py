"""Convert one symbol from a length-prefixed Nasdaq TotalView-ITCH 5.0 file."""
import argparse
import gzip
import sys


LENGTHS = {b"A": 36, b"F": 40, b"E": 31, b"C": 36,
           b"X": 23, b"D": 19, b"U": 35}


def number(message, start, width):
    return int.from_bytes(message[start:start + width], "big")


def convert(source, symbol, max_messages=None):
    orders = {}
    matched = False
    processed = 0
    while max_messages is None or processed < max_messages:
        prefix = source.read(2)
        if not prefix:
            break
        if len(prefix) != 2:
            raise ValueError("Truncated message length")
        length = int.from_bytes(prefix, "big")
        message = source.read(length)
        if not length or len(message) != length:
            raise ValueError("Invalid or truncated message")
        processed += 1
        kind = message[:1]
        if kind not in LENGTHS:
            continue
        if length != LENGTHS[kind]:
            raise ValueError(f"Invalid length for message type {kind!r}")

        timestamp = number(message, 5, 6) // 1000
        order_id = number(message, 11, 8)
        if kind in (b"A", b"F"):
            if message[24:32].rstrip(b" ") != symbol:
                continue
            matched = True
            side = {b"B": "B", b"S": "S"}[message[19:20]]
            quantity = number(message, 20, 4)
            price = number(message, 32, 4)
            orders[order_id] = (side, quantity)
            print(timestamp, "A", order_id, side, price, quantity)
        elif order_id in orders:
            side, remaining = orders[order_id]
            if kind == b"U":
                new_id = number(message, 19, 8)
                quantity = number(message, 27, 4)
                price = number(message, 31, 4)
                del orders[order_id]
                orders[new_id] = (side, quantity)
                print(timestamp, "C", order_id, remaining)
                print(timestamp, "A", new_id, side, price, quantity)
                continue

            quantity = remaining if kind == b"D" else number(message, 19, 4)
            if not 0 < quantity <= remaining:
                raise ValueError(f"Invalid reduction for order {order_id}")
            action = "E" if kind in (b"E", b"C") else "C"
            print(timestamp, action, order_id, quantity)
            if quantity == remaining:
                del orders[order_id]
            else:
                orders[order_id] = (side, remaining - quantity)

    if not matched:
        raise ValueError("No add-order messages found for this symbol")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", help="Historical ITCH file, plain or .gz")
    parser.add_argument("symbol", help="One stock symbol, for example AAPL")
    parser.add_argument("--max-messages", type=int,
                        help="Read at most this many raw ITCH messages from the start")
    args = parser.parse_args()
    if args.max_messages is not None and args.max_messages <= 0:
        parser.error("--max-messages must be positive")
    opener = gzip.open if args.input.lower().endswith(".gz") else open
    try:
        with opener(args.input, "rb") as source:
            convert(source, args.symbol.upper().encode("ascii"), args.max_messages)
    except (OSError, EOFError, ValueError, KeyError) as error:
        sys.exit(f"ITCH conversion failed: {error}")


if __name__ == "__main__":
    main()
