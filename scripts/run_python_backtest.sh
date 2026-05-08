#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
PY_MODULE_DIR="${BUILD_DIR}/python"

file_size_bytes() {
    wc -c < "$1" | tr -d '[:space:]'
}

human_size() {
    awk -v bytes="$1" 'BEGIN {
        split("B KiB MiB GiB TiB", units, " ");
        size = bytes + 0;
        unit = 1;
        while (size >= 1024 && unit < 5) {
            size /= 1024;
            unit += 1;
        }
        if (unit == 1) {
            printf "%.0f %s", size, units[unit];
        } else {
            printf "%.2f %s", size, units[unit];
        }
    }'
}

usage() {
    cat <<EOF
Usage:
  scripts/run_python_backtest.sh <lob_csv> <trades_csv> <strategy_py> [strategy/backtest args...]

Examples:
  scripts/run_python_backtest.sh data/sample/lob.csv data/sample/trades.csv strategies/inventory_market_maker.py --strategy-config config/strategies/inventory_market_maker.yaml --progress-interval-events 10
  scripts/run_python_backtest.sh MD/lob.csv MD/trades.csv strategies/inventory_market_maker.py --strategy-config config/strategies/inventory_market_maker.yaml --report-path reports/python_base_report.md

Environment:
  PYTHON_BIN=/path/to/python  Override Python interpreter.

The strategy script must accept --lob-path and --trades-path.
EOF
}

if [[ $# -lt 3 ]]; then
    usage >&2
    exit 2
fi

LOB_PATH="$1"
TRADES_PATH="$2"
STRATEGY_SCRIPT="$3"
shift 3

if [[ ! -f "${ROOT_DIR}/${STRATEGY_SCRIPT}" && ! -f "${STRATEGY_SCRIPT}" ]]; then
    echo "Strategy script not found: ${STRATEGY_SCRIPT}" >&2
    exit 1
fi

if [[ ! -f "${ROOT_DIR}/${LOB_PATH}" && ! -f "${LOB_PATH}" ]]; then
    echo "LOB CSV not found: ${LOB_PATH}" >&2
    exit 1
fi

if [[ ! -f "${ROOT_DIR}/${TRADES_PATH}" && ! -f "${TRADES_PATH}" ]]; then
    echo "Trades CSV not found: ${TRADES_PATH}" >&2
    exit 1
fi

if [[ ! -d "${PY_MODULE_DIR}" ]]; then
    echo "Python module directory not found: ${PY_MODULE_DIR}" >&2
    echo "Build first: cmake -B build -S . && cmake --build build -j" >&2
    exit 1
fi

if [[ -n "${PYTHON_BIN:-}" ]]; then
    PYTHON="${PYTHON_BIN}"
else
    PYTHON="$(awk -F= '/^_Python3_EXECUTABLE:INTERNAL=/ {print $2; exit}' "${BUILD_DIR}/CMakeCache.txt" 2>/dev/null || true)"
    if [[ -z "${PYTHON}" ]]; then
        PYTHON="python3"
    fi
fi

cd "${ROOT_DIR}"

LOB_SIZE="$(file_size_bytes "${LOB_PATH}")"
TRADES_SIZE="$(file_size_bytes "${TRADES_PATH}")"

cat >&2 <<EOF
Python backtest launch
  strategy runner: ${STRATEGY_SCRIPT}
  lob data:        ${LOB_PATH} ($(human_size "${LOB_SIZE}"))
  trades data:     ${TRADES_PATH} ($(human_size "${TRADES_SIZE}"))
  python:          ${PYTHON}
  module path:     ${PY_MODULE_DIR}
  extra args:      ${*:-<none>}
EOF

PYTHONPATH="${ROOT_DIR}:${ROOT_DIR}/strategies:${PY_MODULE_DIR}${PYTHONPATH:+:${PYTHONPATH}}" \
    "${PYTHON}" "${STRATEGY_SCRIPT}" \
    --lob-path "${LOB_PATH}" \
    --trades-path "${TRADES_PATH}" \
    "$@"
