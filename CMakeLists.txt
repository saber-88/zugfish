
cmake_minimum_required(VERSION 3.21)

# Target a modern Windows version for dependencies such as cURL.
if(WIN32)
    set(CMAKE_SYSTEM_VERSION "10.0")
endif()

project(zugfish VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


option(BUILD_GUI "Build Qt6 GUI application" ON)
option(BUILD_CLI "Build CLI tool" ON)

include(FetchContent)

# CURL: find system package or fetch automatically
find_package(CURL QUIET)
if(NOT CURL_FOUND)
    message(STATUS "System CURL not found; fetching via FetchContent...")
    set(BUILD_CURL_EXE OFF CACHE INTERNAL "")
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
    set(CURL_USE_OPENSSL OFF CACHE INTERNAL "")
    if(WIN32)
        set(CURL_USE_SCHANNEL ON CACHE INTERNAL "")
    endif()
    set(HTTP_ONLY ON CACHE INTERNAL "")
    FetchContent_Declare(
        curl
        URL https://github.com/curl/curl/releases/download/curl-8_11_0/curl-8.11.0.tar.xz
    )
    FetchContent_MakeAvailable(curl)
    if(NOT TARGET CURL::libcurl)
    if(TARGET libcurl_static)
        add_library(CURL::libcurl ALIAS libcurl_static)
    elseif(TARGET libcurl)
        add_library(CURL::libcurl ALIAS libcurl)
    endif()
endif()
endif()

# nlohmann_json: find system package or fetch automatically
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "System nlohmann_json not found; fetching via FetchContent...")
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Core Library
add_library(zugfish_core STATIC
    core/uci_engine.cpp
    core/pgn_parser.cpp
    core/reviewer.cpp
    core/opening_book.cpp
    core/polyglot.cpp
    core/online_importer.cpp
)
target_include_directories(zugfish_core PUBLIC core third_party)
target_link_libraries(zugfish_core PUBLIC CURL::libcurl nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(zugfish_core PUBLIC ws2_32 crypt32)
endif()
target_compile_options(zugfish_core PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>)

# Ensure data files (polyglot book & openings database) are copied to build dir
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data)
file(COPY ${CMAKE_SOURCE_DIR}/data/ DESTINATION ${CMAKE_BINARY_DIR}/data)

# CLI Executable
if(BUILD_CLI)
    add_executable(zugfish-cli cli/main.cpp)
    target_link_libraries(zugfish-cli PRIVATE zugfish_core)
endif()

# Qt GUI Executable
if(BUILD_GUI)
    add_subdirectory(gui)
endif()

include(GNUInstallDirs)
if(BUILD_CLI)
    install(TARGETS zugfish-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()