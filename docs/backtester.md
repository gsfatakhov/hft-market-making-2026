# Backtester Technical Documentation

## Architecture

The backtester is an event-driven C++20 engine organized under `src/backtest`:

- `io/`: CSV market-data readers.
- `core/`: replay engine, book state, exchange simulation, accounting, and strategy interface.
- `strategy/`: built-in sample strategies.

`BookCsvReader` and `TradeCsvReader` stream the current CSV schemas, `LimitOrderBook` stores the latest snapshot, `SimulatedExchange` owns strategy orders and accounting, and `BacktestEngine` merges book/trade events by `local_timestamp`.

Book snapshots update strategy state and mark-to-market valuation. Trades drive simulated fills. At identical timestamps, book snapshots are processed before trades for deterministic replay. For a trade event, fills on orders resting before that event are emitted before `on_trade`, preventing a strategy from seeing a trade and placing an order that fills on the same trade.

## Execution Model

`trades.csv` `side` is treated as aggressor side. Resting buy orders fill from sell-aggressor trades at or below the order limit. Resting sell orders fill from buy-aggressor trades at or above the order limit. Fill quantity is capped by both the observed trade amount and the order's remaining quantity.

When multiple own orders are eligible, price priority is applied first, then FIFO by placement sequence for same-price orders. V1 executes at the resting limit price. Fees and slippage are configurable and default to zero.

## Strategy API

Python strategies are passed to `cmf_backtester.run(lob_path, trades_path, strategy, fee_rate=0.0, slippage=0.0, max_events=0, progress_interval_events=100000, progress_callback=None, print_progress=False)`. Data paths and strategy runners should be passed explicitly; the project helper scripts intentionally avoid default strategy/data substitution. A strategy can implement any of these optional callbacks:

```python
on_start(ctx)
on_book(ctx, book)
on_trade(ctx, trade)
on_fill(ctx, fill)
on_finish(ctx, report)
```

The context exposes `place_limit(side, price, quantity)`, `cancel(order_id)`, `inventory`, `cash`, and `open_orders()`. The sample in `strategies/inventory_market_maker.py` quotes both sides around mid, cancels/replaces stale quotes, and stops quoting the side that would breach inventory limits.

Reusable Python runner infrastructure is in `strategies/strategy_app.py`. Strategy scripts can subclass `BacktestStrategyApp` and implement `create_strategy(strategy_params)` to inherit common CLI args, launch summary printing, built-in C++ progress output, flat YAML loading, and report writing.

Python strategy parameters can be passed through CLI flags or a flat YAML file such as `config/strategies/inventory_market_maker.yaml`:

```yaml
order_quantity: 1000
inventory_limit: 5000
half_spread: 0.00000005
requote_threshold: 0.000000005
skew_per_unit: 0.0
```

The sample parser intentionally supports only simple `key: value` files, so the repo does not need a runtime YAML/Hydra dependency yet. This can be replaced with Hydra/OmegaConf later if strategy configuration becomes nested or compositional.

Python callers can pass `print_progress=True` to use the built-in C++ progress line, or pass `progress_callback=` for custom reporting. The callback receives `BacktestProgress` with `events`, `book_events`, `trade_events`, `percent`, `elapsed_seconds`, `eta_seconds`, and `finished`.

## Progress Output

The C++ runner prints a tqdm-style progress line to stderr when `progress.enabled = true`:

```text
[==========>                   ] 35.2% events=1200000 book=54321 trades=1145679 845000 ev/s elapsed=1s eta=2s
```

Progress is estimated from CSV bytes read across the LOB and trades files. `progress.interval_events` controls how often the line is refreshed. The Python `BacktestStrategyApp` helper passes `print_progress=True` unless `--no-progress` is set. For full datasets, use a larger interval such as `100000`; for tiny samples, use a small interval such as `10`.

## Build And Run

The C++ runner also requires an explicit config path. It does not substitute a sample config or sample dataset when arguments are omitted. The config must include `strategy.name`; currently the supported C++ strategy name is `inventory_market_maker`.

```bash
cmake -B build -S .
cmake --build build -j
ctest --test-dir build --output-on-failure -j
build/bin/hft-market-making config/sample_backtest.cfg
PYTHONPATH=build/python /path/to/cmake-detected-python strategies/inventory_market_maker.py --lob-path data/sample/lob.csv --trades-path data/sample/trades.csv --strategy-config config/strategies/inventory_market_maker.yaml
scripts/run_python_backtest.sh data/sample/lob.csv data/sample/trades.csv strategies/inventory_market_maker.py --strategy-config config/strategies/inventory_market_maker.yaml --progress-interval-events 10
```

Use the same Python interpreter shown by CMake during configure, because the extension module is built for that Python ABI. The C++ runner writes `reports/sample_report.md`. The large local `MD/` directory is ignored; committed sample runs use the small `data/sample` slice.
