cmake_minimum_required(VERSION 3.22)
project(perf-counter-monitor)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)

FetchContent_Declare(argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG af442b4da0cd7a07b56fa709bd16571889dc7fda # v3.0
  GIT_PROGRESS   TRUE
  GIT_SHALLOW    TRUE
  EXCLUDE_FROM_ALL
)
message(STATUS "Fetching and configuring argparse...")
FetchContent_MakeAvailable(argparse)

FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/arthursonzogni/ftxui.git
  GIT_TAG        5cfed50702f52d51c1b189b5f97f8beaf5eaa2a6 #v6.1.9
  GIT_PROGRESS   TRUE
  GIT_SHALLOW    TRUE
  EXCLUDE_FROM_ALL
)
message(STATUS "Fetching and configuring FTXUI...")
FetchContent_MakeAvailable(ftxui)

add_executable(perf-counter-monitor src/main.cpp src/perf.cpp)
target_link_libraries(perf-counter-monitor PRIVATE
  argparse
  ftxui::component
  ftxui::screen
  ftxui::dom
)
