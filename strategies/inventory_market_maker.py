import cmf_backtester as bt

from strategy_app import BacktestStrategyApp


class InventoryMarketMaker:
    def __init__(
        self,
        order_quantity=1000.0,
        inventory_limit=5000.0,
        half_spread=0.00000005,
        requote_threshold=0.000000005,
        skew_per_unit=0.0,
    ):
        self.order_quantity = order_quantity
        self.inventory_limit = inventory_limit
        self.half_spread = half_spread
        self.requote_threshold = requote_threshold
        self.skew_per_unit = skew_per_unit
        self.bid_order_id = None
        self.ask_order_id = None
        self.bid_price = 0.0
        self.ask_price = 0.0

    def on_book(self, ctx, book):
        if not book.bids or not book.asks:
            return

        mid = (book.bids[0].price + book.asks[0].price) / 2.0
        inventory_skew = ctx.inventory * self.skew_per_unit
        desired_bid = mid - self.half_spread - inventory_skew
        desired_ask = mid + self.half_spread - inventory_skew
        can_buy = ctx.inventory + self.order_quantity <= self.inventory_limit
        can_sell = ctx.inventory - self.order_quantity >= -self.inventory_limit

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

    def _should_replace(self, order_id, current_price, desired_price, can_quote):
        if order_id is None:
            return False
        if not can_quote:
            return True
        return abs(current_price - desired_price) > self.requote_threshold


class InventoryMarketMakerApp(BacktestStrategyApp):
    strategy_name = "InventoryMarketMaker"
    default_strategy_params = {
        "order_quantity": 1000.0,
        "inventory_limit": 5000.0,
        "half_spread": 0.00000005,
        "requote_threshold": 0.000000005,
        "skew_per_unit": 0.0,
    }

    def create_strategy(self, strategy_params):
        return InventoryMarketMaker(**strategy_params)


if __name__ == "__main__":
    InventoryMarketMakerApp().run()
