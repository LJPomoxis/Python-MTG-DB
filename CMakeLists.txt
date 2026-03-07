cmake_minimum_required(VERSION 3.10)
project(RedisToMariaDB_DAL)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(MariaDB REQUIRED)

find_path(REDIS_PLUS_PLUS_INCLUDE_DIR sw/redis++/redis++.h)
find_library(REDIS_PLUS_PLUS_LIBRARY redis++)
find_library(HIREDIS_LIBRARY hiredis)

find_package(nlohmann_json REQUIRED)

add_executable(dal_service main.cpp)

# linking
target_include_directories(dal_service PRIVATE 
    ${REDIS_PLUS_PLUS_INCLUDE_DIR}
    /usr/include/mariadb
)

target_link_libraries(dal_service PRIVATE 
    ${REDIS_PLUS_PLUS_LIBRARY} 
    ${HIREDIS_LIBRARY}
    mariadbcpp
    nlohmann_json::nlohmann_json
)