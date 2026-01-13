cmake_minimum_required(VERSION 3.10)
project(FrancoDB)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

# ==========================================
# 1. SETUP SOURCES
# ==========================================
# Use absolute paths to avoid "No SOURCES given" errors
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(TEST_DIR "${CMAKE_SOURCE_DIR}/test")

# Find all source files
file(GLOB_RECURSE FRANCODB_SOURCES CONFIGURE_DEPENDS "${SRC_DIR}/*.cpp")
file(GLOB_RECURSE FRANCODB_HEADERS CONFIGURE_DEPENDS "${SRC_DIR}/include/*.h")

# Exclude main.cpp and shell.cpp from the Library (Lib should only be the engine)
list(FILTER FRANCODB_SOURCES EXCLUDE REGEX ".*/main.cpp$")
list(FILTER FRANCODB_SOURCES EXCLUDE REGEX ".*/shell.cpp$")

# ==========================================
# 2. BUILD THE LIBRARY (Kernel)
# ==========================================
add_library(francodb_lib
        ${FRANCODB_SOURCES}
        ${FRANCODB_HEADERS}
)

target_include_directories(francodb_lib PUBLIC "${SRC_DIR}/include")

# ==========================================
# 3. BUILD THE SHELL (Application)
# ==========================================
if(EXISTS "${SRC_DIR}/shell.cpp")
    add_executable(francodb_shell "${SRC_DIR}/shell.cpp")
    target_link_libraries(francodb_shell francodb_lib)
else()
    message(WARNING "src/shell.cpp not found! Skipping shell build.")
endif()

# ==========================================
# 4. BUILD TESTS
# ==========================================
# Helper macro to add tests easily
macro(add_franco_test test_name file_path)
    if(EXISTS "${file_path}")
        add_executable(${test_name} "${file_path}")
        target_link_libraries(${test_name} francodb_lib)
    endif()
endmacro()

add_franco_test(parser_test "${TEST_DIR}/parser/parser_test.cpp")
add_franco_test(lexer_test "${TEST_DIR}/parser/lexer_test.cpp")
add_franco_test(execution_test "${TEST_DIR}/execution/execution_test.cpp")
add_franco_test(index_execution_test "${TEST_DIR}/execution/index_execution_test.cpp")
add_franco_test(b_plus_tree_test "${TEST_DIR}/index/b_plus_tree_test.cpp")
add_franco_test(b_plus_tree_split_test "${TEST_DIR}/index/b_plus_tree_split_test.cpp")
