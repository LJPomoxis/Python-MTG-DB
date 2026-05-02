cmake_minimum_required(VERSION 3.15) 
project(MTGWebApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(VCPKG_MANIFEST_MODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/curlpp")

# Find the packages
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_package(unofficial-mariadb-connector-cpp CONFIG REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(unofficial-curlpp CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)

add_executable(MTGWebApp main.cpp)

# Link them
target_link_libraries(MTGWebApp 
    PRIVATE 
        fmt::fmt
        nlohmann_json::nlohmann_json
        redis++::redis++_static
        unofficial::mariadb-connector-cpp::mariadbcpp
        Threads::Threads
        unofficial::curlpp::curlpp
        cpr::cpr
)