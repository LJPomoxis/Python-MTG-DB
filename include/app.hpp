#pragma once

#include "requests.hpp"
#include <iostream>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <sw/redis++/redis++.h>
#include <mariadb/conncpp.hpp>
#include <unordered_set>
#include <optional>

using json = nlohmann::json;

namespace app {

    struct cardDetails {
        int ID;
        int setID;
        int collectionID;
        int quantity;
        std::string name;
        std::string setCode;
        std::string cleanName; // not strictly necessary, but simpler to generate during json parsing
        std::string colors;
        std::string colorIdentity;
        int manaValue;
        std::string displayManaValue;
        std::unordered_set<std::string> keywords;
        std::unordered_set<std::string> types;
        std::string oracle;
        std::string flavor;
        bool isCreature = false;
        int power = 0;
        int toughness = 0;
        bool hasXinCost = false;
        std::string smallUri;
        std::string normalUri;
        bool isDFC = false;
    };

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
        int get_deckID(std::unique_ptr<sql::Connection> &conn, sql::SQLString deckName);
        void add_new_card(std::unique_ptr<sql::Connection> &conn, const cardDetails &card);
        void name_deck(std::unique_ptr<sql::Connection> &conn, sql::SQLString deckName);
        bool update_collection(std::unique_ptr<sql::Connection> &conn, int collectionID, int quantity);
        void update_decklist(std::unique_ptr<sql::Connection> &conn, DeckDetails dd);
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

    std::optional<std::vector<json>> extract_faces(const json &scryfallData);
    cardDetails process_faces(const cardDetails &inCard, const json &faceData);
    std::vector<cardDetails> process_card_json(const std::string &scryfallResults);
    void download_card_image(const cardDetails &card, const std::string &scryfallResults, const cpr::Header &headers);
    void download_file(const std::string &fileEnpoint, const std::string &fileName, const cpr::Header &headers);
    DatabaseConfig config_db_conn(const std::string &envFilePath, int iMaxSize);
    void batch_tasks(std::vector<json> &jsonList, sw::redis::Redis &redis, int batchSize); // task manager for watching redis and batching tasks taken from redis queue
    void app_loop(AppContext &app); // Main program loop, keeps redis db in context for entire program
    void worker_thread(AppContext &app, const json &cardJson); // thread logic

}