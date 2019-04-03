# CMakeLists.txt

cmake_minimum_required(VERSION 3.11)
set(CMAKE_CXX_STANDARD 17)

project(kvt)

include_directories(".")

find_package(pybind11 REQUIRED)
find_package(protobuf REQUIRED)
link_directories(${protobuf_LIBRARY_DIRS})

# ltd-core bindings
pybind11_add_module(
    kvt
    kvt.cpp
    protos/service.pb.cc
    protos/competitor.pb.cc
    protos/exchange.pb.cc
    protos/order_book.pb.cc
)
target_link_libraries(kvt PRIVATE -L/usr/local/lib -lprotobuf)
