cmake_minimum_required(VERSION 3.20)
project(bible-cpp
    VERSION 0.1.0
    DESCRIPTION "Pure C++20 parallel-text scripture engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(BIBLE_BUILD_TESTS "Build tests" ON)

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

add_library(bible STATIC
    src/bible.cpp
    src/reference.cpp
    src/strongs_parser.cpp
    src/stats.cpp
)

add_library(bible::bible ALIAS bible)

target_include_directories(bible
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(bible PUBLIC cxx_std_20)

# SQLite: find system or vendored
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    target_link_libraries(bible PRIVATE SQLite::SQLite3)
else()
    message(STATUS "SQLite3 not found via find_package; expecting vendored sqlite3")
    # Vendored SQLite will be added here when available
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

include(GNUInstallDirs)

install(TARGETS bible
    EXPORT BibleCppTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/bible
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT BibleCppTargets
    FILE BibleCppTargets.cmake
    NAMESPACE bible::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bible-cpp
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(BIBLE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
