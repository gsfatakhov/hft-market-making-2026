import math
import tempfile
from pathlib import Path

import cmf_backtester as bt

from avellaneda_stoikov import AvellanedaStoikovMarketMaker


def assert_close(actual, expected, tolerance=1e-12):
    assert abs(actual - expected) <= tolerance, f"{actual} != {expected}"


def test_quote_formula():
    strategy = AvellanedaStoikovMarketMaker(
        order_quantity=1.0,
        inventory_limit=10.0,
        gamma=0.2,
        sigma=2.0,
        k=1.5,
        horizon_seconds=1.0,
    )

    mid = 100.0
    inventory = 3.0
    remaining_time = 0.5
    bid, ask = strategy.quotes(mid, inventory, remaining_time)

    variance_term = 0.2 * 2.0 * 2.0 * 0.5
    reservation = mid - inventory * variance_term
    spread = variance_term + (2.0 / 0.2) * math.log1p(0.2 / 1.5)

    assert_close(bid, reservation - spread / 2.0)
    assert_close(ask, reservation + spread / 2.0)


def test_inventory_skew_direction():
    strategy = AvellanedaStoikovMarketMaker(
        order_quantity=1.0,
        inventory_limit=10.0,
        gamma=0.2,
        sigma=2.0,
        k=1.5,
        horizon_seconds=1.0,
    )

    flat_bid, flat_ask = strategy.quotes(100.0, 0.0, 0.5)
    long_bid, long_ask = strategy.quotes(100.0, 2.0, 0.5)
    short_bid, short_ask = strategy.quotes(100.0, -2.0, 0.5)

    assert long_bid < flat_bid
    assert long_ask < flat_ask
    assert short_bid > flat_bid
    assert short_ask > flat_ask


def test_invalid_parameters():
    invalid_cases = [
        {"order_quantity": 0.0},
        {"inventory_limit": 0.0},
        {"gamma": 0.0},
        {"sigma": -1.0},
        {"k": 0.0},
        {"horizon_seconds": -1.0},
        {"timestamp_unit_seconds": 0.0},
        {"requote_threshold": -1.0},
    ]

    for kwargs in invalid_cases:
        try:
            AvellanedaStoikovMarketMaker(**kwargs)
        except ValueError:
            continue
        raise AssertionError(f"expected ValueError for {kwargs}")


def test_backtester_smoke_run():
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        lob = tmp_path / "lob.csv"
        trades = tmp_path / "trades.csv"
        lob.write_text(
            ",local_timestamp,asks[0].price,asks[0].amount,bids[0].price,bids[0].amount\n"
            "0,1000000000,101,1000,100,1000\n"
            "1,1100000000,101,1000,100,1000\n"
        )
        trades.write_text(
            ",local_timestamp,side,price,amount\n"
            "0,1050000000,sell,99,1\n"
            "1,1150000000,buy,102,1\n"
        )

        strategy = AvellanedaStoikovMarketMaker(
            order_quantity=1.0,
            inventory_limit=10.0,
            gamma=0.1,
            sigma=0.0,
            k=100.0,
            horizon_seconds=1.0,
            timestamp_unit_seconds=1e-9,
            requote_threshold=0.0,
        )
        report = bt.run(str(lob), str(trades), strategy)

        assert report.orders_placed >= 2
        assert report.events == 4


test_quote_formula()
test_inventory_skew_direction()
test_invalid_parameters()
test_backtester_smoke_run()
