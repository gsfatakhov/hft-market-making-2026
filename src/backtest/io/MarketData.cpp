#include "backtest/io/MarketData.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace cmf::backtest
{
namespace
{

constexpr std::size_t npos = static_cast<std::size_t>(-1);

std::vector<std::string> splitCsvLine(const std::string& line)
{
    std::vector<std::string> cells;
    std::string cell;
    cells.reserve(128);

    for (const char ch : line)
    {
        if (ch == ',')
        {
            cells.push_back(cell);
            cell.clear();
        }
        else
        {
            cell.push_back(ch);
        }
    }

    cells.push_back(cell);
    return cells;
}

std::unordered_map<std::string, std::size_t> indexHeader(const std::string& headerLine)
{
    const auto header = splitCsvLine(headerLine);
    std::unordered_map<std::string, std::size_t> columns;
    columns.reserve(header.size());

    for (std::size_t index = 0; index < header.size(); ++index)
    {
        columns.emplace(header[index], index);
    }

    return columns;
}

std::size_t requireColumn(const std::unordered_map<std::string, std::size_t>& columns, const std::string& name)
{
    const auto it = columns.find(name);
    if (it == columns.end())
    {
        throw std::runtime_error("CSV is missing required column: " + name);
    }
    return it->second;
}

std::size_t optionalColumn(const std::unordered_map<std::string, std::size_t>& columns, const std::string& name)
{
    const auto it = columns.find(name);
    if (it == columns.end())
    {
        return npos;
    }
    return it->second;
}

bool hasCell(const std::vector<std::string>& cells, const std::size_t index)
{
    return index != npos && index < cells.size() && !cells[index].empty();
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch)
                   { return static_cast<char>(std::tolower(ch)); });
    return value;
}

Side parseSide(const std::string& raw)
{
    const auto value = lower(raw);
    if (value == "buy" || value == "b")
    {
        return Side::Buy;
    }
    if (value == "sell" || value == "s")
    {
        return Side::Sell;
    }
    throw std::runtime_error("Unsupported trade side: " + raw);
}

std::vector<BookCsvReader::LevelColumns> discoverLevelColumns(const std::unordered_map<std::string, std::size_t>& columns, const std::string& prefix)
{
    std::vector<BookCsvReader::LevelColumns> result;
    for (std::size_t level = 0;; ++level)
    {
        const auto priceColumn = optionalColumn(columns, prefix + "[" + std::to_string(level) + "].price");
        const auto quantityColumn = optionalColumn(columns, prefix + "[" + std::to_string(level) + "].amount");
        if (priceColumn == npos && quantityColumn == npos)
        {
            break;
        }
        if (priceColumn == npos || quantityColumn == npos)
        {
            throw std::runtime_error("Incomplete LOB level columns for " + prefix + "[" + std::to_string(level) + "]");
        }
        result.push_back({priceColumn, quantityColumn});
    }
    return result;
}

void appendLevels(const std::vector<std::string>& cells, const std::vector<BookCsvReader::LevelColumns>& columns, std::vector<BookLevel>& levels)
{
    levels.clear();
    levels.reserve(columns.size());

    for (const auto& column : columns)
    {
        if (!hasCell(cells, column.price) || !hasCell(cells, column.quantity))
        {
            continue;
        }

        levels.push_back({std::stod(cells[column.price]), std::stod(cells[column.quantity])});
    }
}

std::uintmax_t currentOffset(std::ifstream& stream, const std::uintmax_t fallback)
{
    const auto position = stream.tellg();
    if (position < 0)
    {
        return fallback;
    }
    return static_cast<std::uintmax_t>(position);
}

} // namespace

BookCsvReader::BookCsvReader(const std::string& path)
    : stream_{path}
{
    if (!stream_)
    {
        throw std::runtime_error("Failed to open LOB CSV: " + path);
    }
    fileSize_ = std::filesystem::file_size(path);

    std::string headerLine;
    if (!std::getline(stream_, headerLine))
    {
        throw std::runtime_error("LOB CSV is empty: " + path);
    }
    bytesRead_ = currentOffset(stream_, fileSize_);

    const auto columns = indexHeader(headerLine);
    timestampColumn_ = requireColumn(columns, "local_timestamp");
    bidColumns_ = discoverLevelColumns(columns, "bids");
    askColumns_ = discoverLevelColumns(columns, "asks");

    if (bidColumns_.empty() || askColumns_.empty())
    {
        throw std::runtime_error("LOB CSV must contain at least one bid and one ask level: " + path);
    }
}

bool BookCsvReader::next(BookSnapshot& snapshot)
{
    std::string line;
    while (std::getline(stream_, line))
    {
        if (line.empty())
        {
            continue;
        }

        const auto cells = splitCsvLine(line);
        bytesRead_ = currentOffset(stream_, fileSize_);
        if (!hasCell(cells, timestampColumn_))
        {
            throw std::runtime_error("LOB CSV row is missing local_timestamp");
        }

        snapshot.timestamp = std::stoll(cells[timestampColumn_]);
        appendLevels(cells, bidColumns_, snapshot.bids);
        appendLevels(cells, askColumns_, snapshot.asks);
        return true;
    }

    bytesRead_ = fileSize_;
    return false;
}

TradeCsvReader::TradeCsvReader(const std::string& path)
    : stream_{path}
{
    if (!stream_)
    {
        throw std::runtime_error("Failed to open trades CSV: " + path);
    }
    fileSize_ = std::filesystem::file_size(path);

    std::string headerLine;
    if (!std::getline(stream_, headerLine))
    {
        throw std::runtime_error("Trades CSV is empty: " + path);
    }
    bytesRead_ = currentOffset(stream_, fileSize_);

    const auto columns = indexHeader(headerLine);
    timestampColumn_ = requireColumn(columns, "local_timestamp");
    sideColumn_ = requireColumn(columns, "side");
    priceColumn_ = requireColumn(columns, "price");
    quantityColumn_ = requireColumn(columns, "amount");
}

bool TradeCsvReader::next(Trade& trade)
{
    std::string line;
    while (std::getline(stream_, line))
    {
        if (line.empty())
        {
            continue;
        }

        const auto cells = splitCsvLine(line);
        bytesRead_ = currentOffset(stream_, fileSize_);
        if (!hasCell(cells, timestampColumn_) || !hasCell(cells, sideColumn_) || !hasCell(cells, priceColumn_) || !hasCell(cells, quantityColumn_))
        {
            throw std::runtime_error("Trades CSV row is missing a required field");
        }

        trade.timestamp = std::stoll(cells[timestampColumn_]);
        trade.aggressorSide = parseSide(cells[sideColumn_]);
        trade.price = std::stod(cells[priceColumn_]);
        trade.quantity = std::stod(cells[quantityColumn_]);
        return true;
    }

    bytesRead_ = fileSize_;
    return false;
}

} // namespace cmf::backtest
