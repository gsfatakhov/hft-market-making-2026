// Simulated limit-order exchange and accounting.

#pragma once

#include "backtest/io/MarketData.hpp"
#include "common/BasicTypes.hpp"

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cmf::backtest
{

enum class OrderStatus
{
    New,
    PartiallyFilled,
    Filled,
    Cancelled
};

struct ExecutionConfig
{
    double feeRate{0.0};
    Price slippage{0.0};
};

struct LimitOrder
{
    OrderId id{};
    Side side{Side::None};
    Price price{};
    Quantity quantity{};
    Quantity filledQuantity{};
    NanoTime placedTimestamp{};
    NanoTime updatedTimestamp{};
    OrderStatus status{OrderStatus::New};
    std::uint64_t sequence{};

    Quantity remainingQuantity() const;
    bool isOpen() const;
};

struct OrderView
{
    OrderId id{};
    Side side{Side::None};
    Price price{};
    Quantity quantity{};
    Quantity filledQuantity{};
    Quantity remainingQuantity{};
    OrderStatus status{OrderStatus::New};
};

struct Fill
{
    OrderId orderId{};
    Side side{Side::None};
    Price price{};
    Quantity quantity{};
    Quantity remainingQuantity{};
    NanoTime timestamp{};
    Price marketTradePrice{};
    Quantity fee{};
};

struct BacktestReport
{
    double cash{0.0};
    Quantity inventory{0.0};
    double turnover{0.0};
    Quantity tradedQuantity{0.0};
    double fees{0.0};
    double pnl{0.0};
    Price lastMidPrice{0.0};
    std::size_t ordersPlaced{0};
    std::size_t ordersCancelled{0};
    std::size_t fills{0};
    std::size_t bookEvents{0};
    std::size_t tradeEvents{0};
    std::size_t events{0};
};

class SimulatedExchange
{
  public:
    explicit SimulatedExchange(ExecutionConfig config = {});

    OrderId placeLimit(Side side, Price price, Quantity quantity, NanoTime timestamp);
    bool cancel(OrderId orderId, NanoTime timestamp);
    std::vector<Fill> processTrade(const Trade& trade);

    Quantity inventory() const { return inventory_; }
    double cash() const { return cash_; }
    double turnover() const { return turnover_; }
    Quantity tradedQuantity() const { return tradedQuantity_; }

    std::optional<LimitOrder> order(OrderId orderId) const;
    std::vector<OrderView> openOrders() const;
    BacktestReport report(Price lastMidPrice) const;

  private:
    bool isEligibleForTrade(const LimitOrder& order, const Trade& trade) const;
    Price executionPrice(const LimitOrder& order) const;
    Quantity feeForNotional(double notional) const;

    ExecutionConfig config_;
    OrderId nextOrderId_{1};
    std::uint64_t nextSequence_{1};
    std::unordered_map<OrderId, LimitOrder> orders_;
    std::unordered_set<OrderId> activeOrderIds_;
    double cash_{0.0};
    Quantity inventory_{0.0};
    double turnover_{0.0};
    Quantity tradedQuantity_{0.0};
    double fees_{0.0};
    std::size_t ordersPlaced_{0};
    std::size_t ordersCancelled_{0};
    std::size_t fills_{0};
};

} // namespace cmf::backtest
