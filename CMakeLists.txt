cmake_minimum_required(VERSION 3.16)
project(OpenHABBackend VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

find_package(CURL REQUIRED)

# nlohmann/json
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(json)
endif()

# cpp-httplib (header-only)
FetchContent_Declare(httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.18.3)
FetchContent_MakeAvailable(httplib)

# openHAB REST Client library
add_subdirectory(extern/cpp-openhab-rest-client)

# Backend executable
add_executable(openhab_backend src/main.cpp)

target_include_directories(openhab_backend PRIVATE
  ${httplib_SOURCE_DIR})

target_link_libraries(openhab_backend PRIVATE
  openhab_rest_client_static
  ${CURL_LIBRARIES}
  nlohmann_json::nlohmann_json
  pthread)