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

std::string i_to_str(int num);
void log_info(const std::string& message); // thread safe logging using stdout
void log_error(const std::string& message); // thread safe logging using cerr
std::string query_scryfall(std::string query, const cpr::Header &headers); // function for scryfall query
void batch_tasks(std::vector<json> &jsonList, sw::redis::Redis &redis); // task manager for watching redis and batching tasks taken from redis queue
std::string get_env_var(std::string path, std::string varName); // pulls email from env file
cpr::Header format_header(std::string email); // formats headers for scryfall query using email and version number
void process_result(const std::string &result); // function for parsing and checking queried data
void download_card_image(const std::string &fileEnpoint, const std::string &fileName, const cpr::Header &headers);
void replace_char(std::string &cardName, const char checkChar, const char replaceChar);

class ApiClient {
private:
    std::mutex apiMutex;
    TimePoint lastCall = chrono::steady_clock::now() - chrono::seconds(1);
    const chrono::milliseconds interval{500};
public:
    void apiWait(std::function<void()> run_query);
};

class DatabaseClient {
private:
    std::mutex writeMutex;
    std::mutex readMutex;
public:
    std::unique_ptr<sql::ResultSet> db_read();
    void db_write(const std::string &dbData); //This will be data from read, change from string
    std::unique_ptr<sql::Connection> get_conn(const std::string &envfile);
};

struct DatabaseConfig {
    sql::URL url;
    sql::Properties properties;
};

struct AppConfig {
    std::string redisUri;
    std::string envFilePath;
    DatabaseConfig dbCfg;
};

struct AppContext {
    sw::redis::Redis redis;
    std::unique_ptr<sql::Connection> conn;
    cpr::Header headers;
    ApiClient globalClient;
    DatabaseClient globalDBClient;
    std::string envFilePath;

    AppContext (AppConfig config) 
    try : redis(config.redisUri),
          conn(sql::mariadb::get_driver_instance()->connect(config.dbCfg.url, config.dbCfg.properties))
    {
        envFilePath = config.envFilePath;
    }
    catch (const sw::redis::Error &e) {
        std::cerr << "Redis Error: " << e.what() << std::endl;
        throw;
    }
    catch (const sql::SQLException &e) {
        std::cerr << "Mariadb Error: " << e.what() << std::endl;
        throw;
    }
};

DatabaseConfig config_db_conn(const std::string &envFilePath);
void app_loop(AppContext &app); // Main program loop, keeps redis db in context for entire program
void worker_thread(AppContext &app, std::string query); // thread logic

