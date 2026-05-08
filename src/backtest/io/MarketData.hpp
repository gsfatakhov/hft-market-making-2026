// Market-data structures and CSV readers for the backtester.

#pragma once

#include "common/BasicTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace cmf::backtest
{

struct BookLevel
{
    Price price{};
    Quantity quantity{};
};

struct BookSnapshot
{
    NanoTime timestamp{};
    std::vector<BookLevel> bids;
    std::vector<BookLevel> asks;
};

struct Trade
{
    NanoTime timestamp{};
    Side aggressorSide{Side::None};
    Price price{};
    Quantity quantity{};
};

class BookCsvReader
{
  public:
    struct LevelColumns
    {
        std::size_t price{};
        std::size_t quantity{};
    };

    explicit BookCsvReader(const std::string& path);

    bool next(BookSnapshot& snapshot);
    std::uintmax_t bytesRead() const { return bytesRead_; }

  private:
    std::ifstream stream_;
    std::uintmax_t bytesRead_{0};
    std::uintmax_t fileSize_{0};
    std::size_t timestampColumn_{};
    std::vector<LevelColumns> bidColumns_;
    std::vector<LevelColumns> askColumns_;
};

class TradeCsvReader
{
  public:
    explicit TradeCsvReader(const std::string& path);

    bool next(Trade& trade);
    std::uintmax_t bytesRead() const { return bytesRead_; }

  private:
    std::ifstream stream_;
    std::uintmax_t bytesRead_{0};
    std::uintmax_t fileSize_{0};
    std::size_t timestampColumn_{};
    std::size_t sideColumn_{};
    std::size_t priceColumn_{};
    std::size_t quantityColumn_{};
};

} // namespace cmf::backtest
