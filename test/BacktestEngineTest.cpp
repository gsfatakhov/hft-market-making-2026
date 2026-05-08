#include "backtest/core/BacktestEngine.hpp"

#include "TempFile.hpp"
#include "catch2/catch_all.hpp"

#include <fstream>
#include <vector>

using namespace cmf;
using namespace cmf::backtest;

namespace
{

void writeTempFile(const TempFile& file, const std::string& contents)
{
    std::ofstream stream{file.getPath()};
    stream << contents;
}

class TwoSidedStrategy final : public Strategy
{
  public:
    void onBook(StrategyContext& context, const BookSnapshot& book) override
    {
        if (placed_ || book.bids.empty() || book.asks.empty())
        {
            return;
        }

        context.placeLimit(Side::Buy, book.bids.front().price, 10.0);
        context.placeLimit(Side::Sell, book.asks.front().price, 20.0);
        placed_ = true;
    }

    void onFill(StrategyContext& context, const Fill& fill) override
    {
        (void)context;
        fills_ += 1;
        filledQuantity_ += fill.quantity;
    }

    std::size_t fills() const { return fills_; }
    Quantity filledQuantity() const { return filledQuantity_; }

  private:
    bool placed_{false};
    std::size_t fills_{0};
    Quantity filledQuantity_{0.0};
};

} // namespace

TEST_CASE("BacktestEngine - replays books and trades into strategy and report", "[Backtest]")
{
    TempFile lob{"engine-lob.csv"};
    TempFile trades{"engine-trades.csv"};
    writeTempFile(lob, ",local_timestamp,asks[0].price,asks[0].amount,bids[0].price,bids[0].amount\n0,10,101,1000,100,1000\n");
    writeTempFile(trades, ",local_timestamp,side,price,amount\n0,11,sell,100,4\n1,12,buy,101,30\n");

    BacktestConfig config;
    config.lobPath = lob.getPath().string();
    config.tradesPath = trades.getPath().string();

    BacktestEngine engine{config};
    TwoSidedStrategy strategy;
    const auto report = engine.run(strategy);

    REQUIRE(strategy.fills() == 2);
    REQUIRE(strategy.filledQuantity() == Catch::Approx(24.0));
    REQUIRE(report.bookEvents == 1);
    REQUIRE(report.tradeEvents == 2);
    REQUIRE(report.ordersPlaced == 2);
    REQUIRE(report.fills == 2);
    REQUIRE(report.inventory == Catch::Approx(-16.0));
    REQUIRE(report.cash == Catch::Approx(1620.0));
    REQUIRE(report.turnover == Catch::Approx(2420.0));
    REQUIRE(report.pnl == Catch::Approx(12.0));
}

TEST_CASE("BacktestEngine - emits progress callbacks", "[Backtest]")
{
    TempFile lob{"engine-progress-lob.csv"};
    TempFile trades{"engine-progress-trades.csv"};
    writeTempFile(lob, ",local_timestamp,asks[0].price,asks[0].amount,bids[0].price,bids[0].amount\n0,10,101,1000,100,1000\n");
    writeTempFile(trades, ",local_timestamp,side,price,amount\n0,11,sell,100,4\n1,12,buy,101,30\n");

    std::vector<BacktestProgress> progressEvents;
    BacktestConfig config;
    config.lobPath = lob.getPath().string();
    config.tradesPath = trades.getPath().string();
    config.progressIntervalEvents = 1;
    config.progressCallback = [&progressEvents](const BacktestProgress& progress)
    {
        progressEvents.push_back(progress);
    };

    BacktestEngine engine{config};
    TwoSidedStrategy strategy;
    const auto report = engine.run(strategy);

    REQUIRE(report.events == 3);
    REQUIRE(progressEvents.size() >= 3);
    REQUIRE(progressEvents.front().events == 1);
    REQUIRE(progressEvents.back().finished);
    REQUIRE(progressEvents.back().events == report.events);
    REQUIRE(progressEvents.back().percent == Catch::Approx(100.0));
}
