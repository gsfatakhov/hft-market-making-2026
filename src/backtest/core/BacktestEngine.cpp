#include "backtest/core/BacktestEngine.hpp"

#include "backtest/io/MarketData.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <utility>

namespace cmf::backtest
{
namespace
{

double secondsSince(const std::chrono::steady_clock::time_point startedAt)
{
    const auto elapsed = std::chrono::steady_clock::now() - startedAt;
    return std::chrono::duration<double>(elapsed).count();
}

} // namespace

BacktestEngine::BacktestEngine(BacktestConfig config)
    : config_{std::move(config)}
{
    if (config_.lobPath.empty())
    {
        throw std::invalid_argument("BacktestConfig.lobPath is required");
    }
    if (config_.tradesPath.empty())
    {
        throw std::invalid_argument("BacktestConfig.tradesPath is required");
    }
}

BacktestReport BacktestEngine::run(Strategy& strategy)
{
    BookCsvReader bookReader{config_.lobPath};
    TradeCsvReader tradeReader{config_.tradesPath};
    const auto totalBytes = std::filesystem::file_size(config_.lobPath) + std::filesystem::file_size(config_.tradesPath);
    LimitOrderBook book;
    SimulatedExchange exchange{config_.execution};
    StrategyContext context{exchange, book};
    const auto startedAt = std::chrono::steady_clock::now();

    BookSnapshot nextBook;
    Trade nextTrade;
    bool hasBook = bookReader.next(nextBook);
    bool hasTrade = tradeReader.next(nextTrade);
    std::optional<Price> lastMidPrice;
    std::size_t bookEvents = 0;
    std::size_t tradeEvents = 0;
    std::size_t events = 0;
    const auto progressInterval = std::max<std::size_t>(1, config_.progressIntervalEvents);

    const auto reportProgress = [&](const bool finished)
    {
        if (!config_.progressCallback)
        {
            return;
        }

        const auto bytesRead = std::min<std::uintmax_t>(bookReader.bytesRead() + tradeReader.bytesRead(), totalBytes);
        const auto percent = totalBytes == 0 ? 100.0 : std::min(100.0, static_cast<double>(bytesRead) * 100.0 / static_cast<double>(totalBytes));
        const auto elapsedSeconds = secondsSince(startedAt);
        double etaSeconds = 0.0;
        if (!finished && percent > 0.0)
        {
            etaSeconds = elapsedSeconds * (100.0 - percent) / percent;
        }

        config_.progressCallback({events, bookEvents, tradeEvents, bytesRead, totalBytes, percent, elapsedSeconds, etaSeconds, finished});
    };

    strategy.onStart(context);

    while (hasBook || hasTrade)
    {
        if (config_.maxEvents != 0 && events >= config_.maxEvents)
        {
            break;
        }

        const bool processBook = hasBook && (!hasTrade || nextBook.timestamp <= nextTrade.timestamp);
        if (processBook)
        {
            book.update(nextBook);
            context.setTimestamp(nextBook.timestamp);
            lastMidPrice = book.midPrice();
            strategy.onBook(context, nextBook);

            ++bookEvents;
            ++events;
            hasBook = bookReader.next(nextBook);
            if (events % progressInterval == 0)
            {
                if (hasBook || hasTrade)
                {
                    reportProgress(false);
                }
            }
            continue;
        }

        context.setTimestamp(nextTrade.timestamp);
        const auto fills = exchange.processTrade(nextTrade);
        for (const auto& fill : fills)
        {
            strategy.onFill(context, fill);
        }
        strategy.onTrade(context, nextTrade);

        ++tradeEvents;
        ++events;
        hasTrade = tradeReader.next(nextTrade);
        if (events % progressInterval == 0)
        {
            if (hasBook || hasTrade)
            {
                reportProgress(false);
            }
        }
    }

    auto report = exchange.report(lastMidPrice.value_or(0.0));
    report.bookEvents = bookEvents;
    report.tradeEvents = tradeEvents;
    report.events = events;
    strategy.onFinish(context, report);
    reportProgress(true);
    return report;
}

} // namespace cmf::backtest
