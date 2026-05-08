#pragma once

#include "requests.hpp"
#include <iostream>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <sw/redis++/redis++.h>
#include <mariadb/conncpp.hpp>

using json = nlohmann::json;

namespace app {

    class DatabaseClient {
    private:
        std::mutex writeMutex;
        std::mutex readMutex;
    public:
        std::unique_ptr<sql::ResultSet> db_read();
        void db_write(const std::string &dbData); //This will be data from read, change from string
    };

    struct DatabaseConfig {
        sql::URL url;
        std::string user;
        std::string password;
    };

    struct AppConfig {
        std::string redisUri;
        std::string envFilePath;
        DatabaseConfig dbCfg;
    };

    struct AppContext {
        sw::redis::Redis redis;
        std::unique_ptr<sql::mariadb::MariaDbDataSource> dataSource;
        cpr::Header headers;
        requests::ApiClient globalClient;
        DatabaseClient globalDBClient;
        std::string envFilePath;

        AppContext (AppConfig config) 
        try : redis(config.redisUri)
        {
            envFilePath = config.envFilePath;
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

    DatabaseConfig config_db_conn(const std::string &envFilePath);
    void batch_tasks(std::vector<json> &jsonList, sw::redis::Redis &redis); // task manager for watching redis and batching tasks taken from redis queue
    void download_card_image(const std::string &fileEnpoint, const std::string &fileName, const cpr::Header &headers);
    void app_loop(AppContext &app); // Main program loop, keeps redis db in context for entire program
    void worker_thread(AppContext &app, std::string query); // thread logic

}