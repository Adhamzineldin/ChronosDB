cmake_minimum_required(VERSION 3.10)
project(FrancoDB)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-unused-parameter")

# ==========================================
# 1. SETUP PATHS
# ==========================================
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(TEST_DIR "${CMAKE_SOURCE_DIR}/test")

# ==========================================
# 2. BUILD THE LIBRARY (The Engine Core)
# ==========================================
# Find all source files in src/
file(GLOB_RECURSE FRANCODB_SOURCES CONFIGURE_DEPENDS "${SRC_DIR}/*.cpp")
file(GLOB_RECURSE FRANCODB_HEADERS CONFIGURE_DEPENDS "${SRC_DIR}/include/*.h")

# CRITICAL: Exclude the App Entry Points (Server & Shell) from the Library
# We don't want 'main()' functions inside our library.
list(FILTER FRANCODB_SOURCES EXCLUDE REGEX ".*/src/cmd/.*")

add_library(francodb_lib
        ${FRANCODB_SOURCES}
        ${FRANCODB_HEADERS}
)

target_include_directories(francodb_lib PUBLIC "${SRC_DIR}/include")

# ==========================================
# 3. BUILD THE SERVER (Networking Daemon)
# ==========================================
# Path to Server Main
set(SERVER_SRC "${SRC_DIR}/cmd/server/main.cpp")

if(EXISTS "${SERVER_SRC}")
    add_executable(francodb_server "${SERVER_SRC}")
    target_link_libraries(francodb_server francodb_lib)

    # Link Networking Libraries (Windows vs Linux)
    if(WIN32)
        target_link_libraries(francodb_server ws2_32)
    else()
        target_link_libraries(francodb_server pthread)
    endif()
else()
    message(WARNING "Server source not found at: ${SERVER_SRC}")
endif()

# ==========================================
# 4. BUILD THE SHELL (Interactive CLI)
# ==========================================
# Path to Shell Main
set(SHELL_SRC "${SRC_DIR}/cmd/shell/shell.cpp")

if(EXISTS "${SHELL_SRC}")
    add_executable(francodb_shell "${SHELL_SRC}")
    target_link_libraries(francodb_shell francodb_lib)

    # Link Networking Libraries (Windows vs Linux)
    if(WIN32)
            target_link_libraries(francodb_shell ws2_32)
    else()
        target_link_libraries(francodb_shell pthread)
    endif()
else()
    message(WARNING "Shell source not found at: ${SHELL_SRC}")
endif()

# ==========================================
# 5. BUILD THE SETUP UTILITY
# ==========================================
# Path to Setup Main
set(SETUP_SRC "${SRC_DIR}/cmd/setup/setup.cpp")

if(EXISTS "${SETUP_SRC}")
    add_executable(francodb_setup "${SETUP_SRC}")

    # Link Networking Libraries (Windows vs Linux)
    if(WIN32)
        target_link_libraries(francodb_setup ws2_32)
    else()
        target_link_libraries(francodb_setup pthread)
    endif()
else()
    message(WARNING "Setup source not found at: ${SETUP_SRC}")
endif()

# ==========================================
# 6. BUILD THE WINDOWS SERVICE WRAPPER
# ==========================================
# Path to Service Wrapper Main (Windows only)
set(SERVICE_SRC "${SRC_DIR}/cmd/service/service_wrapper.cpp")

if(WIN32 AND EXISTS "${SERVICE_SRC}")
    add_executable(francodb_service "${SERVICE_SRC}")

    # Set Windows subsystem
    set_target_properties(francodb_service PROPERTIES
        WIN32_EXECUTABLE FALSE
    )

    # Link Windows libraries
    target_link_libraries(francodb_service advapi32)
else()
    if(NOT WIN32)
        message(STATUS "Service wrapper is Windows-only, skipping on this platform")
    else()
        message(WARNING "Service wrapper source not found at: ${SERVICE_SRC}")
    endif()
endif()

# ==========================================
# 7. BUILD TESTS
# ==========================================
add_subdirectory(test)