cmake_minimum_required(VERSION 3.10)

# ----------------------------
# App name
# ----------------------------
set(APP_NAME twp)
project(${APP_NAME} C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find zlib package
find_package(ZLIB REQUIRED)

# ----------------------------
# Source files
# ----------------------------
file(GLOB SRC_FILES src/*.c includes/*.h)  # Use C source files

# Create a static library instead of an executable
add_library(${APP_NAME}_lib STATIC ${SRC_FILES})

# Link zlib to the library
target_link_libraries(${APP_NAME}_lib PRIVATE ZLIB::ZLIB)

# Set include directories for the library
target_include_directories(${APP_NAME}_lib PRIVATE
    "${CMAKE_SOURCE_DIR}/includes"   # Include directory for the main project
)

# Set output directory for the library
set_target_properties(${APP_NAME}_lib PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# ----------------------------
# Formatting check (FAIL build if bad formatting)
# ----------------------------
# Collect *all* source/header files for format checking
file(GLOB_RECURSE ALL_SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/src/*.c"
    "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/includes/*.c"
    "${CMAKE_SOURCE_DIR}/includes/*.h"
    "${cmocka_SOURCE_DIR}/*.c"
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
add_dependencies(${APP_NAME}_lib format-check)

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

# Collect test source files
file(GLOB TEST_FILES tests/*.c)

# Create the test executable
add_executable(${APP_NAME}_tests ${TEST_FILES})

# Link the main library and CMocka to the test executable
target_link_libraries(${APP_NAME}_tests
    PRIVATE
    ${APP_NAME}_lib  # Link the static library, not the executable
    cmocka          # Link CMocka for testing
)

# Include CMocka headers and the main project headers
target_include_directories(${APP_NAME}_tests PRIVATE
    "${cmocka_SOURCE_DIR}/src"
    "${cmocka_SOURCE_DIR}/includes"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/includes"
)

# Add the test to CMake's test runner
add_test(NAME ${APP_NAME}_tests COMMAND ${APP_NAME}_tests)

# ----------------------------
# Create the main executable
# ----------------------------
add_executable(${APP_NAME} ${SRC_FILES})

# Link the static library and zlib to the executable
target_link_libraries(${APP_NAME} PRIVATE ${APP_NAME}_lib ZLIB::ZLIB)
