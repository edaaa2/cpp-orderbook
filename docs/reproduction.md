# AAPL replay and execution simulation

Build the Debug targets using the [quick start](../README.md#quick-start), then run these commands from the repository root. The research scripts require Python 3.8 or newer.

## Market replay and 1 ms analysis

`MarketReplay` reconstructs the visible book for one symbol using the same
`BookSide` as the matching engine. It adds resting orders directly and applies
cancel and execution events by order ID. Each event produces a snapshot with
the best bid, best ask, and total resting quantity at each best price.

After the Debug build above, run the included synthetic example in PowerShell:

```powershell
.\build-debug\MarketReplay.exe replay\sample.txt | Set-Content -Encoding ascii build-debug\snapshots.csv
python replay\analyze.py build-debug\snapshots.csv --horizon-us 1000
```

Replay input uses integer price ticks and nondecreasing microsecond timestamps.
Each line is either `timestamp_us A order_id B|S price_ticks quantity`,
`timestamp_us C order_id quantity`, or `timestamp_us E order_id quantity`.
Events with a shared timestamp are replayed in input order.

For an extracted AAPL event file, use its path in place of `replay\sample.txt`.
To extract events from a length-prefixed Nasdaq TotalView-ITCH 5.0 file with
Python 3.8 or newer:

```powershell
python replay\itch_to_events.py FILE.NASDAQ_ITCH50.gz AAPL | Set-Content -Encoding ascii build-debug\aapl.events
.\build-debug\MarketReplay.exe build-debug\aapl.events | Set-Content -Encoding ascii build-debug\aapl.csv
python replay\analyze.py build-debug\aapl.csv --horizon-us 1000
```

The converter handles visible adds, executions, partial cancellations,
deletions, and replacements. A replacement becomes a full cancellation and a
new add at the same timestamp. ITCH prices retain their four decimal places
as integer ticks, and timestamps are truncated to microseconds.

The analysis uses the last snapshot at each timestamp. It compares its
midprice with the latest snapshot at or before the 1 ms target. Both snapshots
must have two-sided quotes, and the target must fall within the data window.
The imbalance is `(bid_qty - ask_qty) / (bid_qty + ask_qty)`. Values of
`-0.2` or lower are sell-heavy, and values of `0.2` or higher are buy-heavy.
The script reports the sample count and up, flat, and down shares for each
group and for all eligible observations. These are descriptive statistics.
Observation windows can overlap.

### AAPL replay validation (June 12, 2026)

The first 2,000,000 raw messages from Nasdaq's June 12, 2026 sample yield
**2,018 AAPL events**. All 2,018 event snapshots matched the original replay
byte for byte. The excerpt spans about 9 minutes and 18 seconds starting at
04:00, before the regular trading session. The generated event file has
SHA-256 `e07015ad74a364a1b0f5c166ae7a20ce5b22f22082264f0ca4e2c5492d08dec7`.

```powershell
$url = 'https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/S061226-v50.txt.gz'
curl.exe --fail --location --range 0-67108863 --max-filesize 67108864 --output build-debug\nasdaq-20260612-prefix.gz $url
if ($LASTEXITCODE -ne 0) { throw 'Nasdaq download failed' }
$events = python replay\itch_to_events.py build-debug\nasdaq-20260612-prefix.gz AAPL --max-messages 2000000
if ($LASTEXITCODE -ne 0) { throw 'ITCH conversion failed' }
$output = Join-Path (Get-Location) 'build-debug\aapl-20260612.events'
[IO.File]::WriteAllText($output, (($events -join "`n") + "`n"), [Text.Encoding]::ASCII)
Get-FileHash $output -Algorithm SHA256
.\build-debug\MarketReplay.exe $output | Set-Content -Encoding ascii build-debug\aapl-20260612.csv
if ($LASTEXITCODE -ne 0) { throw 'Market replay failed' }
python replay\analyze.py build-debug\aapl-20260612.csv --horizon-us 1000
```

| Initial imbalance | Eligible observations | Midprice up | Flat | Down |
| --- | ---: | ---: | ---: | ---: |
| Sell-heavy | 772 | 56 (7.3%) | 694 (89.9%) | 22 (2.8%) |
| Balanced | 919 | 16 (1.7%) | 898 (97.7%) | 5 (0.5%) |
| Buy-heavy | 301 | 8 (2.7%) | 280 (93.0%) | 13 (4.3%) |
| All | 1,992 | 80 (4.0%) | 1,872 (94.0%) | 40 (2.0%) |

#### January 30, 2019 cross-check

All **1,438** AAPL event snapshots matched the original replay. The buy-heavy
group had the highest 1 ms midprice up share in this excerpt (3.1%), while the
sell-heavy group led in the June 2026 excerpt (7.3%).

To regenerate the 2019 input, use the commands above with
[Nasdaq's January 30, 2019 file](https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/01302019.NASDAQ_ITCH50.gz)
and `--max-messages 2870506`. Use distinct output filenames for the two
dates. The first 64 MiB are sufficient. The generated event file has
SHA-256 `a1659101f4a4858258b0685f91bd20ab2efd4225326f768b119335ca2032396d`.

## Event-driven execution simulation

`MarketReplay --simulate` submits a simulated passive buy when top-of-book
imbalance is at least 0.2. It posts at the best bid **at arrival**
after a fixed latency and cancels the remainder at the configured lifetime.
The simulator holds one pending or active order at a time. Both fill models
observe the same order windows, including when one model fills early.
Historical orders continue to update independently.

| Fill model | Rule |
| --- | --- |
| Same price | Credit visible executions against historical bids at the simulated limit, capped at the simulated order size. |
| FIFO | Record the resting order IDs at the arrival price. Executions and cancellations of those IDs reduce the queue ahead. Credit executions against later orders at that price once the queue ahead is empty. Cancellations never count as fills. |

The strategy checks the book after each event. Zero-latency orders enter
after the triggering event. Positive-latency arrivals and expirations are
processed before historical events with the same timestamp. An order needs
a two-sided, uncrossed book at arrival. Prices stay fixed after posting.
The model assumes the simulated order has no market impact and uses only
visible executions at its limit. Waiting times are measured from arrival
among filled orders. The summary uses orders with a full observation
window and excludes rows marked `end_of_feed`.

Check the fill logic with the synthetic queue example:

```powershell
.\build-debug\MarketReplay.exe replay\fill_sample.txt --simulate 0 100 100
```

A 100-share order arrives behind 500 shares. The same-price model fills
all 100 shares on the first 200-share execution at 10 µs. In the FIFO model
those 200 shares go to the queue ahead. A cancellation clears the remaining
300 ahead, and the next two executions fill 70 simulated shares beginning
at 30 µs.

Run `ctest --test-dir build-debug --output-on-failure` to check the expected
fill reports automatically. The regression also verifies that a delayed order
uses the new best bid and excludes executions at expiry.

For the cross-date comparison, generate both event files using the Nasdaq
instructions above. Save the January 2019 events as
`build-debug\aapl-from-nasdaq.events`:

```powershell
foreach ($events in 'aapl-from-nasdaq', 'aapl-20260612') {
    foreach ($latency in 0, 1000) {
        $report = "build-debug\fills-$events-$latency.csv"
        .\build-debug\MarketReplay.exe "build-debug\$events.events" --simulate $latency 1000000 100 | Set-Content -Encoding ascii $report
        if ($LASTEXITCODE -ne 0) { throw 'Execution simulation failed' }
        python replay\summarize_fills.py $report
    }
}
```

| Excerpt | Latency | Model | Complete windows | Orders with fills | Filled shares | Mean first-fill wait |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| 2019-01-30 | 0 µs | Same price | 353 | 20 (5.67%) | 1,470 | 320.1 ms |
| 2019-01-30 | 0 µs | FIFO | 353 | 1 (0.28%) | 100 | 565.3 ms |
| 2019-01-30 | 1,000 µs | Same price | 353 | 22 (6.23%) | 1,654 | 321.9 ms |
| 2019-01-30 | 1,000 µs | FIFO | 353 | 0 (0%) | 0 | — |
| 2026-06-12 | 0 µs | Same price | 59 | 2 (3.39%) | 51 | 489.2 ms |
| 2026-06-12 | 0 µs | FIFO | 59 | 0 (0%) | 0 | — |
| 2026-06-12 | 1,000 µs | Same price | 59 | 2 (3.39%) | 51 | 488.2 ms |
| 2026-06-12 | 1,000 µs | FIFO | 59 | 0 (0%) | 0 | — |

The FIFO model produced fewer simulated fills in both premarket excerpts.
Only one FIFO order filled across these runs, so the latency comparison
is descriptive.

## Verify snapshots against the reference

`ReferenceReplay` is the original standalone prototype. It uses an order-ID
map and aggregate quantities by price, with no dependency on `BookSide`.
After generating both AAPL event files, run:

```powershell
python replay\compare_snapshots.py .\build-debug\MarketReplay.exe .\build-debug\ReferenceReplay.exe build-debug\aapl-from-nasdaq.events build-debug\aapl-20260612.events
```

The script compares timestamps, both best prices and their displayed sizes
after every event. It reports the first differing row and exits with a
nonzero status on a mismatch. Expected total: 3,456 matching snapshots.
Use `replay\sample.txt` for a quick check without downloading data.

## In-memory replay benchmark

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
.\build-release\MarketReplay.exe build-debug\aapl-from-nasdaq.events --benchmark
.\build-release\MarketReplay.exe build-debug\aapl-20260612.events --benchmark
```

The benchmark parses the events before timing. Each pass starts with an empty
book and measures applying each event, querying both best quotes and their
aggregate quantities, and storing the results in a preallocated buffer.
File I/O, parsing, output formatting, book teardown and the checksum are
outside the timed region. It reports the median of 201 passes after one warm-up.

| AAPL excerpt | Events per pass | Median pass | Events/s |
| --- | ---: | ---: | ---: |
| 2019-01-30 | 1,438 | 170.00 us | 8.46 million |
| 2026-06-12 | 2,018 | 489.90 us | 4.12 million |

Measured on Windows with an Intel Core Ultra 5 125H, MSYS2 GCC 16.2.0, and
Release flags `-O3 -DNDEBUG`. These are throughput measurements for repeated
passes over short premarket excerpts.
