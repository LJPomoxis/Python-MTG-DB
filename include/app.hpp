#pragma once

#include "requests.hpp"
#include <iostream>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <sw/redis++/redis++.h>
#include <mariadb/conncpp.hpp>

using json = nlohmann::json;

namespace app {

    struct DeckDetails {
        int deckID;
        int collectionID;
        int numberInDeck;
        bool isProxy;
    };

    class DatabaseClient {
    private:
        // Not stricly necessary unless there is a race condition between threads
        std::mutex writeMutex;
    public:
        int get_cardID(std::unique_ptr<sql::Connection> &conn, sql::SQLString cardName);
        int get_setID(std::unique_ptr<sql::Connection> &conn, sql::SQLString setCode);
        int get_collectionID(std::unique_ptr<sql::Connection> &conn, int setID, int cardID);
        bool update_collection(std::unique_ptr<sql::Connection> &conn, int collectionID, int quantity);
        void create_decklist(std::unique_ptr<sql::Connection> &conn, DeckDetails dd);
    };

    struct DatabaseConfig {
        sql::URL url;
        std::string user;
        std::string password;
    };

    struct AppConfig {
        std::string redisUri;
        std::string envFilePath;
        int batchSize;
        DatabaseConfig dbCfg;
    };

    struct AppContext {
        sw::redis::Redis redis;
        std::unique_ptr<sql::mariadb::MariaDbDataSource> dataSource;
        cpr::Header headers;
        requests::ApiClient globalClient;
        DatabaseClient globalDBClient;
        std::string envFilePath;
        int batchSize;

        AppContext (AppConfig config) 
        try : redis(config.redisUri)
        {
            envFilePath = config.envFilePath;
            batchSize = config.batchSize;
            dataSource.reset(new sql::mariadb::MariaDbDataSource(config.dbCfg.url));
            dataSource->setUser(config.dbCfg.user);
            dataSource->setPassword(config.dbCfg.password);
        }
        catch (const sw::redis::Error &e) {
            std::cerr << "Redis Error: " << e.what() << std::endl;
            throw;
        }

        std::unique_ptr<sql::Connection> get_connection();
    };

    DatabaseConfig config_db_conn(const std::string &envFilePath, int iMaxSize);
    void batch_tasks(std::vector<json> &jsonList, sw::redis::Redis &redis, int batchSize); // task manager for watching redis and batching tasks taken from redis queue
    void download_card_image(const std::string &fileEnpoint, const std::string &fileName, const cpr::Header &headers);
    void app_loop(AppContext &app); // Main program loop, keeps redis db in context for entire program
    void worker_thread(AppContext &app, const json &taskJson); // thread logic

}