import tempfile
from pathlib import Path

import cmf_backtester as bt


class Strategy:
    def __init__(self):
        self.placed = False
        self.fills = 0

    def on_book(self, ctx, book):
        if self.placed:
            return
        ctx.place_limit(bt.Side.Buy, book.bids[0].price, 10.0)
        self.placed = True

    def on_fill(self, ctx, fill):
        del ctx
        assert fill.quantity == 4.0
        self.fills += 1


with tempfile.TemporaryDirectory() as tmp:
    tmp_path = Path(tmp)
    lob = tmp_path / "lob.csv"
    trades = tmp_path / "trades.csv"
    lob.write_text(
        ",local_timestamp,asks[0].price,asks[0].amount,bids[0].price,bids[0].amount\n"
        "0,10,101,1000,100,1000\n"
    )
    trades.write_text(",local_timestamp,side,price,amount\n0,11,sell,100,4\n")

    strategy = Strategy()
    progress_events = []
    report = bt.run(
        str(lob),
        str(trades),
        strategy,
        progress_interval_events=1,
        progress_callback=progress_events.append,
    )
    assert strategy.fills == 1
    assert report.inventory == 4.0
    assert report.traded_quantity == 4.0
    assert progress_events[-1].finished
    assert progress_events[-1].events == report.events
