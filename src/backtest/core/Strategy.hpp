// Strategy interface and context passed to callbacks.

#pragma once

#include "backtest/core/LimitOrderBook.hpp"
#include "backtest/core/SimulatedExchange.hpp"

namespace cmf::backtest
{

class StrategyContext
{
  public:
    StrategyContext(SimulatedExchange& exchange, const LimitOrderBook& book);

    void setTimestamp(NanoTime timestamp) { timestamp_ = timestamp; }
    NanoTime timestamp() const { return timestamp_; }

    OrderId placeLimit(Side side, Price price, Quantity quantity);
    bool cancel(OrderId orderId);

    Quantity inventory() const { return exchange_.inventory(); }
    double cash() const { return exchange_.cash(); }
    std::vector<OrderView> openOrders() const { return exchange_.openOrders(); }
    const LimitOrderBook& book() const { return book_; }

  private:
    SimulatedExchange& exchange_;
    const LimitOrderBook& book_;
    NanoTime timestamp_{};
};

class Strategy
{
  public:
    virtual ~Strategy() = default;

    virtual void onStart(StrategyContext& context);
    virtual void onBook(StrategyContext& context, const BookSnapshot& book);
    virtual void onTrade(StrategyContext& context, const Trade& trade);
    virtual void onFill(StrategyContext& context, const Fill& fill);
    virtual void onFinish(StrategyContext& context, const BacktestReport& report);
};

} // namespace cmf::backtest
