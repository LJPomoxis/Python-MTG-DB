#include <iostream>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <thread>
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <sw/redis++/redis++.h>
#include <mariadb/conncpp.hpp>
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>

using TimePoint = std::chrono::steady_clock::time_point;
using json = nlohmann::json;
namespace chrono = std::chrono;

const std::string VERSION = "0.1.0";

class ApiClient {
private:
    std::mutex api_mutex;
    TimePoint lastCall = chrono::steady_clock::now() - chrono::seconds(1);
    const chrono::milliseconds interval{500};
public:
    void wait(std::function<void()> run_query);
};

std::string i_to_str(int num);
void log_info(const std::string& message);
void log_error(const std::string& message);
void worker_thread(ApiClient& client, std::string query, const cpr::Header &headers);
void query_scryfall(std::string query, const cpr::Header &headers);
void batch_tasks(std::vector<json> &jsonList);
std::string email_from_env(std::string path);
cpr::Header format_header(std::string email);

int main() {
    // Will also need to handle other async tasks with new threads
    // ensure api query function is atomic to respect rate limit for scryfall
    cpr::Header headers;
    headers = format_header(email_from_env("/var/www/mtgwebapp/.env"));
    while (true) {
        ApiClient GlobalClient;

        std::vector<json> jsonList;
        batch_tasks(jsonList);

        if (jsonList.empty()) {
            log_error("Redis failure");
            return 1;
        }

        std::vector<std::thread> threads;
        for (size_t i=0; i < jsonList.size(); ++i) {
            threads.emplace_back(worker_thread, std::ref(GlobalClient), jsonList[i]["url"], headers);
        }

        for(auto& t : threads) t.join();
        log_info("Batch Completed");
    }
    return 0;
}

void ApiClient::wait(std::function<void()> run_query) {
        std::lock_guard<std::mutex> lock(api_mutex);

        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastCall);

        if(elapsed < interval) {
            // wait out remainder of timer
           std::this_thread::sleep_for(interval - elapsed);
        }

        run_query();
        lastCall = chrono::steady_clock::now();
    }

std::string i_to_str(int num) { // Assumes int < 100
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

void log_info(const std::string& message) { // thread safe logging, prints log as one line
    auto now = chrono::system_clock::now();
    std::cout << fmt::format("[INFO] {:%F %T} - {}\n", now, message);
}

void log_error(const std::string& message) {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::cerr << fmt::format("[ERROR] {:%F %T} - {}\n", now, message);
}

void worker_thread(ApiClient& client, std::string query, const cpr::Header &headers) {
    client.wait([&]() {
        query_scryfall(query, headers);
    });
}

void query_scryfall(std::string query, const cpr::Header &headers) {
    std::string test = "Running query: " + query;
    log_info(test);

    cpr::Response response = cpr::Get(cpr::Url{query},
                                cpr::Header{headers});

    //std::cout << response.url << std::endl;
    std::cout << response.text << std::endl;
}

void batch_tasks(std::vector<json> &jsonList) {
    using namespace sw::redis;
    try {
        auto redis = Redis("tcp://127.0.0.1:6379");

        auto task = redis.brpop("mtgdb_queue", 0);
        json data = json::parse(task->second);
        jsonList.push_back(data);
        log_info("Got initial redis task");

        const chrono::seconds timeOut{8}; // arbitrary value, tweak as needed
        auto start = chrono::steady_clock::now();

        size_t targetSize = 5;
        while (chrono::steady_clock::now() - start < timeOut && jsonList.size() < targetSize) {
            auto next_task = redis.rpop("mtgdb_queue");
            if (next_task) {
                json data = json::parse(*next_task);
                jsonList.push_back(data);
            }
        }
    } catch (const Error &e) {
        // Connection error
        std::cerr << "Redis error: " << e.what() << std::endl;
        return;
    }
}

std::string email_from_env(std::string path) {
    std::ifstream envFile;
    envFile.open(path);
    if (!envFile.is_open()) {
        log_error("Failed to open env file");
    }
    std::string buf;
    while (std::getline(envFile, buf)) { 
        //log_info(buf); 
    }
    envFile.close();

    char tmp;
    std::string email;
    int position = 0;
    while (buf[position] != '\"') position++;

    position++;
    while (buf[position] != '\"') {
        email += buf[position];
        position++;
    }

    return email;
}

cpr::Header format_header(std::string email) {
    cpr::Header headers;

    std::string userAgent = fmt::format("mtgDBManagerScript/{} ({})", email, VERSION);
    headers["User-Agent"] = userAgent;
    std::string accpt = "application/json";
    headers["Accept"] = accpt;

    return headers;
}