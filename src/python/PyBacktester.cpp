#include "backtest/core/BacktestEngine.hpp"
#include "backtest/core/ProgressPrinter.hpp"
#include "backtest/core/SimulatedExchange.hpp"
#include "backtest/core/Strategy.hpp"
#include "backtest/io/MarketData.hpp"

#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>
#include <utility>

namespace py = pybind11;

namespace cmf::backtest
{
namespace
{

class PyStrategyAdapter final : public Strategy
{
  public:
    explicit PyStrategyAdapter(py::object strategy)
        : strategy_{std::move(strategy)}
    {
        onStart_ = callbackOrNone("on_start");
        onBook_ = callbackOrNone("on_book");
        onTrade_ = callbackOrNone("on_trade");
        onFill_ = callbackOrNone("on_fill");
        onFinish_ = callbackOrNone("on_finish");
    }

    void onStart(StrategyContext& context) override
    {
        callIfPresent(onStart_, context);
    }

    void onBook(StrategyContext& context, const BookSnapshot& book) override
    {
        callIfPresent(onBook_, context, book);
    }

    void onTrade(StrategyContext& context, const Trade& trade) override
    {
        callIfPresent(onTrade_, context, trade);
    }

    void onFill(StrategyContext& context, const Fill& fill) override
    {
        callIfPresent(onFill_, context, fill);
    }

    void onFinish(StrategyContext& context, const BacktestReport& report) override
    {
        callIfPresent(onFinish_, context, report);
    }

  private:
    py::object callbackOrNone(const char* name) const
    {
        if (py::hasattr(strategy_, name))
        {
            return strategy_.attr(name);
        }
        return py::none();
    }

    template <class... Args>
    void callIfPresent(const py::object& callback, Args&&... args)
    {
        if (!callback.is_none())
        {
            callback(std::forward<Args>(args)...);
        }
    }

    py::object strategy_;
    py::object onStart_;
    py::object onBook_;
    py::object onTrade_;
    py::object onFill_;
    py::object onFinish_;
};

BacktestReport runPythonStrategy(
    const std::string& lobPath,
    const std::string& tradesPath,
    py::object strategy,
    const double feeRate,
    const Price slippage,
    const std::size_t maxEvents,
    const std::size_t progressIntervalEvents,
    py::object progressCallback,
    const bool printProgress)
{
    BacktestConfig config;
    config.lobPath = lobPath;
    config.tradesPath = tradesPath;
    config.execution.feeRate = feeRate;
    config.execution.slippage = slippage;
    config.maxEvents = maxEvents;
    config.progressIntervalEvents = progressIntervalEvents;
    if (!progressCallback.is_none())
    {
        config.progressCallback = [callback = std::move(progressCallback)](const BacktestProgress& progress)
        {
            callback(progress);
        };
    }
    else if (printProgress)
    {
        config.progressCallback = makeProgressPrinter(std::cerr);
    }

    BacktestEngine engine{config};
    PyStrategyAdapter adapter{std::move(strategy)};
    return engine.run(adapter);
}

} // namespace
} // namespace cmf::backtest

