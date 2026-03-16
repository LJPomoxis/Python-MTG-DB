cmake_minimum_required(VERSION 3.15)
project(MTGWebApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the packages
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(redis-plus-plus CONFIG REQUIRED)
find_package(mariadb-connector-cpp CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_executable(MTGWebApp main.cpp)

# Link them
target_link_libraries(MTGWebApp 
    PRIVATE 
        fmt::fmt
        nlohmann_json::nlohmann_json
        redis-plus-plus::redis-plus-plus
        mariadb-connector-cpp::mariadb-cppconn
        Threads::Threads
)