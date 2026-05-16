#include "../include/app.hpp"
#include "../include/utils.hpp"
#include "../include/requests.hpp"

namespace app {

    int DatabaseClient::get_cardID(std::unique_ptr<sql::Connection> &conn, sql::SQLString cardName) {
        // Check card name against card attribute table to get cardID

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "SELECT cardID FROM CardAttributes WHERE cleanCardName LIKE ?"
        ));

        stmnt->setString(1, cardName);

        std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());

        // if !cardID ret -1
        int cardID = 0;
        if (res->next()) {
            cardID = res->getInt("cardID");
        }
        
        return cardID;
    }

    int DatabaseClient::get_setID(std::unique_ptr<sql::Connection> &conn, sql::SQLString setCode) {
        // use setCode to get setID from set table
        // Should handle error of setCode not being in table, but db should contain all sets

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "SELECT setID FROM SetLookup WHERE setCode LIKE ?"
        ));

        stmnt->setString(1, setCode);
        std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());

        int setID = 0;
        if (res->next()) {
            setID = res->getInt("setID");
        }

        return setID;
    }

    int DatabaseClient::get_collectionID(std::unique_ptr<sql::Connection> &conn, int setID, int cardID) {
        // Uses setID and cardID to check Collection table for entry

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "SELECT collectionID FROM Collection WHERE cardID = ? AND setID = ?"
        ));

        stmnt->setInt(1, cardID);
        stmnt->setInt(2, setID);
        std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());

        int collectionID = 0;
        if (res->next()) {
            collectionID = res->getInt("collectionID");
        }

       return collectionID;
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
        // Future additions may require this to become non-blocking
        // This will require rewriting the main loop checks for empty vector
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

        while (true) {
            std::vector<json> jsonList;
            batch_tasks(jsonList, app.redis, app.batchSize);

            if (jsonList.empty()) {
                utils::log_error("Redis failure");
                return;
            }

            std::vector<std::thread> threads;
            for (size_t i=0; i < jsonList.size(); ++i) {
                // using std::ref() instead of std::move() with json due to json object outliving threads
                threads.emplace_back(worker_thread, std::ref(app), std::ref(jsonList[i]));
            }

            for(auto& t : threads) t.join();
            utils::log_info("Batch Completed");
        }
    }

    void worker_thread(AppContext &app, const json &taskJson) {
        // Check DB for card
        std::unique_ptr<sql::Connection> conn(app.get_connection());

        std::string cardName = taskJson["name"];
        int cardID = app.globalDBClient.get_cardID(conn, cardName);

        int collectionID = 0;
        if (cardID) {
            std::string setCode = taskJson["setCode"];
            int setID = app.globalDBClient.get_setID(conn, setCode);
            if (setID) {
                collectionID = app.globalDBClient.get_collectionID(conn, setID, cardID);
            } else {
                // At some point this should trigger a query/scrape to add new set(s)
                // Then after new set(s) added, try again

                /*
                Will need new mutex to lock this down 
                and ensure that multiple threads do no attempt the same query
                */
                std::string error = "set " + setCode + " missing from DB";
                utils::log_error(error);
            }
        }

        // If query comes back empty, i.e. collectionID == 0, query scryfall for card data
        std::string result;
        if (collectionID) {
            std::string query = taskJson["url"];
            app.globalClient.apiWait([&]() {
                result = requests::query_scryfall(query, app.headers);
            });

            // Add scryfall results to DB
        } 

        // Update existing fields
        {
            // call function and pass &json?
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