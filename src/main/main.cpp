#include "backtest/core/BacktestEngine.hpp"
#include "backtest/core/ProgressPrinter.hpp"
#include "backtest/strategy/InventoryMarketMaker.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

using namespace cmf;
using namespace cmf::backtest;

namespace
{

using ConfigMap = std::unordered_map<std::string, std::string>;

std::string trim(std::string value)
{
    const auto isSpace = [](const unsigned char ch)
    { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [isSpace](const char ch)
                                            { return !isSpace(static_cast<unsigned char>(ch)); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [isSpace](const char ch)
                             { return !isSpace(static_cast<unsigned char>(ch)); })
                    .base(),
                value.end());
    return value;
}

ConfigMap readConfig(const std::filesystem::path& path)
{
    std::ifstream stream{path};
    if (!stream)
    {
        throw std::runtime_error("Failed to open config: " + path.string());
    }

    ConfigMap result;
    std::string line;
    while (std::getline(stream, line))
    {
        const auto comment = line.find('#');
        if (comment != std::string::npos)
        {
            line.erase(comment);
        }

        line = trim(line);
        if (line.empty())
        {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos)
        {
            throw std::runtime_error("Invalid config line: " + line);
        }

        auto key = trim(line.substr(0, separator));
        auto value = trim(line.substr(separator + 1));
        result[std::move(key)] = std::move(value);
    }

    return result;
}

std::string requireString(const ConfigMap& config, const std::string& key)
{
    const auto it = config.find(key);
    if (it == config.end())
    {
        throw std::runtime_error("Config is missing required key: " + key);
    }
    return it->second;
}

double requireDouble(const ConfigMap& config, const std::string& key)
{
    const auto it = config.find(key);
    if (it == config.end())
    {
        throw std::runtime_error("Config is missing required key: " + key);
    }
    return std::stod(it->second);
}

std::size_t requireSize(const ConfigMap& config, const std::string& key)
{
    const auto it = config.find(key);
    if (it == config.end())
    {
        throw std::runtime_error("Config is missing required key: " + key);
    }
    return static_cast<std::size_t>(std::stoull(it->second));
}

bool requireBool(const ConfigMap& config, const std::string& key)
{
    const auto it = config.find(key);
    if (it == config.end())
    {
        throw std::runtime_error("Config is missing required key: " + key);
    }

    auto value = it->second;
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch)
                   { return static_cast<char>(std::tolower(ch)); });
    if (value == "1" || value == "true" || value == "yes" || value == "on")
    {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off")
    {
        return false;
    }
    throw std::runtime_error("Config key must be boolean: " + key);
}

std::string formatSize(std::uintmax_t bytes)
{
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    auto value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < sizeof(units) / sizeof(units[0]))
    {
        value /= 1024.0;
        ++unit;
    }

    std::ostringstream stream;
    if (unit == 0)
    {
        stream << static_cast<std::uintmax_t>(value) << ' ' << units[unit];
    }
    else
    {
        stream << std::fixed << std::setprecision(2) << value << ' ' << units[unit];
    }
    return stream.str();
}

std::string fileSummary(const std::string& path)
{
    return path + " (" + formatSize(std::filesystem::file_size(path)) + ")";
}

void printLaunchSummary(
    const std::filesystem::path& configPath,
    const BacktestConfig& backtestConfig,
    const std::string& reportPath,
    const std::string& strategyName,
    const InventoryMarketMakerConfig& strategyConfig,
    const bool progressEnabled)
{
    std::cerr << "C++ backtest launch\n";
    std::cerr << "  config:          " << configPath.string() << '\n';
    std::cerr << "  strategy:        " << strategyName << '\n';
    std::cerr << "  lob data:        " << fileSummary(backtestConfig.lobPath) << '\n';
    std::cerr << "  trades data:     " << fileSummary(backtestConfig.tradesPath) << '\n';
    std::cerr << "  report path:     " << reportPath << '\n';
    std::cerr << "  fee/slippage:    " << backtestConfig.execution.feeRate << " / " << backtestConfig.execution.slippage << '\n';
    std::cerr << "  max events:      " << backtestConfig.maxEvents << '\n';
    std::cerr << "  progress:        " << (progressEnabled ? "every " + std::to_string(backtestConfig.progressIntervalEvents) + " events" : "off") << '\n';
    std::cerr << "  strategy params:\n";
    std::cerr << "    order_quantity: " << strategyConfig.orderQuantity << '\n';
    std::cerr << "    inventory_limit: " << strategyConfig.inventoryLimit << '\n';
    std::cerr << "    half_spread: " << strategyConfig.halfSpread << '\n';
    std::cerr << "    requote_threshold: " << strategyConfig.requoteThreshold << '\n';
    std::cerr << "    skew_per_unit: " << strategyConfig.skewPerUnit << '\n';
}