PYBIND11_MODULE(cmf_backtester, module)
{
    using namespace cmf;
    using namespace cmf::backtest;

    module.doc() = "C++ event-driven market-data backtester";

    py::enum_<Side>(module, "Side")
        .value("None", Side::None)
        .value("Buy", Side::Buy)
        .value("Sell", Side::Sell);

    py::enum_<OrderStatus>(module, "OrderStatus")
        .value("New", OrderStatus::New)
        .value("PartiallyFilled", OrderStatus::PartiallyFilled)
        .value("Filled", OrderStatus::Filled)
        .value("Cancelled", OrderStatus::Cancelled);

    py::class_<BookLevel>(module, "BookLevel")
        .def_readonly("price", &BookLevel::price)
        .def_readonly("quantity", &BookLevel::quantity);

    py::class_<BookSnapshot>(module, "BookSnapshot")
        .def_readonly("timestamp", &BookSnapshot::timestamp)
        .def_readonly("bids", &BookSnapshot::bids)
        .def_readonly("asks", &BookSnapshot::asks);

    py::class_<Trade>(module, "Trade")
        .def_readonly("timestamp", &Trade::timestamp)
        .def_readonly("aggressor_side", &Trade::aggressorSide)
        .def_readonly("price", &Trade::price)
        .def_readonly("quantity", &Trade::quantity);

    py::class_<OrderView>(module, "Order")
        .def_readonly("id", &OrderView::id)
        .def_readonly("side", &OrderView::side)
        .def_readonly("price", &OrderView::price)
        .def_readonly("quantity", &OrderView::quantity)
        .def_readonly("filled_quantity", &OrderView::filledQuantity)
        .def_readonly("remaining_quantity", &OrderView::remainingQuantity)
        .def_readonly("status", &OrderView::status);

    py::class_<Fill>(module, "Fill")
        .def_readonly("order_id", &Fill::orderId)
        .def_readonly("side", &Fill::side)
        .def_readonly("price", &Fill::price)
        .def_readonly("quantity", &Fill::quantity)
        .def_readonly("remaining_quantity", &Fill::remainingQuantity)
        .def_readonly("timestamp", &Fill::timestamp)
        .def_readonly("market_trade_price", &Fill::marketTradePrice)
        .def_readonly("fee", &Fill::fee);

    py::class_<BacktestReport>(module, "BacktestReport")
        .def_readonly("cash", &BacktestReport::cash)
        .def_readonly("inventory", &BacktestReport::inventory)
        .def_readonly("turnover", &BacktestReport::turnover)
        .def_readonly("traded_quantity", &BacktestReport::tradedQuantity)
        .def_readonly("fees", &BacktestReport::fees)
        .def_readonly("pnl", &BacktestReport::pnl)
        .def_readonly("last_mid_price", &BacktestReport::lastMidPrice)
        .def_readonly("orders_placed", &BacktestReport::ordersPlaced)
        .def_readonly("orders_cancelled", &BacktestReport::ordersCancelled)
        .def_readonly("fills", &BacktestReport::fills)
        .def_readonly("book_events", &BacktestReport::bookEvents)
        .def_readonly("trade_events", &BacktestReport::tradeEvents)
        .def_readonly("events", &BacktestReport::events);

    py::class_<BacktestProgress>(module, "BacktestProgress")
        .def_readonly("events", &BacktestProgress::events)
        .def_readonly("book_events", &BacktestProgress::bookEvents)
        .def_readonly("trade_events", &BacktestProgress::tradeEvents)
        .def_readonly("bytes_read", &BacktestProgress::bytesRead)
        .def_readonly("total_bytes", &BacktestProgress::totalBytes)
        .def_readonly("percent", &BacktestProgress::percent)
        .def_readonly("elapsed_seconds", &BacktestProgress::elapsedSeconds)
        .def_readonly("eta_seconds", &BacktestProgress::etaSeconds)
        .def_readonly("finished", &BacktestProgress::finished);

    py::class_<StrategyContext>(module, "StrategyContext")
        .def_property_readonly("timestamp", &StrategyContext::timestamp)
        .def("place_limit", &StrategyContext::placeLimit, py::arg("side"), py::arg("price"), py::arg("quantity"))
        .def("cancel", &StrategyContext::cancel, py::arg("order_id"))
        .def_property_readonly("inventory", &StrategyContext::inventory)
        .def_property_readonly("cash", &StrategyContext::cash)
        .def("open_orders", &StrategyContext::openOrders);

    module.def(
        "run",
        &runPythonStrategy,
        py::arg("lob_path"),
        py::arg("trades_path"),
        py::arg("strategy"),
        py::arg("fee_rate") = 0.0,
        py::arg("slippage") = 0.0,
        py::arg("max_events") = 0,
        py::arg("progress_interval_events") = 100000,
        py::arg("progress_callback") = py::none(),
        py::arg("print_progress") = false,
        "Run an event-driven backtest with a Python strategy object.");
}
