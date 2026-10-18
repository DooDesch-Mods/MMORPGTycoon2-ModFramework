cmake_minimum_required(VERSION 3.24)
project(ExampleNativePatches LANGUAGES CXX)
add_library(ExampleNativePatches SHARED plugin.cpp)
target_compile_features(ExampleNativePatches PRIVATE cxx_std_17)
target_include_directories(ExampleNativePatches PRIVATE .mt2fw .mt2fw/native/mt2)
set_target_properties(ExampleNativePatches PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_SOURCE_DIR}>")
