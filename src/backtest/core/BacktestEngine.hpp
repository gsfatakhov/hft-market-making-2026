// Event-driven historical replay engine.

#pragma once

#include "backtest/core/LimitOrderBook.hpp"
#include "backtest/core/SimulatedExchange.hpp"
#include "backtest/core/Strategy.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace cmf::backtest
{

struct BacktestProgress
{
    std::size_t events{0};
    std::size_t bookEvents{0};
    std::size_t tradeEvents{0};
    std::uintmax_t bytesRead{0};
    std::uintmax_t totalBytes{0};
    double percent{0.0};
    double elapsedSeconds{0.0};
    double etaSeconds{0.0};
    bool finished{false};
};

struct BacktestConfig
{
    std::string lobPath;
    std::string tradesPath;
    ExecutionConfig execution;
    std::size_t maxEvents{0};
    std::size_t progressIntervalEvents{100000};
    std::function<void(const BacktestProgress&)> progressCallback;
};

class BacktestEngine
{
  public:
    explicit BacktestEngine(BacktestConfig config);

    BacktestReport run(Strategy& strategy);

  private:
    BacktestConfig config_;
};

} // namespace cmf::backtest
