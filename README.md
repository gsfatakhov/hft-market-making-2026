# CMF HFT Market-Making Backtester

## Directory structure

```
.
├── 3rdparty                    # place holder for 3rd party libraries (downloaded during the build)
├── build                       # local build tree used by CMake
├    ├── bin                    # generated binaries
├    ├── lib                    # generated libs (including those, which are built from 3rd party sources)
├    ├── cfg                    # generated config files (if any)
├    └── include                # generated include files (installed during the build for 3rd party sources)
├── cmake                       # cmake helper scripts
├── config                      # example config files
├── scripts                     # shell (and other) maintenance scripts
├── src                         # source files
├    ├── backtest               # backtest engine, IO, and sample strategies
├    ├── common                 # common utility files
├    ├── ...                    # ...
├    └── main                   # main() for hft-market-making app
├── test                        # unit-tests and other tests
├── CMakeLists.txt              # main build script
└── README.md                   # this README
```

## OS

Our primary platform is Linux, but nothing prevents it to be built and run on other OS.
The following commands are for Linux users.
Other users are encouraged to add the corresponding instructions for required steps in this README.

## Build

Install dependencies once:

```
sudo apt install -y cmake g++ clang-format
```

Build using cmake:

```
cmake -B build -S .
cmake --build build -j
```

or

```
mkdir -p build
pushd build
cmake ..
make -j VERBOSE=1
popd
```

## Test

To run unit tests:

```
ctest --test-dir build -j
```

or

```
pushd build
ctest -j
popd
```

or

```
build/bin/test/hft-market-making-tests
```

## Run

HFT market-making app:

```
build/bin/hft-market-making <config.cfg>
```

Sample backtest:

```
build/bin/hft-market-making config/sample_backtest.cfg
```

The config path is mandatory. The binary does not provide a default config, default dataset, or implicit strategy selection. For now, the supported C++ strategy is selected explicitly in cfg with `strategy.name = inventory_market_maker`. Use the Python helper script below when you want to run a Python strategy.

Python strategy example, using the same Python interpreter CMake found during configure:

```
PYTHONPATH=build/python /path/to/cmake-detected-python strategies/inventory_market_maker.py \
  --lob-path data/sample/lob.csv \
  --trades-path data/sample/trades.csv \
  --strategy-config config/strategies/inventory_market_maker.yaml
```

Python strategy via helper script:

```
scripts/run_python_backtest.sh MD/lob.csv MD/trades.csv strategies/inventory_market_maker.py \
  --strategy-config config/strategies/inventory_market_maker.yaml \
  --report-path reports/python_base_report.md

scripts/run_python_backtest.sh data/sample/lob.csv data/sample/trades.csv strategies/inventory_market_maker.py \
  --strategy-config config/strategies/inventory_market_maker.yaml \
  --progress-interval-events 10
```

The third positional argument is mandatory and must be the Python strategy runner. The helper intentionally does not provide a default strategy or default dataset, so an omitted argument fails fast instead of silently running the sample strategy. A custom runner must accept `--lob-path` and `--trades-path`.

The helper prints the Python runner, data paths, data sizes, Python executable, and forwarded args before launch. Reusable Python runner infrastructure lives in `strategies/strategy_app.py`; custom strategy runners can subclass `BacktestStrategyApp` to get common args, config loading, report writing, and the built-in C++ progress printer without copying it between strategies. `strategies/inventory_market_maker.py` also supports flat YAML strategy params with `--strategy-config config/strategies/inventory_market_maker.yaml`; CLI flags override values from that file.

The sample run uses `data/sample/lob.csv` and `data/sample/trades.csv`, then writes `reports/sample_report.md`. See `docs/backtester.md` for the engine architecture, fill model, and Python callback API.

The runner prints a tqdm-style progress line with percent, event counts, speed, elapsed time, and ETA. Configure it with `progress.enabled` and `progress.interval_events` in the cfg file.

## Contributing

Install UV, create a virtual environment, and install the project dependencies:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
uv sync
```

Then activate the virtual environment and set up the git pre-commit hooks:

```bash
source .venv/bin/activate
pre-commit install
```

After that, formatting and linting will run automatically before each commit.
If the source code does not meet the required formatting rules, the hook will
modify the files and stop the commit, and you will need to stage the updated
changes manually.

To run formatting and linting yourself, use one of these commands:

```bash
pre-commit run --files file.py
pre-commit run --all-files
```

The current pre-commit hooks do the following:
- format and lint C++ code with `clang-format`;
- format and lint Python code with `ruff`;
- strip outputs from Jupyter notebooks.