void writeReport(const std::filesystem::path& path, const BacktestReport& report)
{
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream stream{path};
    if (!stream)
    {
        throw std::runtime_error("Failed to write report: " + path.string());
    }

    stream << "# Backtest Performance Report\n\n";
    stream << std::fixed << std::setprecision(12);
    stream << "| Metric | Value |\n";
    stream << "| --- | ---: |\n";
    stream << "| PnL | " << report.pnl << " |\n";
    stream << "| Cash | " << report.cash << " |\n";
    stream << "| Inventory | " << report.inventory << " |\n";
    stream << "| Turnover | " << report.turnover << " |\n";
    stream << "| Traded quantity | " << report.tradedQuantity << " |\n";
    stream << "| Fees | " << report.fees << " |\n";
    stream << "| Last mid price | " << report.lastMidPrice << " |\n";
    stream << "| Orders placed | " << report.ordersPlaced << " |\n";
    stream << "| Orders cancelled | " << report.ordersCancelled << " |\n";
    stream << "| Fills | " << report.fills << " |\n";
    stream << "| Book events | " << report.bookEvents << " |\n";
    stream << "| Trade events | " << report.tradeEvents << " |\n";
    stream << "| Total events | " << report.events << " |\n";
}

void printUsage(const char* argv0)
{
    std::cerr << "Usage: " << argv0 << " <config.cfg>\n";
    std::cerr << "Example: " << argv0 << " config/sample_backtest.cfg\n";
}

} // namespace

int main(int argc, const char* argv[])
{
    try
    {
        if (argc != 2)
        {
            printUsage(argv[0]);
            return 2;
        }

        const std::filesystem::path configPath = argv[1];
        const auto config = readConfig(configPath);

        BacktestConfig backtestConfig;
        backtestConfig.lobPath = requireString(config, "data.lob_path");
        backtestConfig.tradesPath = requireString(config, "data.trades_path");
        backtestConfig.execution.feeRate = requireDouble(config, "execution.fee_rate");
        backtestConfig.execution.slippage = requireDouble(config, "execution.slippage");
        backtestConfig.maxEvents = requireSize(config, "engine.max_events");
        backtestConfig.progressIntervalEvents = requireSize(config, "progress.interval_events");
        const auto progressEnabled = requireBool(config, "progress.enabled");
        if (progressEnabled)
        {
            backtestConfig.progressCallback = makeProgressPrinter(std::cerr);
        }

        const auto strategyName = requireString(config, "strategy.name");
        if (strategyName != "inventory_market_maker")
        {
            throw std::runtime_error("Unsupported strategy.name: " + strategyName);
        }

        InventoryMarketMakerConfig strategyConfig;
        strategyConfig.orderQuantity = requireDouble(config, "strategy.order_quantity");
        strategyConfig.inventoryLimit = requireDouble(config, "strategy.inventory_limit");
        strategyConfig.halfSpread = requireDouble(config, "strategy.half_spread");
        strategyConfig.requoteThreshold = requireDouble(config, "strategy.requote_threshold");
        strategyConfig.skewPerUnit = requireDouble(config, "strategy.skew_per_unit");
        const auto reportPath = requireString(config, "report.path");

        printLaunchSummary(configPath, backtestConfig, reportPath, strategyName, strategyConfig, progressEnabled);
        InventoryMarketMaker strategy{strategyConfig};
        BacktestEngine engine{backtestConfig};
        const auto report = engine.run(strategy);

        writeReport(reportPath, report);

        std::cout << "Backtest completed: pnl=" << std::fixed << std::setprecision(12) << report.pnl << ", inventory=" << report.inventory << ", turnover=" << report.turnover << '\n';
        std::cout << "Report written to " << reportPath << '\n';
    }
    catch (std::exception& ex)
    {
        std::cerr << "HFT market-making app threw an exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
