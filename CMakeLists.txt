cmake_minimum_required(VERSION 3.10)
project(BIKE_timing)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimized by default. "-O2" without -DNDEBUG keeps assert() active in Release.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

find_library(NTL_LIB ntl)
if(NOT NTL_LIB)
    message(FATAL_ERROR "NTL not found")
endif()

# --- library: everything except the file that contains main() ---
add_library(bike_lib STATIC
    src/utils.cpp
    src/random_bike.cpp
    src/decoder.cpp
)
target_include_directories(bike_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bike_lib PUBLIC ${NTL_LIB} gmp pthread m)
target_compile_options(bike_lib PUBLIC -Wall -Wextra)

# --- main executable ---
add_executable(main src/BIKE.cpp)
target_link_libraries(main PRIVATE bike_lib)

# --- experiments: generic harness + one program per experiment ---
add_library(harness STATIC experiments/harness.cpp)
target_include_directories(harness PUBLIC ${CMAKE_SOURCE_DIR}/experiments)
target_link_libraries(harness PUBLIC bike_lib)

add_executable(exp_bit_flip experiments/exp_bit_flip.cpp)
target_link_libraries(exp_bit_flip PRIVATE harness)

add_executable(exp_bgf experiments/exp_bgf.cpp)
target_link_libraries(exp_bgf PRIVATE harness)

# --- unit tests (run with: ctest --test-dir build --output-on-failure) ---
enable_testing()

function(add_unit_test name)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE bike_lib)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# add them as the files appear, e.g.:
# add_unit_test(test_consistency)
