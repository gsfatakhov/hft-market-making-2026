import csv
import math
from collections import defaultdict
from pathlib import Path

import cmf_backtester as bt

from strategy_app import BacktestStrategyApp


class MicropriceEstimator:
    def __init__(
        self,
        calibration_lob_path="",
        imbalance_bins=10,
        max_spread_bins=5,
        tick_size=0.0,
        min_state_observations=10,
        calibration_max_rows=0,
        microprice_tolerance=1e-12,
        microprice_max_iterations=100,
    ):
        self.imbalance_bins = self._require_positive_int(
            "imbalance_bins", imbalance_bins
        )
        self.max_spread_bins = self._require_positive_int(
            "max_spread_bins", max_spread_bins
        )
        self._require_non_negative("tick_size", tick_size)
        self.min_state_observations = self._require_non_negative_int(
            "min_state_observations", min_state_observations
        )
        self.calibration_max_rows = self._require_non_negative_int(
            "calibration_max_rows", calibration_max_rows
        )
        self._require_positive("microprice_tolerance", microprice_tolerance)
        self.microprice_tolerance = microprice_tolerance
        self.microprice_max_iterations = self._require_positive_int(
            "microprice_max_iterations", microprice_max_iterations
        )

        self.tick_size = tick_size
        self.adjustments = {}
        self.state_observations = defaultdict(int)
        if calibration_lob_path:
            self.calibrate(calibration_lob_path)

    def reference_from_book(self, book):
        best_bid = book.bids[0]
        best_ask = book.asks[0]
        return self.reference_from_values(
            best_bid.price, best_bid.quantity, best_ask.price, best_ask.quantity
        )

    def reference_from_values(self, bid_price, bid_quantity, ask_price, ask_quantity):
        mid = (bid_price + ask_price) / 2.0
        fallback = self.weighted_mid(
            bid_price, bid_quantity, ask_price, ask_quantity
        )
        if not self.adjustments or self.tick_size <= 0.0:
            return fallback

        state = self.state_for_values(
            bid_price, bid_quantity, ask_price, ask_quantity
        )
        if self.state_observations[state] < self.min_state_observations:
            return fallback
        return mid + self.adjustments.get(state, 0.0)

    def weighted_mid(self, bid_price, bid_quantity, ask_price, ask_quantity):
        total_quantity = bid_quantity + ask_quantity
        if total_quantity <= 0.0:
            return (bid_price + ask_price) / 2.0
        imbalance = bid_quantity / total_quantity
        return imbalance * ask_price + (1.0 - imbalance) * bid_price

    def state_for_values(self, bid_price, bid_quantity, ask_price, ask_quantity):
        total_quantity = bid_quantity + ask_quantity
        imbalance = 0.5
        if total_quantity > 0.0:
            imbalance = bid_quantity / total_quantity
        spread = ask_price - bid_price
        return self.state_for_imbalance_spread(imbalance, spread)

    def state_for_imbalance_spread(self, imbalance, spread):
        clipped_imbalance = min(max(imbalance, 0.0), 1.0)
        imbalance_bin = min(
            int(clipped_imbalance * self.imbalance_bins),
            self.imbalance_bins - 1,
        )

        if self.tick_size <= 0.0:
            spread_bin = 1
        else:
            spread_bin = int(round(spread / self.tick_size))
            spread_bin = min(max(spread_bin, 1), self.max_spread_bins)
        return imbalance_bin, spread_bin

    def calibrate(self, calibration_lob_path):
        path = Path(calibration_lob_path)
        if not path.is_file():
            raise ValueError(f"calibration_lob_path does not exist: {path}")

        rows = self._read_lob_rows(path)
        if len(rows) < 2:
            raise ValueError("calibration_lob_path must contain at least two LOB rows")

        if self.tick_size == 0.0:
            self.tick_size = self._infer_tick_size(rows)

        observations = []
        for current, next_row in zip(rows, rows[1:]):
            current_state = self.state_for_imbalance_spread(
                current["imbalance"], current["spread"]
            )
            next_state = self.state_for_imbalance_spread(
                next_row["imbalance"], next_row["spread"]
            )
            mid_change = next_row["mid"] - current["mid"]
            observations.append((current_state, next_state, mid_change))
            observations.append(
                (
                    self._symmetric_state(current_state),
                    self._symmetric_state(next_state),
                    -mid_change,
                )
            )

        self.adjustments = self._estimate_adjustments(observations)

    def _read_lob_rows(self, path):
        rows = []
        with path.open(newline="") as stream:
            reader = csv.DictReader(stream)
            required = {
                "asks[0].price",
                "asks[0].amount",
                "bids[0].price",
                "bids[0].amount",
            }
            missing = sorted(required - set(reader.fieldnames or []))
            if missing:
                raise ValueError(f"{path} is missing columns: {', '.join(missing)}")

            for raw_row in reader:
                bid_price = float(raw_row["bids[0].price"])
                bid_quantity = float(raw_row["bids[0].amount"])
                ask_price = float(raw_row["asks[0].price"])
                ask_quantity = float(raw_row["asks[0].amount"])
                if bid_price <= 0.0 or ask_price <= 0.0 or ask_price <= bid_price:
                    continue
                total_quantity = bid_quantity + ask_quantity
                if total_quantity <= 0.0:
                    continue

                rows.append(
                    {
                        "bid": bid_price,
                        "ask": ask_price,
                        "mid": (bid_price + ask_price) / 2.0,
                        "imbalance": bid_quantity / total_quantity,
                        "spread": ask_price - bid_price,
                    }
                )
                if self.calibration_max_rows and len(rows) >= self.calibration_max_rows:
                    break
        return rows

    def _infer_tick_size(self, rows):
        prices = sorted({row["bid"] for row in rows} | {row["ask"] for row in rows})
        price_diffs = [
            right - left
            for left, right in zip(prices, prices[1:])
            if right - left > 0.0
        ]
        if price_diffs:
            return min(price_diffs)

        spreads = [row["spread"] for row in rows if row["spread"] > 0.0]
        if spreads:
            return min(spreads)

        raise ValueError("failed to infer tick_size from calibration_lob_path")

    def _estimate_adjustments(self, observations):
        states = [
            (imbalance_bin, spread_bin)
            for spread_bin in range(1, self.max_spread_bins + 1)
            for imbalance_bin in range(self.imbalance_bins)
        ]
        state_set = set(states)
        total_counts = defaultdict(int)
        zero_counts = defaultdict(lambda: defaultdict(int))
        move_counts = defaultdict(lambda: defaultdict(int))
        move_sums = defaultdict(float)

        for current_state, next_state, mid_change in observations:
            if current_state not in state_set or next_state not in state_set:
                continue
            total_counts[current_state] += 1
            self.state_observations[current_state] += 1
            if abs(mid_change) <= self.microprice_tolerance:
                zero_counts[current_state][next_state] += 1
            else:
                move_counts[current_state][next_state] += 1
                move_sums[current_state] += mid_change

        direct = {
            state: move_sums[state] / total_counts[state]
            if total_counts[state]
            else 0.0
            for state in states
        }
        g1 = self._solve_zero_transition_system(
            direct, states, total_counts, zero_counts
        )

        adjustments = dict(g1)
        term = dict(g1)
        for _ in range(self.microprice_max_iterations):
            term = self._apply_b(term, states, total_counts, zero_counts, move_counts)
            max_step = max((abs(value) for value in term.values()), default=0.0)
            for state in states:
                adjustments[state] += term[state]
            if max_step <= self.microprice_tolerance:
                break
        return adjustments

    def _apply_b(self, vector, states, total_counts, zero_counts, move_counts):
        rhs = {}
        for state in states:
            total_count = total_counts[state]
            if total_count == 0:
                rhs[state] = 0.0
                continue
            rhs[state] = sum(
                count * vector[next_state] / total_count
                for next_state, count in move_counts[state].items()
            )

        return self._solve_zero_transition_system(
            rhs, states, total_counts, zero_counts
        )

    def _solve_zero_transition_system(self, rhs, states, total_counts, zero_counts):
        result = dict(rhs)
        for _ in range(self.microprice_max_iterations):
            max_step = 0.0
            next_result = {}
            for state in states:
                total_count = total_counts[state]
                value = rhs[state]
                if total_count:
                    value += sum(
                        count * result[next_state] / total_count
                        for next_state, count in zero_counts[state].items()
                    )
                next_result[state] = value
                max_step = max(max_step, abs(value - result[state]))
            result = next_result
            if max_step <= self.microprice_tolerance:
                break
        return result

    def _symmetric_state(self, state):
        imbalance_bin, spread_bin = state
        return self.imbalance_bins - 1 - imbalance_bin, spread_bin

    @staticmethod
    def _require_positive(name, value):
        if value <= 0.0:
            raise ValueError(f"{name} must be positive")

    @staticmethod
    def _require_non_negative(name, value):
        if value < 0.0:
            raise ValueError(f"{name} must be non-negative")

    @staticmethod
    def _require_positive_int(name, value):
        if int(value) != value or value <= 0:
            raise ValueError(f"{name} must be a positive integer")
        return int(value)

    @staticmethod
    def _require_non_negative_int(name, value):
        if int(value) != value or value < 0:
            raise ValueError(f"{name} must be a non-negative integer")
        return int(value)


