cmake_minimum_required(VERSION 3.16)
project(heap_manager C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(HM_BUILD_SERVER "Build demo server" ON)
option(HM_BUILD_TESTS  "Build tests" ON)

add_library(heap_manager
    src/heap_manager.c
    src/segment.c
    src/freelist_nextfit.c
    src/metrics.c
    src/platform.c
)

target_include_directories(heap_manager PUBLIC include)

if(HM_BUILD_SERVER)
  add_subdirectory(server)
endif()

if(HM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
