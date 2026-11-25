# C++ Limit Order Book and Market Replay

A C++17 limit order book and matching engine with Nasdaq TotalView-ITCH replay
and event-driven execution simulation.

- **Matching:** Price-time priority, partial and multi-level fills, and cancellation by order ID.
- **Replay:** Reuses the order book to reconstruct the best bid and ask and their displayed sizes after each historical event.
- **Execution research:** Simulates passive buy orders with fixed latency, then compares same-price fills with FIFO queue-aware fills.

## Quick start

Requires a C++17 compiler, CMake and Ninja. Run from the repository root in PowerShell:

```powershell
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
.\build-debug\OrderBook.exe test
ctest --test-dir build-debug --output-on-failure
.\build-debug\MarketReplay.exe replay\sample.txt
.\build-debug\MarketReplay.exe replay\fill_sample.txt --simulate 0 100 100
```

The examples use bundled synthetic events.

## AAPL results

**3,456 snapshots** across two Nasdaq AAPL premarket excerpts matched the
reference replay event by event. With 100-share orders, a 1-second lifetime,
and zero latency:

| Excerpt | Completed order windows | Same-price orders with fills | FIFO orders with fills |
| --- | ---: | ---: | ---: |
| 2019-01-30 | 353 | 20 | 1 |
| 2026-06-12 | 59 | 2 | 0 |

The [reproduction guide](docs/reproduction.md) covers the Nasdaq download,
snapshot comparison, in-memory replay benchmark, and full research results.
