#include "backtest/core/SimulatedExchange.hpp"

#include "catch2/catch_all.hpp"

using namespace cmf;
using namespace cmf::backtest;

TEST_CASE("SimulatedExchange - places and cancels limit orders", "[Backtest]")
{
    SimulatedExchange exchange;

    const auto orderId = exchange.placeLimit(Side::Buy, 10.0, 100.0, 1);
    REQUIRE(orderId == 1);
    REQUIRE(exchange.openOrders().size() == 1);

    REQUIRE(exchange.cancel(orderId, 2));
    REQUIRE(exchange.openOrders().empty());
    REQUIRE_FALSE(exchange.cancel(orderId, 3));
}

TEST_CASE("SimulatedExchange - partially fills eligible buy orders FIFO at same price", "[Backtest]")
{
    SimulatedExchange exchange;
    const auto first = exchange.placeLimit(Side::Buy, 10.0, 100.0, 1);
    const auto second = exchange.placeLimit(Side::Buy, 10.0, 80.0, 2);

    const auto fills = exchange.processTrade({3, Side::Sell, 9.5, 150.0});

    REQUIRE(fills.size() == 2);
    REQUIRE(fills[0].orderId == first);
    REQUIRE(fills[0].quantity == Catch::Approx(100.0));
    REQUIRE(fills[1].orderId == second);
    REQUIRE(fills[1].quantity == Catch::Approx(50.0));
    REQUIRE(fills[1].remainingQuantity == Catch::Approx(30.0));

    const auto secondOrder = exchange.order(second);
    REQUIRE(secondOrder.has_value());
    REQUIRE(secondOrder->status == OrderStatus::PartiallyFilled);
    REQUIRE(secondOrder->remainingQuantity() == Catch::Approx(30.0));

    const auto report = exchange.report(11.0);
    REQUIRE(report.inventory == Catch::Approx(150.0));
    REQUIRE(report.cash == Catch::Approx(-1500.0));
    REQUIRE(report.turnover == Catch::Approx(1500.0));
    REQUIRE(report.pnl == Catch::Approx(150.0));
}

TEST_CASE("SimulatedExchange - fills sell orders from buy aggressor trades", "[Backtest]")
{
    SimulatedExchange exchange;
    const auto orderId = exchange.placeLimit(Side::Sell, 12.0, 20.0, 1);

    REQUIRE(exchange.processTrade({2, Side::Sell, 12.0, 20.0}).empty());
    const auto fills = exchange.processTrade({3, Side::Buy, 12.5, 8.0});

    REQUIRE(fills.size() == 1);
    REQUIRE(fills.front().orderId == orderId);
    REQUIRE(fills.front().price == Catch::Approx(12.0));
    REQUIRE(fills.front().quantity == Catch::Approx(8.0));
    REQUIRE(exchange.inventory() == Catch::Approx(-8.0));
    REQUIRE(exchange.cash() == Catch::Approx(96.0));
}

TEST_CASE("SimulatedExchange - does not fill when price does not cross", "[Backtest]")
{
    SimulatedExchange exchange;
    exchange.placeLimit(Side::Buy, 10.0, 100.0, 1);

    REQUIRE(exchange.processTrade({2, Side::Sell, 10.5, 100.0}).empty());
    REQUIRE(exchange.inventory() == Catch::Approx(0.0));
    REQUIRE(exchange.openOrders().size() == 1);
}
