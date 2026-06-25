#pragma once

#include <cpr/cpr.h>

using TimePoint = std::chrono::steady_clock::time_point;
namespace chrono = std::chrono;

namespace requests {

    std::string query_scryfall(std::string query, const cpr::Header &headers); // function for scryfall query
    cpr::Header format_header(std::string email); // formats headers for scryfall query using email and version number

    class ApiClient {
    private:
        std::mutex apiMutex;
        TimePoint lastCall = chrono::steady_clock::now() - chrono::seconds(1);
        const chrono::milliseconds interval{500};
    public:
        void apiWait(std::function<void()> run_query);
    };

}