// Simple sample inventory-aware market-making strategy.

#pragma once

#include "backtest/core/Strategy.hpp"

#include <optional>

namespace cmf::backtest
{

struct InventoryMarketMakerConfig
{
    Quantity orderQuantity{1000.0};
    Quantity inventoryLimit{10000.0};
    Price halfSpread{0.00000005};
    Price requoteThreshold{0.00000001};
    Price skewPerUnit{0.0};
};

class InventoryMarketMaker final : public Strategy
{
  public:
    explicit InventoryMarketMaker(InventoryMarketMakerConfig config = {});

    void onBook(StrategyContext& context, const BookSnapshot& book) override;
    void onFill(StrategyContext& context, const Fill& fill) override;

  private:
    bool shouldReplace(std::optional<OrderId> orderId, Price currentPrice, Price desiredPrice, bool canQuote) const;
    void clearFilledOrder(const Fill& fill);

    InventoryMarketMakerConfig config_;
    std::optional<OrderId> bidOrder_;
    std::optional<OrderId> askOrder_;
    Price bidPrice_{};
    Price askPrice_{};
};

} // namespace cmf::backtest
