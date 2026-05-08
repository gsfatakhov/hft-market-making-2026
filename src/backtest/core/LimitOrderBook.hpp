// Snapshot-based top-of-book state.

#pragma once

#include "backtest/io/MarketData.hpp"

#include <optional>
#include <vector>

namespace cmf::backtest
{

class LimitOrderBook
{
  public:
    void update(const BookSnapshot& snapshot);

    NanoTime timestamp() const { return timestamp_; }
    const std::vector<BookLevel>& bids() const { return bids_; }
    const std::vector<BookLevel>& asks() const { return asks_; }

    bool hasBid() const { return !bids_.empty(); }
    bool hasAsk() const { return !asks_.empty(); }
    std::optional<Price> bestBidPrice() const;
    std::optional<Price> bestAskPrice() const;
    std::optional<Price> midPrice() const;

    BookSnapshot snapshot() const;

  private:
    NanoTime timestamp_{};
    std::vector<BookLevel> bids_;
    std::vector<BookLevel> asks_;
};

} // namespace cmf::backtest
