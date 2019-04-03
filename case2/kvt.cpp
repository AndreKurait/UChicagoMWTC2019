#include <pybind11/pybind11.h>

#include "MarketMaker.h"

namespace py = pybind11;
namespace kvt::pybind {
    PYBIND11_MODULE(kvt, m) {
        py::class_<kvt::MarketMaker>(m, "MarketMaker")
            .def(py::init<>())
            .def("handle_update", &kvt::MarketMaker::handle_update);
    }
}
