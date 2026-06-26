cmake_minimum_required(VERSION 3.14)
project(TelemetryBroker)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch cpp-httplib directly to avoid manual dependency management
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.14.3
)
FetchContent_MakeAvailable(httplib)

add_executable(broker main.cpp)
find_package(Threads REQUIRED)
target_link_libraries(broker PRIVATE httplib::httplib Threads::Threads)