int main() {
    AppConfig appCfg;
    appCfg.redisUri = "tcp://127.0.0.1:6379";
    appCfg.envFilePath = "/var/www/mtgwebapp/.env";
    appCfg.dbCfg = config_db_conn(appCfg.envFilePath);

    try {
        AppContext app(appCfg);
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

std::unique_ptr<sql::ResultSet> DatabaseClient::db_read() {
    std::lock_guard<std::mutex> lock(readMutex);

    std::unique_ptr<sql::ResultSet> tmp; //Placeholder to prevent errors

    return tmp;
}

void DatabaseClient::db_write(const std::string &dbData) {
    // lock thread
    std::lock_guard<std::mutex> lock(writeMutex);

    // write to DB
}

DatabaseConfig config_db_conn(const std::string &envFilePath) {
    std::string db_host = get_env_var(envFilePath, "DB_HOST");
    std::string db_name = get_env_var(envFilePath, "DB_NAME");
    std::string db_user = get_env_var(envFilePath, "DB_USER");
    std::string db_pass = get_env_var(envFilePath, "DB_PASS");

    sql::SQLString url("jdbc:mariadb://" + db_host + "/" + db_name + "?sslMode=disable");
    sql::Properties properties{{"user", db_user}, {"password", db_pass}};

    return DatabaseConfig{url, properties};
}

void app_loop(AppContext &app) {
    app.headers = format_header(get_env_var(app.envFilePath, "EMAIL"));

    // Run test
    std::unique_ptr<sql::Statement> stmt(app.conn->createStatement());
    std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT * FROM CardAttributes LIMIT 10"));

    while (res->next()) {
        std::cout << "cardID: " << res->getInt("cardID") 
                  << ", Name: " << res->getString("cardName")
                  << ", CleanName: " << res->getString("cleanCardName") << std::endl;
    }

    // Close connection
    app.conn->close();

    while (true) {

        std::vector<json> jsonList;
        batch_tasks(jsonList, app.redis);

        if (jsonList.empty()) {
            log_error("Redis failure");
            return;
        }

        std::vector<std::thread> threads;
        for (size_t i=0; i < jsonList.size(); ++i) {
            threads.emplace_back(worker_thread, std::ref(app), jsonList[i]["url"]);
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

void worker_thread(AppContext &app, std::string query) {
    // Eventually need to add connection pool for mariaDB, but for testing we'll use mutex

    // Check DB for card

    // If query comes back empty, query scryfall for card data
    std::string result;
    app.globalClient.apiWait([&]() {
        result = query_scryfall(query, app.headers);
    });

    // Insert scryfall results or update existing fields to DB
    {
        // call function and pass &json?
    }

    // If initial DB query came back empty, query again to get card ID for file write
    {

    }

    // Parse query result into json
    json parsedResult = json::parse(result);
    std::string cardName = parsedResult["name"];
    replace_char(cardName, ' ', '-');
    log_info(cardName);

    // extract card image file endpoint
    std::string fileEndpoint = parsedResult["image_uris"]["normal"]; // Might not be normal, double check python
    log_info(fileEndpoint);

    // construct file path for donwload
    // this will be changed to "[cardID]-[cardSet].jpg" after db conn is implemented
    std::string testDir = "/var/www/mtgwebapp/downloadTest/";
    std::string testFileName = testDir + cardName + ".jpg";
    
    // Download card image
    download_card_image(fileEndpoint, testFileName, app.headers);
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

std::string get_env_var(std::string path, std::string varName) {
    std::ifstream envFile;
    envFile.open(path);
    if (!envFile.is_open()) {
        log_error("Failed to open env file");
        return "";
    }

    std::string buf, checkVar;
    int position = 0;
    int nextPos = 0;
    while (std::getline(envFile, buf)) {
        checkVar = "";
        while (buf[position] != '=') {
            checkVar += buf[position];
            position++;
        }
        nextPos = position;
        position = 0;

        if (checkVar == varName) break;
    }
    envFile.close();
    nextPos++;

    std::string varValue;
    while (buf[nextPos] != '\0') {
        if (buf[nextPos] == '\"' || buf[nextPos] == '\'') nextPos++;
        if (buf[nextPos] == '\0') break;
        varValue += buf[nextPos];
        nextPos++;
    }

    return varValue;
}

cpr::Header format_header(std::string email) {
    cpr::Header headers;

    std::string userAgent = fmt::format("mtgDBManagerScript/{} ({})", email, VERSION);
    headers["User-Agent"] = userAgent;
    std::string accpt = "application/json";
    headers["Accept"] = accpt;

    return headers;
}

void process_result(const std::string &result) {
    json parsedResult = json::parse(result);
    log_info(parsedResult["name"]);
}

void download_card_image(const std::string &fileEndpoint, const std::string &fileName, const cpr::Header &headers) {
    std::ofstream outFile(fileName, std::ios::binary);

    cpr::Response response = cpr::Download(outFile, cpr::Url{fileEndpoint}, headers);

    std::string reponseStatus = "Download http status code: " + response.status_code;
    log_info(reponseStatus);
}

void replace_char(std::string &cardName, const char checkChar, const char replaceChar) {
    int i = 0;
    while (cardName[i] != '\0') {
        if (cardName[i] == checkChar) {
            cardName[i] = replaceChar;
        }
        ++i;
    }
}