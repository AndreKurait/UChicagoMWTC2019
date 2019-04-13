# CMakeLists.txt

cmake_minimum_required(VERSION 3.11)
set(CMAKE_CXX_STANDARD 17)

project(kvt)

include_directories(".")

find_package(pybind11 REQUIRED)

# ltd-core bindings
pybind11_add_module(
    kvt
    kvt.cpp
    black_scholes.cpp
    lets-be-rational/LetsBeRational.cpp
    lets-be-rational/rationalcubic.cpp
    lets-be-rational/normaldistribution.cpp
    lets-be-rational/erf_cody.cpp
)
target_link_libraries(kvt PRIVATE -L/usr/local/lib)
