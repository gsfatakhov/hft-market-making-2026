#include "backtest/core/ProgressPrinter.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <sstream>

namespace cmf::backtest
{
namespace
{

std::string formatDuration(double seconds)
{
    if (seconds < 0.0)
    {
        seconds = 0.0;
    }

    const auto totalSeconds = static_cast<std::uint64_t>(seconds + 0.5);
    const auto hours = totalSeconds / 3600;
    const auto minutes = (totalSeconds % 3600) / 60;
    const auto secs = totalSeconds % 60;

    std::ostringstream stream;
    if (hours > 0)
    {
        stream << hours << 'h' << std::setw(2) << std::setfill('0') << minutes << 'm' << std::setw(2) << secs << 's';
    }
    else if (minutes > 0)
    {
        stream << minutes << 'm' << std::setw(2) << std::setfill('0') << secs << 's';
    }
    else
    {
        stream << secs << 's';
    }

    return stream.str();
}

} // namespace

void printProgressLine(const BacktestProgress& progress, std::ostream& stream)
{
    constexpr int barWidth = 30;
    const auto ratio = progress.totalBytes == 0 ? 1.0 : std::clamp(progress.percent / 100.0, 0.0, 1.0);
    const auto filled = static_cast<int>(ratio * barWidth);
    const auto eventsPerSecond = progress.elapsedSeconds > 0.0 ? static_cast<double>(progress.events) / progress.elapsedSeconds : 0.0;

    stream << "\r[";
    for (int index = 0; index < barWidth; ++index)
    {
        char marker = ' ';
        if (index < filled)
        {
            marker = '=';
        }
        else if (index == filled)
        {
            marker = '>';
        }
        stream << marker;
    }

    stream << "] " << std::fixed << std::setprecision(1) << progress.percent << "% "
           << "events=" << progress.events << " book=" << progress.bookEvents << " trades=" << progress.tradeEvents << ' '
           << std::setprecision(0) << eventsPerSecond << " ev/s elapsed=" << formatDuration(progress.elapsedSeconds);

    if (progress.finished)
    {
        stream << " done";
    }
    else
    {
        stream << " eta=" << formatDuration(progress.etaSeconds);
    }

    stream << std::flush;
    if (progress.finished)
    {
        stream << '\n';
    }
}

std::function<void(const BacktestProgress&)> makeProgressPrinter(std::ostream& stream)
{
    return [&stream](const BacktestProgress& progress)
    {
        printProgressLine(progress, stream);
    };
}

} // namespace cmf::backtest
