#include "backtest/strategy/InventoryMarketMaker.hpp"

#include <cmath>

namespace cmf::backtest
{

InventoryMarketMaker::InventoryMarketMaker(InventoryMarketMakerConfig config)
    : config_{config}
{
}

void InventoryMarketMaker::onBook(StrategyContext& context, const BookSnapshot& book)
{
    if (book.bids.empty() || book.asks.empty())
    {
        return;
    }

    const auto mid = (book.bids.front().price + book.asks.front().price) / 2.0;
    const auto inventorySkew = context.inventory() * config_.skewPerUnit;
    const auto desiredBid = mid - config_.halfSpread - inventorySkew;
    const auto desiredAsk = mid + config_.halfSpread - inventorySkew;
    const bool canBuy = context.inventory() + config_.orderQuantity <= config_.inventoryLimit;
    const bool canSell = context.inventory() - config_.orderQuantity >= -config_.inventoryLimit;

    if (shouldReplace(bidOrder_, bidPrice_, desiredBid, canBuy))
    {
        if (bidOrder_.has_value())
        {
            context.cancel(*bidOrder_);
            bidOrder_.reset();
        }
    }

    if (shouldReplace(askOrder_, askPrice_, desiredAsk, canSell))
    {
        if (askOrder_.has_value())
        {
            context.cancel(*askOrder_);
            askOrder_.reset();
        }
    }

    if (canBuy && !bidOrder_.has_value())
    {
        bidOrder_ = context.placeLimit(Side::Buy, desiredBid, config_.orderQuantity);
        bidPrice_ = desiredBid;
    }

    if (canSell && !askOrder_.has_value())
    {
        askOrder_ = context.placeLimit(Side::Sell, desiredAsk, config_.orderQuantity);
        askPrice_ = desiredAsk;
    }
}

void InventoryMarketMaker::onFill(StrategyContext& context, const Fill& fill)
{
    (void)context;
    clearFilledOrder(fill);
}

bool InventoryMarketMaker::shouldReplace(const std::optional<OrderId> orderId, const Price currentPrice, const Price desiredPrice, const bool canQuote) const
{
    if (!orderId.has_value())
    {
        return false;
    }
    if (!canQuote)
    {
        return true;
    }
    return std::abs(currentPrice - desiredPrice) > config_.requoteThreshold;
}

void InventoryMarketMaker::clearFilledOrder(const Fill& fill)
{
    if (fill.remainingQuantity > 0.0)
    {
        return;
    }
    if (bidOrder_.has_value() && *bidOrder_ == fill.orderId)
    {
        bidOrder_.reset();
    }
    if (askOrder_.has_value() && *askOrder_ == fill.orderId)
    {
        askOrder_.reset();
    }
}

} // namespace cmf::backtest
