import math
import tempfile
from pathlib import Path

import cmf_backtester as bt

from avellaneda_stoikov_microprice import (
    AvellanedaStoikovMicropriceMarketMaker,
    MicropriceEstimator,
)


def assert_close(actual, expected, tolerance=1e-12):
    assert abs(actual - expected) <= tolerance, f"{actual} != {expected}"


def write_lob(path, rows):
    path.write_text(
        ",local_timestamp,asks[0].price,asks[0].amount,bids[0].price,bids[0].amount\n"
        + "\n".join(
            f"{index},{timestamp},{ask},{ask_quantity},{bid},{bid_quantity}"
            for index, (timestamp, bid, bid_quantity, ask, ask_quantity) in enumerate(rows)
        )
        + "\n"
    )


def test_weighted_mid_fallback():
    estimator = MicropriceEstimator()

    reference = estimator.reference_from_values(
        bid_price=100.0,
        bid_quantity=75.0,
        ask_price=101.0,
        ask_quantity=25.0,
    )

    assert_close(reference, 0.75 * 101.0 + 0.25 * 100.0)


def test_calibrated_microprice_skew_direction():
    with tempfile.TemporaryDirectory() as tmp:
        lob_path = Path(tmp) / "calibration_lob.csv"
        write_lob(
            lob_path,
            [
                (1, 100.0, 90.0, 101.0, 10.0),
                (2, 101.0, 90.0, 102.0, 10.0),
                (3, 102.0, 90.0, 103.0, 10.0),
                (4, 103.0, 90.0, 104.0, 10.0),
            ],
        )

        estimator = MicropriceEstimator(
            calibration_lob_path=str(lob_path),
            imbalance_bins=10,
            max_spread_bins=1,
            tick_size=1.0,
            min_state_observations=1,
            microprice_max_iterations=10,
        )

        high_reference = estimator.reference_from_values(100.0, 90.0, 101.0, 10.0)
        low_reference = estimator.reference_from_values(100.0, 1.0, 101.0, 99.0)

        assert high_reference > 100.5
        assert low_reference < 100.5


def test_quotes_use_microprice_reference():
    strategy = AvellanedaStoikovMicropriceMarketMaker(
        order_quantity=1.0,
        inventory_limit=10.0,
        gamma=0.2,
        sigma=2.0,
        k=1.5,
        horizon_seconds=1.0,
    )

    reference_price = strategy.microprice.reference_from_values(
        bid_price=100.0,
        bid_quantity=75.0,
        ask_price=101.0,
        ask_quantity=25.0,
    )
    bid, ask = strategy.quotes(reference_price, inventory=3.0, remaining_time=0.5)

    variance_term = 0.2 * 2.0 * 2.0 * 0.5
    reservation = reference_price - 3.0 * variance_term
    spread = variance_term + (2.0 / 0.2) * math.log1p(0.2 / 1.5)

    assert_close(bid, reservation - spread / 2.0)
    assert_close(ask, reservation + spread / 2.0)


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
        {"imbalance_bins": 0},
        {"max_spread_bins": 0},
        {"tick_size": -1.0},
        {"min_state_observations": -1},
        {"calibration_max_rows": -1},
        {"microprice_tolerance": 0.0},
        {"microprice_max_iterations": 0},
        {"calibration_lob_path": "/path/that/does/not/exist.csv"},
    ]

    for kwargs in invalid_cases:
        try:
            AvellanedaStoikovMicropriceMarketMaker(**kwargs)
        except ValueError:
            continue
        raise AssertionError(f"expected ValueError for {kwargs}")


def test_backtester_smoke_run():
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        lob = tmp_path / "lob.csv"
        trades = tmp_path / "trades.csv"
        write_lob(
            lob,
            [
                (1000000000, 100.0, 1000.0, 101.0, 1000.0),
                (1100000000, 100.0, 1000.0, 101.0, 1000.0),
            ],
        )
        trades.write_text(
            ",local_timestamp,side,price,amount\n"
            "0,1050000000,sell,99,1\n"
            "1,1150000000,buy,102,1\n"
        )

        strategy = AvellanedaStoikovMicropriceMarketMaker(
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


test_weighted_mid_fallback()
test_calibrated_microprice_skew_direction()
test_quotes_use_microprice_reference()
test_invalid_parameters()
test_backtester_smoke_run()
