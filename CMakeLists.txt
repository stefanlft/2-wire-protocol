cmake_minimum_required(VERSION 3.10)

# ----------------------------
# App name
# ----------------------------
set(APP_NAME twp)
project(${APP_NAME} C)  # Changed to C language

set(CMAKE_C_STANDARD 11)           # Set C standard to C11
set(CMAKE_C_STANDARD_REQUIRED ON)  # Enforce C11 standard

# Find zlib package
find_package(ZLIB REQUIRED)

# ----------------------------
# Source files
# ----------------------------
file(GLOB SRC_FILES src/*.c)  # Use C source files

add_executable(${APP_NAME} ${SRC_FILES})

# Link zlib to your target
target_link_libraries(${PROJECT_NAME} ZLIB::ZLIB)


set_target_properties(${APP_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ----------------------------
# Formatting check (FAIL build if bad formatting)
# ----------------------------

# Collect *all* source/header files for format checking
file(GLOB_RECURSE ALL_SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/src/*.c"
    "${CMAKE_SOURCE_DIR}/src/*.h"
)

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if (NOT CLANG_FORMAT_EXE)
    message(FATAL_ERROR "clang-format not found! Install it first.")
endif()

# Check formatting (does NOT write changes)
add_custom_target(format-check
    COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${ALL_SOURCE_FILES}
    COMMENT "Checking formatting (clang-format)…"
    VERBATIM
)

# Always run format-check before compiling main app
add_dependencies(${APP_NAME} format-check)

# Optional: A separate "format" target to auto-fix formatting
add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_SOURCE_FILES}
    COMMENT "Auto-formatting source files"
)

# ----------------------------
# Fetch CMocka
# ----------------------------
include(FetchContent)
FetchContent_Declare(
    cmocka
    GIT_REPOSITORY https://git.cryptomilk.org/projects/cmocka.git
    GIT_TAG master
)

# Fetch and build CMocka
FetchContent_MakeAvailable(cmocka)

# ----------------------------
# Unit Test Setup
# ----------------------------
enable_testing()

# Collect test source files (make sure the files are in a `tests/` folder)
file(GLOB TEST_FILES tests/*.c)

# Create the test executable
add_executable(${APP_NAME}_tests ${TEST_FILES})
target_link_libraries(${APP_NAME}_tests cmocka)

# Include CMocka headers
target_include_directories(${APP_NAME}_tests PRIVATE ${cmocka_SOURCE_DIR}/src)

# Add the test to CMake's test runner
add_test(NAME ${APP_NAME}_tests COMMAND ${APP_NAME}_tests)
