cmake_minimum_required(VERSION 3.14)
project(TelemetryAgent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.14.3
)
FetchContent_MakeAvailable(httplib)

add_executable(agent main.cpp)
target_link_libraries(agent PRIVATE httplib::httplib Threads::Threads)
