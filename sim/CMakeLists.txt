cmake_minimum_required(VERSION 2.8.12)
project(chimbuko_sim)

find_package(PythonInterp REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

add_subdirectory(pybind11)
pybind11_add_module(ad_params src/ad_params.cpp)
#pybind11_add_module(thread_example thread_example.cpp)
