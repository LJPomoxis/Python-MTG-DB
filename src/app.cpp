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

        // if !cardID ret 0, DB records start at 1 using auto increment
        int cardID = 0;
        if (res->next()) {
            cardID = res->getInt("cardID");
        }
        
        return cardID;
    }

    int DatabaseClient::get_setID(std::unique_ptr<sql::Connection> &conn, sql::SQLString setCode) {
        // use setCode to get setID from set table
        // Maybe in future if setCode doesn't return ID, function is triggered to update sets table
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

    int DatabaseClient::get_deckID(std::unique_ptr<sql::Connection> &conn, sql::SQLString deckName) {
        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "SELECT deckID FROM DeckNameLookup WHERE deckName LIKE ?"
        ));

        stmnt->setString(1, deckName);
        std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());

        int deckID = 1;
        if (res->next()) {
            deckID = res->getInt("deckID");
        }

        return deckID;
    }

    void DatabaseClient::name_deck(std::unique_ptr<sql::Connection> &conn, sql::SQLString deckName) {
        try {
            std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
                "INSERT INTO DeckNameLookup (deckName) VALUES (?)"
            ));

            stmnt->setString(1, deckName);
            stmnt->execute();
            conn->commit();
        } catch (sql::SQLException &e) {
            std::ostringstream err;
            err << "Error naming deck: " << e.what();
            utils::log_error(err.str());
        }
    } 

    bool DatabaseClient::update_collection(std::unique_ptr<sql::Connection> &conn, int collectionID, int quantity) {
        int collectionQuantity;
        bool proxy = false;
        std::shared_ptr<sql::PreparedStatement> query(conn->prepareStatement(
            "SELECT quantity FROM Collection WHERE collectionID = ?"
        ));

        query->setInt(1, collectionID);
        std::unique_ptr<sql::ResultSet> res(query->executeQuery());

        if (res->next()) {
            collectionQuantity = res->getInt("quantity");
        }

        quantity = quantity - collectionQuantity;

        /*
            For now we assume cards exist in DB Collection table even with a quantity of 0

            In the future if the card DNE then an empty entry will be added for it,
            this way we can handle proxies without needing to manually add cards to the DB

            This is what scryfall query is for, just needs to be fully implemented
        */

        if (quantity < 0) {
            std::ostringstream err;
            err << "Error: Insufficient quantity; [" << collectionID << "] marked as proxy";
            utils::log_error(err.str());
            quantity = 0;
            proxy = true;
        }

        try {
            std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
                "UPDATE Collection SET quantity = ? WHERE collectionID = ?"
            ));
            
            stmnt->setInt(1, quantity);
            stmnt->setInt(2, collectionID);

            stmnt->execute();
            conn->commit();
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error updating collection: " << e.what();
            utils::log_error(err.str());
        }

        return proxy;
    }

    void DatabaseClient::create_decklist(std::unique_ptr<sql::Connection> &conn, DeckDetails dd) {
        try {
            std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
                "INSERT INTO Decks (deckID, collectionID, numberInDeck, isProxy) VALUES (?, ?, ?, ?)"
            ));

            stmnt->setInt(1, dd.deckID);
            stmnt->setInt(2, dd.collectionID);
            stmnt->setInt(3, dd.numberInDeck);
            stmnt->setBoolean(4, dd.isProxy);

            stmnt->execute();
            conn->commit();
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error adding to deck: " << e.what();
            utils::log_error(err.str());
        }
    }

    std::unique_ptr<sql::Connection> AppContext::get_connection() {
        return std::unique_ptr<sql::Connection>(dataSource->getConnection());
    }

    // Doesn't handle DFC currently
    void process_card_json(cardDetails &card, const json &scryfallResults) {
        json scryfallData = json::parse(scryfallResults);
        
        card.name = scryfallData["name"].get<std::string>();
        card.setCode = scryfallData["set"].get<std::string>();
        // Not sure where this comes into play yet
        card.quantity = 0;
        card.cleanName = utils::clean_name(card.name);
        
        card.colors = utils::get_color_name(scryfallData["colors"].get<std::vector<std::string>>());
        card.colorIdentity = utils::get_color_name(scryfallData["color_identity"].get<std::vector<std::string>>());

        card.manaValue = scryfallData["cmc"].get<int>();
        std::string manaCost = scryfallData["mana_cost"].get<std::string>();
        card.displayManaValue = manaCost;

        card.keywords = scryfallData["keywords"].get<std::unordered_set<std::string>>();
        std::string types = scryfallData["type_line"].get<std::string>();
        utils::extract_types(types, card.types);

        card.oracle = scryfallData["oracle_text"].get<std::string>();
        card.flavor = scryfallData["flavor_text"].get<std::string>();

        for (const auto& type : card.types) {
            size_t pos = type.find("Creature");
            if (pos != std::string::npos) {
                card.isCreature = true;
            }
        }

        if (card.isCreature) {
            if (scryfallData["power"].is_string()) {
                card.power = 0;
            } else if (scryfallData["power"].is_number_integer()) {
                card.power = scryfallData["power"];
            } else {
                // default case
                card.power = 0;
            }

            if (scryfallData["toughness"].is_string()) {
                card.toughness = 0;
            } else if (scryfallData["toughness"]) {
                card.toughness = scryfallData["toughness"];
            } else {
                card.toughness = 0;
            }
        }

        // search mana_cost for the char 'X'
        card.hasXinCost = false;
        size_t pos = manaCost.find("X");
        if (pos != std::string::npos) {
            card.hasXinCost = true;
        }

        card.smallUri = scryfallData["image_uris"]["small"].get<std::string>();
        card.normalUri = scryfallData["image_uris"]["normal"].get<std::string>();

        // get cardID and setID are DB functions, not in function purview
        card.ID = 0;
        card.setID = 0;
    }

    void add_new_card(std::unique_ptr<sql::Connection> &conn, const cardDetails &card) {

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
                continue; // Not very graceful recovery, but this should never happen?
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

    void worker_thread(AppContext &app, const json &cardJson) {
        // get connection
        std::unique_ptr<sql::Connection> conn(app.get_connection());
        conn->setAutoCommit(false);

        // Set deck name
        int deckID;
        std::string deckName;
        if (cardJson.contains("deckName")) {
            deckName = cardJson["deckName"];
            app.globalDBClient.name_deck(conn, deckName);
        } else {
            deckName = cardJson["dName"];
            deckID = app.globalDBClient.get_deckID(conn, deckName);
        }

        // Check DB for card
        std::string cardName = cardJson["name"];
        int cardID = app.globalDBClient.get_cardID(conn, cardName);

        int collectionID = 0;
        if (cardID) {
            std::string setCode = cardJson["set"];
            int setID = app.globalDBClient.get_setID(conn, setCode);
            if (setID) {
                collectionID = app.globalDBClient.get_collectionID(conn, setID, cardID);
            } else {
                // At some point this should trigger a query/scrape to add new set(s)
                /*
                    After new set(s) added, try queries again

                    Will need new mutex to lock this down 
                    and ensure that multiple threads do no attempt the same query
                */
                std::string error = "set " + setCode + " not present in DB";
                utils::log_error(error);
            }
        }

        // If query comes back empty, i.e. collectionID == 0, query scryfall for card data
        std::string result;
        if (!collectionID) {
            std::string query = cardJson["url"];
            app.globalClient.apiWait([&]() {
                result = requests::query_scryfall(query, app.headers);
            });

            // Add scryfall results to DB here
        }

        // Update existing fields
        int quantity = cardJson["quantity"].get<int>();
        bool isProxy = app.globalDBClient.update_collection(conn, collectionID, quantity);
        DeckDetails dd = {deckID, collectionID, quantity, isProxy};
        app.globalDBClient.create_decklist(conn, dd);

        // temporarily removed this section for testing
        bool download = false;
        if (download) {
            // Parse query result into json
            json parsedResult = json::parse(result);
            std::string card = parsedResult["name"];
            utils::replace_char(card, ' ', '-');
            utils::log_info(card);

            // extract card image file endpoint
            std::string fileEndpoint = parsedResult["image_uris"]["normal"]; // Might not be normal, double check python
            utils::log_info(fileEndpoint);

            // construct file path for donwload
            // this will be changed to "[cardID]-[cardSet].jpg" after db conn is implemented
            std::string testDir = "/var/www/mtgwebapp/downloadTest/";
            std::string testFileName = testDir + card + ".jpg";
            
            // Download card image
            download_card_image(fileEndpoint, testFileName, app.headers);
        }
    }

}