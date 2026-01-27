# Example CMakeLists.txt for using ChronosDB in your project

cmake_minimum_required(VERSION 3.10)
project(MyApp)

set(CMAKE_CXX_STANDARD 20)

# Find ChronosDB
# Option 1: If ChronosDB is installed system-wide
# find_package(ChronosDB REQUIRED)

# Option 2: If ChronosDB is in parent directory
set(CHRONOSDB_ROOT "${CMAKE_SOURCE_DIR}/..")
set(CHRONOSDB_INCLUDE_DIR "${CHRONOSDB_ROOT}/src/include")
set(CHRONOSDB_LIBRARY "${CHRONOSDB_ROOT}/build/libchronosdb_lib.a")

# Add executable
add_executable(my_app cpp_example.cpp)

# Include directories
target_include_directories(my_app PRIVATE ${CHRONOSDB_INCLUDE_DIR})

# Link libraries
target_link_libraries(my_app ${CHRONOSDB_LIBRARY})

# Link system libraries
if(WIN32)
    target_link_libraries(my_app ws2_32)
else()
    target_link_libraries(my_app pthread)
endif()
