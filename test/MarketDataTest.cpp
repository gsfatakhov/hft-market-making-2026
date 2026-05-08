#include "backtest/io/MarketData.hpp"
#include "backtest/core/LimitOrderBook.hpp"

#include "TempFile.hpp"
#include "catch2/catch_all.hpp"

#include <fstream>

using namespace cmf;
using namespace cmf::backtest;

namespace
{

void writeFile(const TempFile& file, const std::string& contents)
{
    std::ofstream stream{file.getPath()};
    stream << contents;
}

} // namespace

TEST_CASE("MarketData - reads trade CSV", "[Backtest]")
{
    TempFile file{"trades-reader-test.csv"};
    writeFile(file, ",local_timestamp,side,price,amount\n0,10,sell,100.5,7\n1,11,buy,101.0,3\n");

    TradeCsvReader reader{file.getPath().string()};
    Trade trade;

    REQUIRE(reader.next(trade));
    REQUIRE(trade.timestamp == 10);
    REQUIRE(trade.aggressorSide == Side::Sell);
    REQUIRE(trade.price == Catch::Approx(100.5));
    REQUIRE(trade.quantity == Catch::Approx(7.0));

    REQUIRE(reader.next(trade));
    REQUIRE(trade.aggressorSide == Side::Buy);
    REQUIRE_FALSE(reader.next(trade));
}

TEST_CASE("MarketData - reads dynamic LOB depth", "[Backtest]")
{
    TempFile file{"lob-reader-test.csv"};
    writeFile(file, ",local_timestamp,asks[0].price,asks[0].amount,bids[0].price,bids[0].amount,asks[1].price,asks[1].amount,bids[1].price,bids[1].amount\n0,10,101,5,100,6,102,7,99,8\n");

    BookCsvReader reader{file.getPath().string()};
    BookSnapshot snapshot;

    REQUIRE(reader.next(snapshot));
    REQUIRE(snapshot.timestamp == 10);
    REQUIRE(snapshot.asks.size() == 2);
    REQUIRE(snapshot.bids.size() == 2);
    REQUIRE(snapshot.asks[0].price == Catch::Approx(101.0));
    REQUIRE(snapshot.bids[1].quantity == Catch::Approx(8.0));
    REQUIRE_FALSE(reader.next(snapshot));
}

TEST_CASE("LimitOrderBook - exposes best prices and mid", "[Backtest]")
{
    LimitOrderBook book;
    book.update({42, {{100.0, 4.0}}, {{101.0, 5.0}}});

    REQUIRE(book.timestamp() == 42);
    REQUIRE(book.bestBidPrice().value() == Catch::Approx(100.0));
    REQUIRE(book.bestAskPrice().value() == Catch::Approx(101.0));
    REQUIRE(book.midPrice().value() == Catch::Approx(100.5));
}
