// Shared stderr-style progress rendering for backtest runners.

#pragma once

#include "backtest/core/BacktestEngine.hpp"

#include <functional>
#include <iosfwd>

namespace cmf::backtest
{

void printProgressLine(const BacktestProgress& progress, std::ostream& stream);

std::function<void(const BacktestProgress&)> makeProgressPrinter(std::ostream& stream);

} // namespace cmf::backtest
