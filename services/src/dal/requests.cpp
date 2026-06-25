#include "dal/requests.hpp"
#include "common/utils.hpp"
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <mutex>

namespace requests {

    const std::string VERSION = "0.1.0";

    std::string query_scryfall(std::string query, const cpr::Header &headers) {
        //std::string test = "Running query: " + query;
        //utils::log_info(test);

        cpr::Response response = cpr::Get(cpr::Url{query},
                                    cpr::Header{headers});

        //json results = json::parse(response.text);
        
        return response.text;
    }

    cpr::Header format_header(std::string email) {
        cpr::Header headers;

        std::string userAgent = fmt::format("mtgDBManagerScript/{} ({})", email, VERSION);
        headers["User-Agent"] = userAgent;
        std::string accpt = "application/json";
        headers["Accept"] = accpt;

        return headers;
    }

    void ApiClient::apiWait(std::function<void()> run_query) {
        // Block scope to enforce timing on API calls
        {
            std::lock_guard<std::mutex> lock(apiMutex);

            auto now = chrono::steady_clock::now();
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastCall);

            if(elapsed < interval) {
                // wait out remainder of timer
                std::this_thread::sleep_for(interval - elapsed);
            }
            lastCall = chrono::steady_clock::now();
        }
        run_query();
    }
}