class AvellanedaStoikovMicropriceMarketMaker:
    def __init__(
        self,
        order_quantity=1000.0,
        inventory_limit=5000.0,
        gamma=0.1,
        sigma=0.00001,
        k=20000000.0,
        horizon_seconds=1.0,
        timestamp_unit_seconds=1e-9,
        requote_threshold=0.000000005,
        calibration_lob_path="",
        imbalance_bins=10,
        max_spread_bins=5,
        tick_size=0.0,
        min_state_observations=10,
        calibration_max_rows=0,
        microprice_tolerance=1e-12,
        microprice_max_iterations=100,
    ):
        self._require_positive("order_quantity", order_quantity)
        self._require_positive("inventory_limit", inventory_limit)
        self._require_positive("gamma", gamma)
        self._require_non_negative("sigma", sigma)
        self._require_positive("k", k)
        self._require_non_negative("horizon_seconds", horizon_seconds)
        self._require_positive("timestamp_unit_seconds", timestamp_unit_seconds)
        self._require_non_negative("requote_threshold", requote_threshold)

        self.order_quantity = order_quantity
        self.inventory_limit = inventory_limit
        self.gamma = gamma
        self.sigma = sigma
        self.k = k
        self.horizon_seconds = horizon_seconds
        self.timestamp_unit_seconds = timestamp_unit_seconds
        self.requote_threshold = requote_threshold
        self.microprice = MicropriceEstimator(
            calibration_lob_path=calibration_lob_path,
            imbalance_bins=imbalance_bins,
            max_spread_bins=max_spread_bins,
            tick_size=tick_size,
            min_state_observations=min_state_observations,
            calibration_max_rows=calibration_max_rows,
            microprice_tolerance=microprice_tolerance,
            microprice_max_iterations=microprice_max_iterations,
        )

        self.start_timestamp = None
        self.bid_order_id = None
        self.ask_order_id = None
        self.bid_price = 0.0
        self.ask_price = 0.0

    def on_book(self, ctx, book):
        if not book.bids or not book.asks:
            return

        if self.start_timestamp is None:
            self.start_timestamp = book.timestamp

        reference_price = self.microprice.reference_from_book(book)
        remaining_time = self.remaining_time(book.timestamp)
        desired_bid, desired_ask = self.quotes(
            reference_price, ctx.inventory, remaining_time
        )
        can_buy = (
            desired_bid > 0.0
            and ctx.inventory + self.order_quantity <= self.inventory_limit
        )
        can_sell = (
            desired_ask > 0.0
            and ctx.inventory - self.order_quantity >= -self.inventory_limit
        )

        if self._should_replace(self.bid_order_id, self.bid_price, desired_bid, can_buy):
            ctx.cancel(self.bid_order_id)
            self.bid_order_id = None
        if self._should_replace(self.ask_order_id, self.ask_price, desired_ask, can_sell):
            ctx.cancel(self.ask_order_id)
            self.ask_order_id = None

        if can_buy and self.bid_order_id is None:
            self.bid_order_id = ctx.place_limit(bt.Side.Buy, desired_bid, self.order_quantity)
            self.bid_price = desired_bid
        if can_sell and self.ask_order_id is None:
            self.ask_order_id = ctx.place_limit(bt.Side.Sell, desired_ask, self.order_quantity)
            self.ask_price = desired_ask

    def on_fill(self, ctx, fill):
        del ctx
        if fill.remaining_quantity > 0:
            return
        if self.bid_order_id == fill.order_id:
            self.bid_order_id = None
        if self.ask_order_id == fill.order_id:
            self.ask_order_id = None

    def remaining_time(self, timestamp):
        elapsed_seconds = (timestamp - self.start_timestamp) * self.timestamp_unit_seconds
        return max(self.horizon_seconds - elapsed_seconds, 0.0)

    def quotes(self, reference_price, inventory, remaining_time):
        variance_term = self.gamma * self.sigma * self.sigma * remaining_time
        reservation = reference_price - inventory * variance_term
        spread = variance_term + (2.0 / self.gamma) * math.log1p(self.gamma / self.k)
        half_spread = spread / 2.0
        return reservation - half_spread, reservation + half_spread

    def _should_replace(self, order_id, current_price, desired_price, can_quote):
        if order_id is None:
            return False
        if not can_quote:
            return True
        return abs(current_price - desired_price) > self.requote_threshold

    @staticmethod
    def _require_positive(name, value):
        if value <= 0.0:
            raise ValueError(f"{name} must be positive")

    @staticmethod
    def _require_non_negative(name, value):
        if value < 0.0:
            raise ValueError(f"{name} must be non-negative")


class AvellanedaStoikovMicropriceApp(BacktestStrategyApp):
    strategy_name = "AvellanedaStoikovMicroprice"
    default_strategy_params = {
        "order_quantity": 1000.0,
        "inventory_limit": 5000.0,
        "gamma": 0.1,
        "sigma": 0.00001,
        "k": 20000000.0,
        "horizon_seconds": 1.0,
        "timestamp_unit_seconds": 1e-9,
        "requote_threshold": 0.000000005,
        "calibration_lob_path": "",
        "imbalance_bins": 10,
        "max_spread_bins": 5,
        "tick_size": 0.0,
        "min_state_observations": 10,
        "calibration_max_rows": 0,
        "microprice_tolerance": 1e-12,
        "microprice_max_iterations": 100,
    }

    def create_strategy(self, strategy_params):
        return AvellanedaStoikovMicropriceMarketMaker(**strategy_params)


if __name__ == "__main__":
    AvellanedaStoikovMicropriceApp().run()
