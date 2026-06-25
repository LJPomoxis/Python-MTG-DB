cmake_minimum_required(VERSION 3.15)
project(MTGServices)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_package(unofficial-mariadb-connector-cpp CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(unofficial-curlpp CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)

add_library(CommonLib STATIC
    src/common/utils.cpp
)

target_include_directories(CommonLib PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(CommonLib PUBLIC fmt::fmt)

add_executable(DAL
    src/dal/main.cpp
    src/dal/app.cpp 
    src/dal/requests.cpp
)

target_link_libraries(DAL PRIVATE
    CommonLib
    nlohmann_json::nlohmann_json
    redis++::redis++_static
    unofficial::mariadb-connector-cpp::mariadbcpp
    Threads::Threads
    unofficial::curlpp::curlpp
    cpr::cpr
)

#add_executable(cardScanner
#
#)

#target_link_libraries(cardScanner PRIVATE
#    CommonLib  
#)