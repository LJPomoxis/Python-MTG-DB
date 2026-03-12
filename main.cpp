#include <iostream>
#include <iomanip>
#include <mutex>
#include <thread>
#include <fmt/core.h>
#include <sw/redis++/redis++.h>
#include <mariadb-connector-cpp/include/conncpp.hpp>
#include <nlohmann/json.hpp>

using TimePoint = std::chrono::steady_clock::time_point;
namespace chrono = std::chrono;

/*
try {
    using namespace sw::redis;
    auto redis = Redis("tcp://127.0.0.1:6379");
}
*/

class ApiClient {
private:
    std::mutex api_mutex;
    TimePoint lastCall = chrono::steady_clock::now() - chrono::seconds(1);
    const chrono::milliseconds interval{500};
public:
    void wait(std::function<void()> func) {
        std::lock_guard<std::mutex> lock(api_mutex);

        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastCall);

        if(elapsed < interval) {
            // wait out remainder of timer
           std::this_thread::sleep_for(interval - elapsed);
        }

        func();
        lastCall = chrono::steady_clock::now();
    }
};

std::string i_to_str(int num);
void log_info(const std::string& message);
void log_error(const std::string& message);
void worker_thread(ApiClient& client, std::string query);
void query_scryfall(std::string query);

int main() {
    using namespace sw::redis;
    try {
        auto redis = Redis("tcp://127.0.0.1:6379");
    } catch (const Error &e) {
        // Connection error
        std::cout << &e << std::endl;
    }
    using namespace std;
    // loop and check redis queue

    // after intake number met or time increment, execute api query and put results in database

    // Will also need to handle other async tasks with new threads
    // ensure api query function is atomic to respect rate limit for scryfall

    ApiClient GlobalClient;
    bool test = true;

    while(test) {
        vector<thread> threads;

        string query = "example query";
        for(int i=0; i < 5; i++) {
            std::string iterMsg = fmt::format("Thread {} starting", i_to_str(i));
            log_info(iterMsg);
            threads.emplace_back(worker_thread, ref(GlobalClient), query);
        }

        for(auto& t : threads) t.join();
        test = false;
    }
    return 0;
}

std::string i_to_str(int num) {
    std::string strNum = "00";
    char charNum = '0';
    if (num > 9) {
        char top = '0' + (num / 10);
        char bottom = '0' + (num % 10);
        strNum[0] = top;
        strNum[1] = bottom;
    } else {
        charNum += num;
        strNum = charNum;
    }
    return strNum;
}

void log_info(const std::string& message) {
    auto now = chrono::system_clock::now();
    std::cout << fmt::format("[INFO] {:%F %T} - {}\n", now, message);
}

void log_error(const std::string& message) {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::cerr << fmt::format("[ERROR] {:%F %T} - {}\n", now, message);
}

void worker_thread(ApiClient& client, std::string query) {
    client.wait([&]() {
        query_scryfall(query);
    });
}

void query_scryfall(std::string query) {
    std::string result = "Trying: " + query;
    log_info(result);
}