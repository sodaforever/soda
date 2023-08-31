
cmake_minimum_required(VERSION 3.10)

project(soda LANGUAGES CXX)

# C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# compiler setting
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG ")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
endif()

# output dir
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/static)

# thread
find_package(Threads REQUIRED)
# openssl
find_package(OpenSSL REQUIRED)

# include
include_directories(${PROJECT_SOURCE_DIR}/include)

# lib
# link_directories(/usr/lib)

# sub dir
# if each die has CMakeLists.txt
# add_subdirectory(subdir1)
# add_subdirectory(subdir2)

# src 
file(GLOB_RECURSE SOURCES "./src/*.cpp")

# excutable
add_executable(soda ${SOURCES})

# link
target_link_libraries(soda PRIVATE Threads::Threads mysqlclient OpenSSL::SSL OpenSSL::Crypto)
