import argparse
import sys
from abc import ABC, abstractmethod
from pathlib import Path

import cmf_backtester as bt


class BacktestStrategyApp(ABC):
    strategy_name = "Strategy"
    default_strategy_params = {}

    def parse_args(self):
        parser = argparse.ArgumentParser(description=f"Run {self.strategy_name}.")
        parser.add_argument("--lob-path", required=True)
        parser.add_argument("--trades-path", required=True)
        parser.add_argument("--strategy-config", default="")
        parser.add_argument("--report-path", default="")
        parser.add_argument("--fee-rate", type=float, default=0.0)
        parser.add_argument("--slippage", type=float, default=0.0)
        parser.add_argument("--max-events", type=int, default=0)
        parser.add_argument("--progress-interval-events", type=int, default=100000)
        parser.add_argument("--no-progress", action="store_true")
        self.add_strategy_param_overrides(parser)
        return parser.parse_args()

    def add_strategy_param_overrides(self, parser):
        for name, default in self.default_strategy_params.items():
            parser.add_argument(
                f"--{name.replace('_', '-')}",
                dest=name,
                type=self.param_type(default),
            )

    def param_type(self, default):
        if isinstance(default, bool):
            return self.parse_bool
        if isinstance(default, int) and not isinstance(default, bool):
            return int
        if isinstance(default, float):
            return float
        return str

    def run(self):
        args = self.parse_args()
        strategy_params = self.resolved_strategy_params(args)
        self.print_launch_summary(args, strategy_params)
        strategy = self.create_strategy(strategy_params)
        report = bt.run(
            args.lob_path,
            args.trades_path,
            strategy,
            fee_rate=args.fee_rate,
            slippage=args.slippage,
            max_events=args.max_events,
            progress_interval_events=args.progress_interval_events,
            print_progress=not args.no_progress,
        )
        if args.report_path:
            self.write_report(args.report_path, report)
        print(
            f"pnl={report.pnl:.12f} "
            f"inventory={report.inventory:.2f} "
            f"turnover={report.turnover:.12f}"
        )
        return report

    @abstractmethod
    def create_strategy(self, strategy_params):
        raise NotImplementedError

    def resolved_strategy_params(self, args):
        params = dict(self.default_strategy_params)
        config_values = self.read_flat_yaml(args.strategy_config)
        unknown_keys = sorted(set(config_values) - set(params))
        if unknown_keys:
            raise ValueError(
                f"Unsupported {self.strategy_name} config keys: {', '.join(unknown_keys)}"
            )
        params.update(config_values)

        cli_values = {
            key: getattr(args, key)
            for key in self.default_strategy_params
            if getattr(args, key) is not None
        }
        params.update(cli_values)
        return params

    def print_launch_summary(self, args, strategy_params):
        print("Strategy launch", file=sys.stderr)
        print(f"  strategy:        {self.strategy_name}", file=sys.stderr)
        print(
            f"  strategy config: {args.strategy_config or '<built-in defaults + CLI>'}",
            file=sys.stderr,
        )
        print(
            f"  lob data:        {args.lob_path} ({self.human_size(self.file_size(args.lob_path))})",
            file=sys.stderr,
        )
        print(
            f"  trades data:     {args.trades_path} ({self.human_size(self.file_size(args.trades_path))})",
            file=sys.stderr,
        )
        print(f"  report path:     {args.report_path or '<not written>'}", file=sys.stderr)
        print(f"  fee/slippage:    {args.fee_rate} / {args.slippage}", file=sys.stderr)
        print(f"  max events:      {args.max_events}", file=sys.stderr)
        print(
            f"  progress:        {'off' if args.no_progress else f'every {args.progress_interval_events} events'}",
            file=sys.stderr,
        )
        print("  strategy params:", file=sys.stderr)
        for key in sorted(strategy_params):
            print(f"    {key}: {strategy_params[key]}", file=sys.stderr)

    @staticmethod
    def write_report(path, report):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            "# Python Backtest Performance Report\n\n"
            "| Metric | Value |\n"
            "| --- | ---: |\n"
            f"| PnL | {report.pnl:.12f} |\n"
            f"| Cash | {report.cash:.12f} |\n"
            f"| Inventory | {report.inventory:.12f} |\n"
            f"| Turnover | {report.turnover:.12f} |\n"
            f"| Traded quantity | {report.traded_quantity:.12f} |\n"
            f"| Fees | {report.fees:.12f} |\n"
            f"| Last mid price | {report.last_mid_price:.12f} |\n"
            f"| Orders placed | {report.orders_placed} |\n"
            f"| Orders cancelled | {report.orders_cancelled} |\n"
            f"| Fills | {report.fills} |\n"
            f"| Book events | {report.book_events} |\n"
            f"| Trade events | {report.trade_events} |\n"
            f"| Total events | {report.events} |\n"
        )

    @classmethod
    def read_flat_yaml(cls, path):
        values = {}
        if not path:
            return values

        path = Path(path)
        for line_no, raw_line in enumerate(path.read_text().splitlines(), start=1):
            line = raw_line.split("#", 1)[0].strip()
            if not line:
                continue
            if ":" not in line:
                raise ValueError(f"{path}:{line_no}: expected 'key: value'")
            key, value = line.split(":", 1)
            key = key.strip()
            value = value.strip()
            if not key:
                raise ValueError(f"{path}:{line_no}: empty key")
            values[key] = cls.parse_scalar(value)
        return values

    @classmethod
    def parse_scalar(cls, value):
        lowered = value.lower()
        if lowered in {"true", "yes", "on"}:
            return True
        if lowered in {"false", "no", "off"}:
            return False
        try:
            if any(marker in value for marker in (".", "e", "E")):
                return float(value)
            return int(value)
        except ValueError:
            return value.strip("\"'")

    @staticmethod
    def parse_bool(value):
        lowered = value.lower()
        if lowered in {"1", "true", "yes", "on"}:
            return True
        if lowered in {"0", "false", "no", "off"}:
            return False
        raise argparse.ArgumentTypeError(f"expected boolean, got {value}")

    @staticmethod
    def file_size(path):
        return Path(path).stat().st_size

    @staticmethod
    def human_size(size):
        units = ("B", "KiB", "MiB", "GiB", "TiB")
        value = float(size)
        unit = 0
        while value >= 1024 and unit < len(units) - 1:
            value /= 1024
            unit += 1
        if unit == 0:
            return f"{value:.0f} {units[unit]}"
        return f"{value:.2f} {units[unit]}"
