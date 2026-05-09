import math

import cmf_backtester as bt

from strategy_app import BacktestStrategyApp


class AvellanedaStoikovMarketMaker:
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

        mid = (book.bids[0].price + book.asks[0].price) / 2.0
        remaining_time = self.remaining_time(book.timestamp)
        desired_bid, desired_ask = self.quotes(mid, ctx.inventory, remaining_time)
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

    def quotes(self, mid, inventory, remaining_time):
        variance_term = self.gamma * self.sigma * self.sigma * remaining_time
        reservation = mid - inventory * variance_term
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


class AvellanedaStoikovApp(BacktestStrategyApp):
    strategy_name = "AvellanedaStoikov"
    default_strategy_params = {
        "order_quantity": 1000.0,
        "inventory_limit": 5000.0,
        "gamma": 0.1,
        "sigma": 0.00001,
        "k": 20000000.0,
        "horizon_seconds": 1.0,
        "timestamp_unit_seconds": 1e-9,
        "requote_threshold": 0.000000005,
    }

    def create_strategy(self, strategy_params):
        return AvellanedaStoikovMarketMaker(**strategy_params)


if __name__ == "__main__":
    AvellanedaStoikovApp().run()
