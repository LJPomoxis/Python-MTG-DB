#include "../include/app.hpp"
#include "../include/utils.hpp"
#include "../include/requests.hpp"

namespace app {

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

    std::unique_ptr<sql::Connection> AppContext::get_connection() {
        return std::unique_ptr<sql::Connection>(dataSource->getConnection());
    }

    DatabaseConfig config_db_conn(const std::string &envFilePath, int iMaxSize) {
        std::string db_host = utils::get_env_var(envFilePath, "DB_HOST");
        std::string db_name = utils::get_env_var(envFilePath, "DB_NAME");
        std::string db_user = utils::get_env_var(envFilePath, "DB_USER");
        std::string db_pass = utils::get_env_var(envFilePath, "DB_PASS");

        std::string maxSize = utils::i_to_str(iMaxSize);

        sql::SQLString url( "jdbc:mariadb://" + 
                            db_host + 
                            "/" + 
                            db_name + 
                            "?sslMode=disable&" +
                            "tcpKeepAlive=true&" +
                            "minPoolSize=2&" +
                            "maxPoolSize=" + maxSize +"&" +
                            "maxIdleTime=900&" +
                            "poolValidMinDelay=2000");

        return DatabaseConfig{url, db_user, db_pass};
    }

    void batch_tasks(std::vector<json> &jsonList, sw::redis::Redis &redis, int batchSize) {
        auto task = redis.brpop("mtgdb_queue", 0);
        json data = json::parse(task->second);
        jsonList.push_back(data);
        utils::log_info("Got initial redis task");

        const chrono::seconds timeOut{8}; // arbitrary value, tweak as needed
        auto start = chrono::steady_clock::now();

        size_t targetSize = batchSize;
        while (chrono::steady_clock::now() - start < timeOut && jsonList.size() < targetSize) {
            auto next_task = redis.rpop("mtgdb_queue");
            if (next_task) {
                json data = json::parse(*next_task);
                jsonList.push_back(data);
            }
        }
    }

    void download_card_image(const std::string &fileEndpoint, const std::string &fileName, const cpr::Header &headers) {
        std::ofstream outFile(fileName, std::ios::binary);

        cpr::Response response = cpr::Download(outFile, cpr::Url{fileEndpoint}, headers);

        std::string reponseStatus = "Download http status code: " + response.status_code;
        utils::log_info(reponseStatus);
    }

    void app_loop(AppContext &app) {
        app.headers = requests::format_header(utils::get_env_var(app.envFilePath, "EMAIL"));

        std::unique_ptr<sql::Connection> conn(app.get_connection());

        // Run test
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT * FROM CardAttributes LIMIT 10"));

        while (res->next()) {
            std::cout << "cardID: " << res->getInt("cardID") 
                    << ", Name: " << res->getString("cardName")
                    << ", CleanName: " << res->getString("cleanCardName") << std::endl;
        }

        while (true) {
            std::vector<json> jsonList;
            batch_tasks(jsonList, app.redis, app.batchSize);

            if (jsonList.empty()) {
                utils::log_error("Redis failure");
                return;
            }

            std::vector<std::thread> threads;
            for (size_t i=0; i < jsonList.size(); ++i) {
                threads.emplace_back(worker_thread, std::ref(app), jsonList[i]["url"]);
            }

            for(auto& t : threads) t.join();
            utils::log_info("Batch Completed");
        }
    }

    void worker_thread(AppContext &app, std::string query) {
        // Eventually need to add connection pool for mariaDB, but for testing we'll use mutex

        // Check DB for card

        // If query comes back empty, query scryfall for card data
        std::string result;
        app.globalClient.apiWait([&]() {
            result = requests::query_scryfall(query, app.headers);
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
        utils::replace_char(cardName, ' ', '-');
        utils::log_info(cardName);

        // extract card image file endpoint
        std::string fileEndpoint = parsedResult["image_uris"]["normal"]; // Might not be normal, double check python
        utils::log_info(fileEndpoint);

        // construct file path for donwload
        // this will be changed to "[cardID]-[cardSet].jpg" after db conn is implemented
        std::string testDir = "/var/www/mtgwebapp/downloadTest/";
        std::string testFileName = testDir + cardName + ".jpg";
        
        // Download card image
        download_card_image(fileEndpoint, testFileName, app.headers);
    }

}