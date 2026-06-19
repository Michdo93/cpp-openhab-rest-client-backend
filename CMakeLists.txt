cmake_minimum_required(VERSION 3.16)
project(OpenHABBackend VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dependencies ──────────────────────────────────────────────────────────────

find_package(CURL REQUIRED)

# nlohmann/json
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
  include(FetchContent)
  FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(json)
endif()

# Crow – lightweight C++ HTTP server (header-only)
FetchContent_Declare(crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG        v1.2.0)
set(CROW_USE_BOOST OFF CACHE BOOL "" FORCE)
set(CROW_ENABLE_SSL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(crow)

FetchContent_Declare(asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        asio-1-30-2)
FetchContent_MakeAvailable(asio)

add_library(asio_interface INTERFACE)
target_include_directories(asio_interface INTERFACE
  ${asio_SOURCE_DIR}/asio/include)

# openHAB REST Client library (as submodule / subdirectory)
add_subdirectory(extern/cpp-openhab-rest-client)

# ── Backend executable ────────────────────────────────────────────────────────

add_executable(openhab_backend src/main.cpp)

target_link_libraries(openhab_backend PRIVATE
  Crow::Crow
  asio_interface
  openhab_rest_client_static
  ${CURL_LIBRARIES}
  nlohmann_json::nlohmann_json
  pthread)

# Crow needs these on Linux
if(UNIX)
  target_link_libraries(openhab_backend PRIVATE pthread)
endif()
