#include "backtest/core/Strategy.hpp"

namespace cmf::backtest
{

StrategyContext::StrategyContext(SimulatedExchange& exchange, const LimitOrderBook& book)
    : exchange_{exchange},
      book_{book}
{
}

OrderId StrategyContext::placeLimit(const Side side, const Price price, const Quantity quantity)
{
    return exchange_.placeLimit(side, price, quantity, timestamp_);
}

bool StrategyContext::cancel(const OrderId orderId)
{
    return exchange_.cancel(orderId, timestamp_);
}

void Strategy::onStart(StrategyContext& context)
{
    (void)context;
}

void Strategy::onBook(StrategyContext& context, const BookSnapshot& book)
{
    (void)context;
    (void)book;
}

void Strategy::onTrade(StrategyContext& context, const Trade& trade)
{
    (void)context;
    (void)trade;
}

void Strategy::onFill(StrategyContext& context, const Fill& fill)
{
    (void)context;
    (void)fill;
}

void Strategy::onFinish(StrategyContext& context, const BacktestReport& report)
{
    (void)context;
    (void)report;
}

} // namespace cmf::backtest
