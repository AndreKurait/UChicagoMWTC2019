#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>

#include "MarketMaker.h"

/*
    struct Order {
        enum class OrderType {
            Market,
            Limit
        };

        std::string asset;
        int qty;
        OrderType type;
        float price;
        // competitor identifier
        std::string order_id;
    };

    struct Fill {

    };

    struct Asset {
        std::string asset_code;
        // min_tick_size
        // max_position
    };

    struct PriceLevel {
        int size;
        float price;
    }

    struct MarketUpdate {
        Asset asset;
        float mid_market_price;
        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;
    };

    struct Update {
        std::vector<Fill> fills;
        std::vector<MarketUpdate> market_updates;
    };
*/

namespace py = pybind11;
namespace kvt::pybind {
    PYBIND11_MODULE(kvt, m) {
        py::class_<kvt::Asset>(m, "Asset")
            .def(py::init<>())
            .def_readwrite("asset_code", &kvt::Asset::asset_code);

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

        py::class_<kvt::Update>(m, "Update")
            .def(py::init<>())
            .def_readwrite("market_updates", &kvt::Update::market_updates);

        py::class_<kvt::MarketMaker>(m, "MarketMaker")
            .def(py::init<>())
            .def("handle_update", &kvt::MarketMaker::handle_update);
    }
}
