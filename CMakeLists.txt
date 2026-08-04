cmake_minimum_required(VERSION 3.23.0)
project(Chrony LANGUAGES C CXX)

# Helpers
set(IS_GCC      "$<CXX_COMPILER_ID:GNU>")
set(IS_CLANG    "$<CXX_COMPILER_ID:Clang>")
set(IS_MSVC     "$<CXX_COMPILER_ID:MSVC>")
set(IS_NOT_MSVC "$<NOT:${IS_MSVC}>")

# Dependencies
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CPM_SOURCE_CACHE ${CMAKE_SOURCE_DIR}/.cache/cpm CACHE PATH "Directory to cache CPM packages" FORCE)
include(cmake/CPM.cmake)

CPMAddPackage(
  NAME Boost
  VERSION 1.91.0
  URL https://github.com/boostorg/boost/releases/download/boost-1.91.0-1/boost-1.91.0-1-cmake.7z
  URL_HASH SHA256=29c7d4f4ac36ad853b6765d03571ea60d90286775df026b4efd9f3281131972b
  OPTIONS "BUILD_SHARED_LIBS OFF" 
          "BOOST_ENABLE_CMAKE ON"
          "BOOST_ASIO_DISABLE_BOOST_COROUTINE ON"
          "BOOST_INCLUDE_LIBRARIES asio\\\;program_options")

# Options
option(CHRONY_ENABLE_SANITIZERS "Build unit tests with sanitizers" OFF)

# Lib
add_library(chrony)
add_library(chrony::chrony ALIAS chrony)
target_sources(chrony PRIVATE src/lib/chrony.cpp)
target_compile_features(chrony PUBLIC cxx_std_20)
target_compile_options(chrony PRIVATE $<${IS_NOT_MSVC}:-Wall -Wextra -Wpedantic -Werror>)
target_link_libraries(chrony PUBLIC Boost::asio)

target_include_directories(chrony
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lib>
  PUBLIC $<INSTALL_INTERFACE:include>)

# Example
add_executable(client src/examples/client.cpp)
target_link_libraries(client PRIVATE chrony::chrony)
target_link_options(client PRIVATE $<$<CONFIG:Release>:-Wl,--gc-sections;-Wl,--as-needed;-s>)

# # Test
# add_executable(chrony_tests 
#   src/tests/doctest.cpp
#   src/tests/encodec.cpp)
# target_link_libraries(encodec_tests PRIVATE encodec::encodec)

if (CHRONY_ENABLE_SANITIZERS)
  message(STATUS "Enabling sanitizers on unit tests")
  target_compile_options(chrony PUBLIC $<$<OR:${IS_GCC},${IS_CLANG}>:-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined>)
  target_link_options(chrony PUBLIC $<$<OR:${IS_GCC},${IS_CLANG}>:-fsanitize=address,undefined>)
endif()