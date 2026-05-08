#include "backtest/core/LimitOrderBook.hpp"

namespace cmf::backtest
{

void LimitOrderBook::update(const BookSnapshot& snapshot)
{
    timestamp_ = snapshot.timestamp;
    bids_ = snapshot.bids;
    asks_ = snapshot.asks;
}

std::optional<Price> LimitOrderBook::bestBidPrice() const
{
    if (bids_.empty())
    {
        return std::nullopt;
    }
    return bids_.front().price;
}

std::optional<Price> LimitOrderBook::bestAskPrice() const
{
    if (asks_.empty())
    {
        return std::nullopt;
    }
    return asks_.front().price;
}

std::optional<Price> LimitOrderBook::midPrice() const
{
    if (bids_.empty() || asks_.empty())
    {
        return std::nullopt;
    }
    return (bids_.front().price + asks_.front().price) / 2.0;
}

BookSnapshot LimitOrderBook::snapshot() const
{
    return {timestamp_, bids_, asks_};
}

} // namespace cmf::backtest
