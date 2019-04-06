#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>

#include "MarketMaker.h"

namespace py = pybind11;
namespace kvt::pybind {
    PYBIND11_MODULE(kvt, m) {
        m.def("asset_to_string", &kvt::Asset::to_str);
        m.def("asset_parse", &kvt::Asset::parse);

        py::enum_<kvt::Asset::Type>(m, "Asset");

        py::class_<kvt::Competitor>(m, "Competitor")
            .def(py::init<>())
            .def_readwrite("id", &kvt::Competitor::id);

        py::enum_<kvt::Order::OrderType>(m, "OrderType")
            .value("Market", kvt::Order::OrderType::Market)
            .value("Limit", kvt::Order::OrderType::Limit);

        py::class_<kvt::Order>(m, "Order")
            .def(py::init<>())
            .def_readwrite("asset", &kvt::Order::asset)
            .def_readwrite("size", &kvt::Order::size)
            .def_readwrite("type", &kvt::Order::type)
            .def_readwrite("price", &kvt::Order::price)
            .def_readwrite("comp", &kvt::Order::comp)
            .def_readwrite("spread", &kvt::Order::spread)
            .def_readwrite("bid", &kvt::Order::bid)
            .def_readwrite("order_id", &kvt::Order::order_id);

        py::class_<kvt::Fill>(m, "Fill")
            .def(py::init<>())
            .def_readwrite("order_id", &kvt::Fill::order_id)
            .def_readwrite("comp", &kvt::Fill::comp)
            .def_readwrite("filled", &kvt::Fill::filled)
            .def_readwrite("fill_price", &kvt::Fill::fill_price);

        py::class_<kvt::PriceLevel>(m, "PriceLevel")
            .def(py::init<>())
            .def_readwrite("size", &kvt::PriceLevel::size)
            .def_readwrite("price", &kvt::PriceLevel::price);

        py::class_<kvt::MarketUpdate>(m, "MarketUpdate")
            .def(py::init<>())
            .def_readwrite("asset", &kvt::MarketUpdate::asset)
            .def_readwrite("mid_market_price", &kvt::MarketUpdate::mid_market_price)
            .def_readwrite("bids", &kvt::MarketUpdate::bids)
            .def_readwrite("asks", &kvt::MarketUpdate::asks);

        py::bind_vector<std::vector<kvt::MarketUpdate>>(m, "MarketUpdates");
        py::bind_vector<std::vector<kvt::PriceLevel>>(m, "PriceLevels");
        py::bind_vector<std::vector<kvt::Order>>(m, "Orders");

        py::class_<kvt::Update>(m, "Update")
            .def(py::init<>())
            .def_readwrite("market_updates", &kvt::Update::market_updates);

        py::class_<kvt::MarketMaker>(m, "MarketMaker")
            .def(py::init<>())
            .def("handle_update", &kvt::MarketMaker::handle_update)
            .def("handle_fill", &kvt::MarketMaker::handle_fill)
            .def("place_order", &kvt::MarketMaker::place_order)
            .def("get_and_clear_orders", &kvt::MarketMaker::get_and_clear_orders);
    }
}
