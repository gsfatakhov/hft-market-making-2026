#include "backtest/core/SimulatedExchange.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cmf::backtest
{
namespace
{

constexpr Quantity eps = 1e-12;

OrderView makeOrderView(const LimitOrder& order)
{
    return {order.id, order.side, order.price, order.quantity, order.filledQuantity, order.remainingQuantity(), order.status};
}

} // namespace

Quantity LimitOrder::remainingQuantity() const
{
    return std::max<Quantity>(0.0, quantity - filledQuantity);
}

bool LimitOrder::isOpen() const
{
    return status == OrderStatus::New || status == OrderStatus::PartiallyFilled;
}

SimulatedExchange::SimulatedExchange(ExecutionConfig config)
    : config_{config}
{
}

OrderId SimulatedExchange::placeLimit(const Side side, const Price price, const Quantity quantity, const NanoTime timestamp)
{
    if (side != Side::Buy && side != Side::Sell)
    {
        throw std::invalid_argument("Limit order side must be buy or sell");
    }
    if (price <= 0.0)
    {
        throw std::invalid_argument("Limit order price must be positive");
    }
    if (quantity <= 0.0)
    {
        throw std::invalid_argument("Limit order quantity must be positive");
    }

    const auto orderId = nextOrderId_++;
    orders_.emplace(orderId, LimitOrder{orderId, side, price, quantity, 0.0, timestamp, timestamp, OrderStatus::New, nextSequence_++});
    activeOrderIds_.insert(orderId);
    ++ordersPlaced_;
    return orderId;
}

bool SimulatedExchange::cancel(const OrderId orderId, const NanoTime timestamp)
{
    const auto it = orders_.find(orderId);
    if (it == orders_.end() || !it->second.isOpen())
    {
        return false;
    }

    it->second.status = OrderStatus::Cancelled;
    it->second.updatedTimestamp = timestamp;
    activeOrderIds_.erase(orderId);
    ++ordersCancelled_;
    return true;
}

std::vector<Fill> SimulatedExchange::processTrade(const Trade& trade)
{
    if (trade.quantity <= 0.0 || (trade.aggressorSide != Side::Buy && trade.aggressorSide != Side::Sell))
    {
        return {};
    }

    std::vector<OrderId> eligibleIds;
    eligibleIds.reserve(activeOrderIds_.size());
    for (const auto orderId : activeOrderIds_)
    {
        const auto it = orders_.find(orderId);
        if (it != orders_.end() && it->second.isOpen() && isEligibleForTrade(it->second, trade))
        {
            eligibleIds.push_back(orderId);
        }
    }

    std::sort(eligibleIds.begin(), eligibleIds.end(), [this](const OrderId lhs, const OrderId rhs)
              {
        const auto& left = orders_.at(lhs);
        const auto& right = orders_.at(rhs);
        if (std::abs(left.price - right.price) > eps)
        {
            if (left.side == Side::Buy)
            {
                return left.price > right.price;
            }
            return left.price < right.price;
        }
        return left.sequence < right.sequence; });

    Quantity remainingTradeQuantity = trade.quantity;
    std::vector<Fill> fills;
    for (const auto orderId : eligibleIds)
    {
        if (remainingTradeQuantity <= eps)
        {
            break;
        }

        auto it = orders_.find(orderId);
        if (it == orders_.end() || !it->second.isOpen())
        {
            continue;
        }

        auto& order = it->second;
        const auto fillQuantity = std::min(order.remainingQuantity(), remainingTradeQuantity);
        if (fillQuantity <= eps)
        {
            continue;
        }

        const auto fillPrice = executionPrice(order);
        const auto notional = fillPrice * fillQuantity;
        const auto fee = feeForNotional(notional);

        if (order.side == Side::Buy)
        {
            inventory_ += fillQuantity;
            cash_ -= notional + fee;
        }
        else
        {
            inventory_ -= fillQuantity;
            cash_ += notional - fee;
        }

        turnover_ += notional;
        tradedQuantity_ += fillQuantity;
        fees_ += fee;
        order.filledQuantity += fillQuantity;
        order.updatedTimestamp = trade.timestamp;
        remainingTradeQuantity -= fillQuantity;

        if (order.remainingQuantity() <= eps)
        {
            order.filledQuantity = order.quantity;
            order.status = OrderStatus::Filled;
            activeOrderIds_.erase(order.id);
        }
        else
        {
            order.status = OrderStatus::PartiallyFilled;
        }

        fills.push_back({order.id, order.side, fillPrice, fillQuantity, order.remainingQuantity(), trade.timestamp, trade.price, fee});
        ++fills_;
    }

    return fills;
}

std::optional<LimitOrder> SimulatedExchange::order(const OrderId orderId) const
{
    const auto it = orders_.find(orderId);
    if (it == orders_.end())
    {
        return std::nullopt;
    }
    return it->second;
}

std::vector<OrderView> SimulatedExchange::openOrders() const
{
    std::vector<OrderView> result;
    result.reserve(activeOrderIds_.size());
    for (const auto orderId : activeOrderIds_)
    {
        const auto it = orders_.find(orderId);
        if (it != orders_.end() && it->second.isOpen())
        {
            result.push_back(makeOrderView(it->second));
        }
    }

    std::sort(result.begin(), result.end(), [](const OrderView& lhs, const OrderView& rhs)
              { return lhs.id < rhs.id; });
    return result;
}

BacktestReport SimulatedExchange::report(const Price lastMidPrice) const
{
    BacktestReport result;
    result.cash = cash_;
    result.inventory = inventory_;
    result.turnover = turnover_;
    result.tradedQuantity = tradedQuantity_;
    result.fees = fees_;
    result.lastMidPrice = lastMidPrice;
    result.pnl = cash_ + inventory_ * lastMidPrice;
    result.ordersPlaced = ordersPlaced_;
    result.ordersCancelled = ordersCancelled_;
    result.fills = fills_;
    return result;
}

bool SimulatedExchange::isEligibleForTrade(const LimitOrder& order, const Trade& trade) const
{
    if (trade.aggressorSide == Side::Sell)
    {
        return order.side == Side::Buy && order.price + eps >= trade.price;
    }
    if (trade.aggressorSide == Side::Buy)
    {
        return order.side == Side::Sell && order.price <= trade.price + eps;
    }
    return false;
}

Price SimulatedExchange::executionPrice(const LimitOrder& order) const
{
    if (order.side == Side::Buy)
    {
        return order.price + config_.slippage;
    }
    return order.price - config_.slippage;
}

Quantity SimulatedExchange::feeForNotional(const double notional) const
{
    return std::abs(notional) * config_.feeRate;
}

} // namespace cmf::backtest
