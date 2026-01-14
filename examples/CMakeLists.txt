# Example CMakeLists.txt for using FrancoDB in your project

cmake_minimum_required(VERSION 3.10)
project(MyApp)

set(CMAKE_CXX_STANDARD 20)

# Find FrancoDB
# Option 1: If FrancoDB is installed system-wide
# find_package(FrancoDB REQUIRED)

# Option 2: If FrancoDB is in parent directory
set(FRANCODB_ROOT "${CMAKE_SOURCE_DIR}/..")
set(FRANCODB_INCLUDE_DIR "${FRANCODB_ROOT}/src/include")
set(FRANCODB_LIBRARY "${FRANCODB_ROOT}/build/libfrancodb_lib.a")

# Add executable
add_executable(my_app cpp_example.cpp)

# Include directories
target_include_directories(my_app PRIVATE ${FRANCODB_INCLUDE_DIR})

# Link libraries
target_link_libraries(my_app ${FRANCODB_LIBRARY})

# Link system libraries
if(WIN32)
    target_link_libraries(my_app ws2_32)
else()
    target_link_libraries(my_app pthread)
endif()
