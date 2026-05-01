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
    std::mutex apiMutex;
    TimePoint lastCall = chrono::steady_clock::now() - chrono::seconds(1);
    const chrono::milliseconds interval{500};
public:
    void apiWait(std::function<void()> run_query);
};

class DatabaseWriter {
private:
    std::mutex dbMutex;
public:
    void db_write();
};

struct AppContext {
    sw::redis::Redis redis;
    ApiClient globalClient;
    DatabaseWriter globalDBWriter;

    AppContext (const std::string &uri) 
    try : redis(uri) {
        // Constructor body
    }
    catch (const sw::redis::Error &e) {
        std::cerr << "Redis error: " << e.what() << std::endl;
        throw;
    }
};

void app_loop(AppContext &app);
std::string i_to_str(int num);
void log_info(const std::string& message);
void log_error(const std::string& message);
void worker_thread(AppContext& app, std::string query, const cpr::Header &headers);
std::string query_scryfall(std::string query, const cpr::Header &headers);
void batch_tasks(std::vector<json> &jsonList, sw::redis::Redis &redis);
std::string email_from_env(std::string path);
cpr::Header format_header(std::string email);
void processResult(const std::string &result);

int main() {
    try {
        AppContext app("tcp://127.0.0.1:6379");
        app_loop(app);
    } catch (const std::exception &e) {
        // Connection error
        std::cerr << "Fatal connection error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
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

void DatabaseWriter::db_write() {
    // Read needed data

    // lock thread
    std::lock_guard<std::mutex> lock(dbMutex);

    // write to DB
}

void app_loop(AppContext &app) {
    cpr::Header headers;
    headers = format_header(email_from_env("/var/www/mtgwebapp/.env"));
    while (true) {

        std::vector<json> jsonList;
        batch_tasks(jsonList, app.redis);

        if (jsonList.empty()) {
            log_error("Redis failure");
            return;
        }

        std::vector<std::thread> threads;
        for (size_t i=0; i < jsonList.size(); ++i) {
            threads.emplace_back(worker_thread, std::ref(app), jsonList[i]["url"], std::ref(headers));
        }

        for(auto& t : threads) t.join();
        log_info("Batch Completed");
    }
}

std::string i_to_str(int num) {
    std::string strNum;
    strNum.reserve(11); // reserving 10 digits worth of space for int value
    char charNum = '0';

    int sizeCheck = num;
    int numSize = 1; // number of digits in input decimal number
    while (sizeCheck > 10) {
        sizeCheck /= 10;
        numSize++;
    }

    // mod num to isolate lowest digit and concat to strNum, divide by 10 to asr num
    for (int i=0; i < numSize; ++i) {
        strNum += ('0' + (num % 10));
        num /= 10;
    }

    // Reverse string to put digit sequence back in order
    std::reverse(strNum.begin(), strNum.end());
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

void worker_thread(AppContext &app, std::string query, const cpr::Header &headers) {
    std::string result;
    app.globalClient.apiWait([&]() {
        result = query_scryfall(query, headers);
    });

    // Parse query result into json
    json parsedResult = json::parse(result);
    log_info(parsedResult["name"]);

    // donwload file
    std::string fileEndpoint = parsedResult["image_uris"]["normal"]; // Might not be normal, double check python

    // Do download

    // DB write
    {
        // call function and pass &json?
    }
}

std::string query_scryfall(std::string query, const cpr::Header &headers) {
    std::string test = "Running query: " + query;
    log_info(test);

    cpr::Response response = cpr::Get(cpr::Url{query},
                                cpr::Header{headers});

    json results = json::parse(response.text);
    
    return response.text;
}

void batch_tasks(std::vector<json> &jsonList, sw::redis::Redis &redis) {
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

void processResult(const std::string &result) {
    json parsedResult = json::parse(result);
    log_info(parsedResult["name"]);
}