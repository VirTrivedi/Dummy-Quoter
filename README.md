# Dummy-Quoter

A deliberately simple quoting strategy that replays recorded market data and writes the orders it would have sent. It exists to produce a baseline order stream for the [fill-simulator](../fill-simulator) and to sanity-check the data pipeline, not to make money.

## What it does

The quoter reads a `book_tops` file and a `book_fills` file for one symbol and merges them by timestamp with a priority queue. For each event:

- **Book top**: skips the update if either side is empty (`INT64_MAX`). Otherwise it joins the best bid and best ask, posting a post-only order at each price with the displayed top-level quantity. It places a new order only if it has none resting at that price, and forgets orders at prices that are no longer the top of book.
- **Book fill**: if the fill's resting order id matches the order it has tracked at that price, it drops that order from its tracking and logs the fill.

At the end it prints a PnL report. That PnL counts every order placed as fully filled at its limit price, then closes the remaining position at the last mid price.

## Build

Requires CMake 3.5.1+ and a C++14 compiler.

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

```bash
./dummy-quoter <book_tops_file> <book_fills_file> <output_orders_file>
```

The output directory is created if it does not exist. Progress is printed every 10,000 events, and non-monotonic timestamps are reported as warnings.

## File formats

All files are little-endian, packed binary (see `src/types/market_data_types.h`).

| File | Layout |
|---|---|
| Book tops (input) | 24-byte header (`feed_id`, `dateint`, `number_of_tops`, `symbol_idx`), then 88-byte `book_top_t` records: `ts`, `seqno`, and 3 levels of `bid_nanos`, `ask_nanos`, `bid_qty`, `ask_qty`. |
| Book fills (input) | 24-byte header, then 90-byte `book_fill_snapshot_t` records (trade price/qty, resting order id and side, book state at the time of the fill). |
| Orders (output) | Packed `Order` records, no header: `orderid`, `symbolid`, `sent_ts`, `md_ts`, `price_nanos`, `quantity`, `is_bid`, `is_post_only`. Prices are in nanos (1e-9 dollars). |

## Layout

```
src/main.cpp                  argument handling, event merge loop
src/quoter.{h,cpp}            quoting logic, order tracking, end-of-day PnL
src/book_tops_processor.*     feeds book tops to the quoter
src/book_fills_processor.*    feeds fills to the quoter
src/types/market_data_types.h binary record definitions
src/util/                     file and time helpers
```
