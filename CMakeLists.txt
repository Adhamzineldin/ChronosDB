cmake_minimum_required(VERSION 3.10)
project(ChronosDB)

set(CMAKE_CXX_STANDARD 20)

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(TEST_DIR "${CMAKE_SOURCE_DIR}/test")

# =========================================================
# 1. CORE LIBRARY (The Engine)
# =========================================================
file(GLOB_RECURSE CHRONOSDB_SOURCES CONFIGURE_DEPENDS "${SRC_DIR}/*.cpp")
file(GLOB_RECURSE CHRONOSDB_HEADERS CONFIGURE_DEPENDS "${SRC_DIR}/include/*.h")
# Exclude main() files from the library to avoid "multiple definition of main" errors
list(FILTER CHRONOSDB_SOURCES EXCLUDE REGEX ".*/src/cmd/.*")

add_library(chronosdb_lib ${CHRONOSDB_SOURCES} ${CHRONOSDB_HEADERS})
# Add compiler-appropriate warning flags per target
if(MSVC)
    target_compile_options(chronosdb_lib PRIVATE /W4)
else()
    target_compile_options(chronosdb_lib PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_include_directories(chronosdb_lib PUBLIC "${SRC_DIR}/include")

# =========================================================
# 2. SERVER EXECUTABLE
# =========================================================
set(SERVER_SRC "${SRC_DIR}/cmd/server/main.cpp")
if(EXISTS "${SERVER_SRC}")
    add_executable(chronosdb_server "${SERVER_SRC}")
    target_link_libraries(chronosdb_server chronosdb_lib)
    if(WIN32)
        target_link_libraries(chronosdb_server ws2_32)
    else()
        target_link_libraries(chronosdb_server pthread)
    endif()
endif()

# =========================================================
# 3. SHELL EXECUTABLE (CLI)
# =========================================================
set(SHELL_SRC "${SRC_DIR}/cmd/shell/shell.cpp")
if(EXISTS "${SHELL_SRC}")
    add_executable(chronosdb_shell "${SHELL_SRC}")
    target_link_libraries(chronosdb_shell chronosdb_lib)
    if(WIN32)
        target_link_libraries(chronosdb_shell ws2_32)
    else()
        target_link_libraries(chronosdb_shell pthread)
    endif()
endif()

# =========================================================
# 4. UTILITIES (Setup & Service)
# =========================================================
set(SETUP_SRC "${SRC_DIR}/cmd/setup/setup.cpp")
if(EXISTS "${SETUP_SRC}")
    add_executable(chronosdb_setup "${SETUP_SRC}")
    if(WIN32)
        target_link_libraries(chronosdb_setup ws2_32)
    else()
        target_link_libraries(chronosdb_setup pthread)
    endif()
endif()

set(SERVICE_SRC "${SRC_DIR}/cmd/service/service_wrapper.cpp")
if(WIN32 AND EXISTS "${SERVICE_SRC}")
    add_executable(chronosdb_service "${SERVICE_SRC}")
    set_target_properties(chronosdb_service PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_libraries(chronosdb_service advapi32)
endif()

# =========================================================
# 5. TESTS (Delegate to test/ folder)
# =========================================================
option(BUILD_TESTING "Build the testing tree." ON)

if(BUILD_TESTING AND EXISTS "${TEST_DIR}")
    add_subdirectory(test)
endif()

# =========================================================
# 6. WINDOWS DLL AUTO-COPY (Fixes "Missing DLL" errors)
# =========================================================
if(WIN32)
    get_filename_component(COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    file(GLOB RUNTIME_DLLS
            "${COMPILER_BIN_DIR}/libstdc++-*.dll"
            "${COMPILER_BIN_DIR}/libgcc_s_*.dll"
            "${COMPILER_BIN_DIR}/libwinpthread-*.dll"
    )
    add_custom_command(TARGET chronosdb_server POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${RUNTIME_DLLS}
            $<TARGET_FILE_DIR:chronosdb_server>
            COMMENT "Auto-copying MinGW Runtime DLLs..."
    )
endif()



# =========================================================
# 7. INSTALLATION CONFIGURATION
# =========================================================
# This makes "cmake --install" actually work!

# Install the Server
if(TARGET chronosdb_server)
    install(TARGETS chronosdb_server RUNTIME DESTINATION bin)
endif()

# Install the Shell (Client)
if(TARGET chronosdb_shell)
    install(TARGETS chronosdb_shell RUNTIME DESTINATION bin)
endif()

# Install Setup Tool (if it exists)
if(TARGET chronosdb_setup)
    install(TARGETS chronosdb_setup RUNTIME DESTINATION bin)
endif()
