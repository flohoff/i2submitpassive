cmake_minimum_required (VERSION 3.25)
project (i2submitpassive)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include(FindPkgConfig)
pkg_check_modules(CURLPP REQUIRED curlpp)

find_package(Boost 1.55.0 REQUIRED COMPONENTS program_options)
include_directories(${Boost_INCLUDE_DIRS})

find_path(JSON_INCLUDE_DIR
        NAMES "nlohmann/json.hpp"
        PATHS
        /usr/include/
        /usr/include/
        ${CMAKE_SOURCE_DIR}/json/include
        ${CMAKE_SYSROOT}/usr/local/include
        )
message("nlohmann directory ${JSON_INCLUDE_DIR}")

add_executable(i2submitpassive i2submitpassive.cpp)
target_link_libraries(i2submitpassive ${Boost_LIBRARIES} ${CURLPP_LDFLAGS})
