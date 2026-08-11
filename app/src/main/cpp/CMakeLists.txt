cmake_minimum_required(VERSION 3.22.1)
project("hotpath")

add_library(hotpath SHARED
    hotpath.cpp)

find_library(log-lib log)

target_link_libraries(hotpath
    ${log-lib})

# Keep this a RelWithDebInfo-style build in spirit: optimize like a real
# release, but don't strip -- CI will split debug info afterward the same
# way it did for ARMSX3, so the profiling behavior actually matches what
# ships, not an -O0 debug build.
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
