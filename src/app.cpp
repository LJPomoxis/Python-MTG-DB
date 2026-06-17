#include "../include/app.hpp"
#include "../include/utils.hpp"
#include "../include/requests.hpp"
#include <optional>

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

    int DatabaseClient::get_dfcID(std::unique_ptr<sql::Connection> &conn) {
        int dfcID = 0;

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "SELECT dfcID FROM MultiFaceCards ORDER BY dfcID desc LIMIT 2"
        ));

        std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());

        int dfcLower = 0;
        int dfcUpper = 0;
        if (res->next()) {
            dfcLower = res->getInt(1);
        }

        if (res->next()) {
            dfcUpper = res->getInt(1);
        }

        if (dfcLower == dfcUpper) {
            dfcID = dfcUpper + 1;
        } else {
            dfcID = dfcUpper;
        }

        return dfcID;
    }

    int DatabaseClient::get_colorID(std::unique_ptr<sql::Connection> &conn, sql::SQLString colorName) {
        int colorID = 0;

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "SELECT colorID FROM ColorLookup WHERE colorName LIKE ?"
        ));

        stmnt->setString(1, colorName);
        std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());

        if (res->next()) {
            colorID = res->getInt(1);
        }

        return colorID;
    }
    
    int DatabaseClient::get_typeID(std::unique_ptr<sql::Connection> &conn, sql::SQLString type) {
        int typeID = 0;
        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO TypeLookup (type) VALUES (?)"
            "ON DUPLICATE KEY UPDATE type = VALUE(type)"
        ));

        try {
            stmnt->setString(1, type);
            stmnt->executeUpdate();
            conn->commit();

            std::unique_ptr<sql::Statement> idstmnt(conn->createStatement());
            std::unique_ptr<sql::ResultSet> res(idstmnt->executeQuery("SELECT LAST_INSERT_ID()"));

            if (res->next()) {
                typeID = res->getInt(1);
            } else {
                //throw error?
            }
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error getting typeID: " << e.what();
            utils::log_error(err.str());
        }

        return typeID;
    }

    int DatabaseClient::get_keywordID(std::unique_ptr<sql::Connection> &conn, sql::SQLString keyword) {
        int keywordID = 0;
        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO KeywordLookup (keyword) VALUES (?)"
            "ON DUPLICATE KEY UPDATE keyword = VALUE(keyword)"
        ));

        try {
            stmnt->setString(1, keyword);
            stmnt->executeUpdate();
            conn->commit();

            std::unique_ptr<sql::Statement> idStmnt(conn->createStatement());
            std::unique_ptr<sql::ResultSet> res(idStmnt->executeQuery("SELECT LAST_INSERT_ID()"));

            if (res->next()) {
                // maybe should be getInt64?
                keywordID = res->getInt(1);
            } else {
                // throw error?
            }
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error getting keywordID: " << e.what();
            utils::log_error(err.str());
        }

        return keywordID;
    }

    int DatabaseClient::get_oracleID(std::unique_ptr<sql::Connection> &conn, sql::SQLString oracle) {
        int oracleID = 0;

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO OracleLookup (oracle) VALUES (?)"
            "ON DUPLICATE KEY UPDATE oracle = VALUE(oracle)"
        ));

        try {
            stmnt->setString(1, oracle);
            stmnt->executeUpdate();
            conn->commit();

            std::unique_ptr<sql::Statement> idstmnt(conn->createStatement());
            std::unique_ptr<sql::ResultSet> res(idstmnt->executeQuery("SELECT LAST_INSERT_ID()"));

            if (res->next()) {
                oracleID = res->getInt(1);
            } else {
                //throw error?
            }
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error getting oracleID: " << e.what();
            utils::log_error(err.str());
        }

        return oracleID;
    }

    int DatabaseClient::get_flavorID(std::unique_ptr<sql::Connection> &conn, sql::SQLString flavor) {
        int flavorID = 0;

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO FlavorLookup (flavor) VALUES (?)"
            "ON DUPLICATE KEY UPDATE flavor = VALUE(flavor)"
        ));

        try {
            stmnt->setString(1, flavor);
            stmnt->executeUpdate();
            conn->commit();

            std::unique_ptr<sql::Statement> idstmnt(conn->createStatement());
            std::unique_ptr<sql::ResultSet> res(idstmnt->executeQuery("SELECT LAST_INSERT_ID()"));

            if (res->next()) {
                flavorID = res->getInt(1);
            } else {
                //throw error?
            }
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error getting flavorID: " << e.what();
            utils::log_error(err.str());
        }

        return flavorID;
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

    int DatabaseClient::create_cardID(std::unique_ptr<sql::Connection> &conn, sql::SQLString cardName, sql::SQLString cleanCardName) {
        // default value, if returned as 0 error has occured/DNE
        int generatedCardID = 0;
        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
           "INSERT INTO CardAttributes (cardName, cleanCardName) VALUES (?, ?)"
           "ON DUPLICATE KEY UPDATE"
           "cardName = VALUE(cardName)"
           "cleanCardName = VALUE(cleanCardName)"
        ));

        try {
            stmnt->setString(1, cardName);
            stmnt->setString(2, cleanCardName);

            stmnt->executeUpdate();
            
            std::unique_ptr<sql::Statement> idstmnt(conn->createStatement());
            std::unique_ptr<sql::ResultSet> res(idstmnt->executeQuery("SELECT LAST_INSERT_ID()"));

            if (res->next()) {
                generatedCardID = res->getInt(1);
            } else {
                std::string error = "Error creating: " + std::string(cardName);
                utils::log_error(error);
            }

            conn->commit();
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error adding card: " << e.what();
            utils::log_error(err.str());
        }

        return generatedCardID;
    }

    int DatabaseClient::create_collectionID(std::unique_ptr<sql::Connection> &conn, const cardDetails &card) {
        int collectionID = 0;

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO Collection (cardID, setID, quantity) VALUES (?, ?, ?)"
        ));

        try {
            stmnt->setInt(1, card.ID);
            stmnt->setInt(2, card.setID);
            stmnt->setInt(3, card.quantity);

            stmnt->executeUpdate();
            conn->commit();

            std::unique_ptr<sql::Statement> idstmnt(conn->createStatement());
            std::unique_ptr<sql::ResultSet> res(idstmnt->executeQuery("SELECT LAST_INSERT_ID()"));

            if (res->next()) {
                collectionID = res->getInt(1);
            } else {
                //throw error?
            }
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error creating collection entry: " << e.what();
            utils::log_error(err.str());
        }

        return collectionID;
    }

    void DatabaseClient::add_keywords(std::unique_ptr<sql::Connection> &conn, const cardDetails &card) {
        
        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO CardKeywords (cardID, keywordID) VALUES (?, ?)"
        ));

        try {

            int keywordID = 0;
            for (const auto& keyword : card.keywords) {
                keywordID = get_keywordID(conn, keyword);
                stmnt->setInt(1, card.ID);
                stmnt->setInt(2, keywordID);
                stmnt->addBatch();
            }

            stmnt->executeBatch();
            conn->commit();
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error adding keywords: " << e.what();
            utils::log_error(err.str());
        }
    }

    void DatabaseClient::add_types(std::unique_ptr<sql::Connection> &conn, const cardDetails &card) {

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO CardType (cardID, cardTypeID) VALUES (?)"
        ));

        try {

            int typeID = 0;
            for (const auto& type : card.types) {
                typeID = get_typeID(conn, type);
                stmnt->setInt(1, card.ID);
                stmnt->setInt(1, typeID);
                stmnt->addBatch();
            }

            stmnt->executeBatch();
            conn->commit();
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error adding types: " << e.what();
            utils::log_error(err.str()); 
        }

    }

    void DatabaseClient::add_dfcID(std::unique_ptr<sql::Connection> &conn, const cardDetails &card) {

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO MultiFaceCards (cardID, dfcID) VALUES (?, ?)"
        ));

        try {
            int dfcID = get_dfcID(conn);
            stmnt->setInt(1, card.ID);
            stmnt->setInt(2, dfcID);

            stmnt->executeUpdate();
            conn->commit();

        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error adding dfcID: " << e.what();
            utils::log_error(err.str()); 
        }

    }

    void DatabaseClient::add_colors(std::unique_ptr<sql::Connection> &conn, const cardDetails &card) {

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO CardColors (cardID, colorID, colorIdentityID) VALUE (?, ?)"
        ));

        try {
            int colorID = get_colorID(conn, card.colors);
            int colorIdentityID = get_colorID(conn, card.colorIdentity);

            stmnt->setInt(1, card.ID);
            stmnt->setInt(2, colorID);
            stmnt->setInt(3, colorIdentityID);
            stmnt->executeUpdate();
            conn->commit();
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error adding card colors: " << e.what();
            utils::log_error(err.str());
        }

    }

    void DatabaseClient::add_oracle(std::unique_ptr<sql::Connection> &conn, const cardDetails &card) {

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO CardOracle (cardID, oracleID) VALUES (?, ?)"
        ));

        try {
            int oracleID = get_oracleID(conn, card.oracle);
            stmnt->setInt(1, card.ID);
            stmnt->setInt(2, oracleID);
            
            stmnt->executeUpdate();
            conn->commit();
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error adding card oracle: " << e.what();
            utils::log_error(err.str());
        }

    }

    void DatabaseClient::add_flavor(std::unique_ptr<sql::Connection> &conn, const cardDetails &card) {

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO CardFlavor (cardID, setID, flavorID) VALUES (?, ?, ?)"
        ));

        try {
            int flavorID = get_flavorID(conn, card.flavor);
            stmnt->setInt(1, card.ID);
            stmnt->setInt(2, card.setID);
            stmnt->setInt(3, flavorID);

            stmnt->executeUpdate();
            conn->commit();
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error adding card flavor: " << e.what();
            utils::log_error(err.str());
        }

    }

    void DatabaseClient::add_manaVal(std::unique_ptr<sql::Connection> &conn, const cardDetails &card) {

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO CardManaValue (cardID, manaValue, hasXinCost, stringManaValue) VALUES (?, ?, ?, ?)"
        ));

        try {
            stmnt->setInt(1, card.ID);
            stmnt->setInt(2, card.manaValue);
            stmnt->setBoolean(3, card.hasXinCost);
            stmnt->setString(4, card.displayManaValue);

            stmnt->executeUpdate();
            conn->commit();
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error adding card mana info: " << e.what();
            utils::log_error(err.str());
        }

    }

    void DatabaseClient::add_PT(std::unique_ptr<sql::Connection> &conn, const cardDetails &card) {

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO CardPT (cardID, power, toughness) VALUES (?, ?, ?)"
        ));

        try {
            stmnt->setInt(1, card.ID);
            stmnt->setInt(2, card.power);
            stmnt->setInt(3, card.toughness);

            stmnt->executeUpdate();
            conn->commit();
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error adding card P/T: " << e.what();
            utils::log_error(err.str());
        }
    }

    void DatabaseClient::add_image(std::unique_ptr<sql::Connection> &conn, const cardDetails &card) {

        std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
            "INSERT INTO CardImage (cardID, setID, imageUrl, bigImageUrl) VALUES (?, ?, ?, ?)"
        ));

        try {
            stmnt->setInt(1, card.ID);
            stmnt->setInt(2, card.setID);
            stmnt->setString(3, card.smallUri);
            stmnt->setString(4, card.normalUri);

            stmnt->executeUpdate();
            conn->commit();
        } catch (sql::SQLException &e) {
            conn->rollback();
            std::ostringstream err;
            err << "Error adding card images: " << e.what();
            utils::log_error(err.str());
        }
    }

    void DatabaseClient::add_new_card(std::unique_ptr<sql::Connection> &conn, cardDetails &card) {
        card.ID = create_cardID(conn, card.name, card.cleanName);
        card.setID = get_setID(conn, card.setCode);

        int collectionID = get_collectionID(conn, card.setID, card.ID);
        if (!collectionID) {
            collectionID = create_collectionID(conn, card);
        }
        
        add_keywords(conn, card);
        add_types(conn, card);

        if (card.isDFC) {
            add_dfcID(conn, card);
        }

        add_colors(conn, card);
        add_oracle(conn, card);
        add_flavor(conn, card);
        add_manaVal(conn, card);
        
        if (card.isCreature) {
            add_PT(conn, card);
        }
        
        add_image(conn, card);
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
            conn->rollback();
            std::ostringstream err;
            err << "Error naming deck: " << e.what();
            utils::log_error(err.str());
        }
    } 

    bool DatabaseClient::update_collection(std::unique_ptr<sql::Connection> &conn, int collectionID, int quantity) {
        int collectionQuantity;
        bool isProxy = false;
        std::shared_ptr<sql::PreparedStatement> query(conn->prepareStatement(
            "SELECT quantity FROM Collection WHERE collectionID = ?"
        ));

        query->setInt(1, collectionID);
        std::unique_ptr<sql::ResultSet> res(query->executeQuery());

        if (res->next()) {
            collectionQuantity = res->getInt("quantity");
        }

        quantity = quantity - collectionQuantity;
        if (quantity < 0) {
            std::ostringstream err;
            err << "Error: Insufficient quantity; [" << collectionID << "] marked as proxy";
            utils::log_error(err.str());
            quantity = 0;
            isProxy = true;
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

        return isProxy;
    }

    void DatabaseClient::update_decklist(std::unique_ptr<sql::Connection> &conn, DeckDetails dd) {
        try {
            std::shared_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
                "INSERT INTO Decks (deckID, collectionID, numberInDeck, isProxy) VALUES (?, ?, ?, ?)"
            ));

            stmnt->setInt(1, dd.deckID);
            stmnt->setInt(2, dd.collectionID);
            stmnt->setInt(3, dd.numberInDeck);
            stmnt->setBoolean(4, dd.isProxy);
            stmnt->setBoolean(5, dd.isCommander);

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

    std::optional<std::vector<json>> extract_faces(const json &scryfallData) {
        if (!scryfallData.contains("card_faces")) {
            return std::nullopt;
        }

        std::vector<json> faceData;
        for (const auto& face : scryfallData["card_faces"]) {
            faceData.emplace_back(face);
        }
        return faceData;
    }

    cardDetails process_faces(const cardDetails &inCard, const json &faceData) {
        cardDetails card = inCard;
        card.isDFC = true;

        card.name = faceData["name"].get<std::string>();
        card.cleanName = utils::clean_name(card.name);

        card.colors = utils::get_color_name(faceData["colors"].get<std::vector<std::string>>());
        card.displayManaValue = faceData["mana_cost"].get<std::string>();

        std::string types = faceData["type_line"].get<std::string>();
        card.types = utils::extract_types(types);

        card.oracle = faceData["oracle_text"].get<std::string>();
        if (faceData.contains("flavor_text")) {
            card.flavor = faceData["flavor_text"].get<std::string>();
        } else {
            card.flavor = "";
        }

        if (faceData.contains("power")) card.isCreature = true;
        if (card.isCreature) {
            if (faceData["power"].is_number_integer()) {
                card.power = faceData["power"].get<int>();
            }
            
            if (faceData["toughness"].is_number_integer()) {
                card.toughness = faceData["toughness"].get<int>();
            }
        }

        // search mana_cost for the char 'X'
        size_t pos = card.displayManaValue.find("X");
        if (pos != std::string::npos) {
            card.hasXinCost = true;
        }

        card.smallUri = faceData["image_uris"]["small"].get<std::string>();
        card.normalUri = faceData["image_uris"]["normal"].get<std::string>();

        return card;
    }

    std::vector<cardDetails> process_card_json(const std::string &scryfallResults) {
        json scryfallData = json::parse(scryfallResults);
        
        auto faces = extract_faces(scryfallData);

        std::vector<cardDetails> cards;
        cardDetails card;

        card.ID = card.setID = card.quantity = 0;

        card.setCode = scryfallData["set"].get<std::string>();
        card.colorIdentity = utils::get_color_name(scryfallData["color_identity"].get<std::vector<std::string>>());
        card.manaValue = scryfallData["cmc"].get<int>();
        card.keywords = scryfallData["keywords"].get<std::unordered_set<std::string>>();

        if (faces.has_value()) {
            for (const auto& face : faces.value()) {
                cards.emplace_back(process_faces(card, face));
            }
            return cards;
        }

        cards.emplace_back(card);

        cards[0].name = scryfallData["name"].get<std::string>();
        cards[0].cleanName = utils::clean_name(cards[0].name);
        
        cards[0].colors = utils::get_color_name(scryfallData["colors"].get<std::vector<std::string>>());
        cards[0].displayManaValue = scryfallData["mana_cost"].get<std::string>();

        std::string types = scryfallData["type_line"].get<std::string>();
        cards[0].types = utils::extract_types(types);

        cards[0].oracle = scryfallData["oracle_text"].get<std::string>();
        if (scryfallData.contains("flavor_text")) {
            cards[0].flavor = scryfallData["flavor_text"].get<std::string>();
        } else {
            cards[0].flavor = "";
        }

        if (scryfallData.contains("power")) cards[0].isCreature = true;

        if (cards[0].isCreature) {
            if (scryfallData["power"].is_number_integer()) {
                cards[0].power = scryfallData["power"].get<int>();
            }
            
            if (scryfallData["toughness"].is_number_integer()) {
                cards[0].toughness = scryfallData["toughness"].get<int>();
            }
        }

        // search mana_cost for the char 'X'
        size_t pos = cards[0].displayManaValue.find("X");
        if (pos != std::string::npos) {
            cards[0].hasXinCost = true;
        }

        cards[0].smallUri = scryfallData["image_uris"]["small"].get<std::string>();
        cards[0].normalUri = scryfallData["image_uris"]["normal"].get<std::string>();

        return cards;
    }

    // This function works properly under the assumption both faces of any give DFC are passed
    // as separate cards which is correct following the design of the database
    void download_card_image(const cardDetails &card, const std::string &scryfallResults, const cpr::Header &headers) {
        // Parse query result into json
        json scryfallData = json::parse(scryfallResults);

        // extract card image file endpoint
        std::string fileEndpoint;
        if (scryfallData.contains("card_faces")) {
            bool loop = true;
            // Structured to maybe handle DFC eventually?
            for (const auto& face : scryfallData["card_faces"]) {
                if (loop) {
                    loop = false;
                    fileEndpoint = face["image_uris"]["normal"].get<std::string>();
                }
            }
        } else {
            fileEndpoint = scryfallData["image_uris"]["normal"].get<std::string>();
        }

        // Construct download file path
        std::string imageDir = "/var/www/mtgwebapp/static/images/cards";
        std::string ID = utils::i_to_str(card.ID);
        std::string setID = utils::i_to_str(card.setID);
        std::string fileName = imageDir + ID + "-" + setID + ".jpg";
        
        // Download card image
        download_file(fileEndpoint, fileName, headers);
    }

    void download_file(const std::string &fileEndpoint, const std::string &fileName, const cpr::Header &headers) {
        std::ofstream outFile(fileName, std::ios::binary);

        cpr::Response response = cpr::Download(outFile, cpr::Url{fileEndpoint}, headers);

        std::string reponseStatus = "Download http status code: " + response.status_code;
        utils::log_info(reponseStatus);
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

    void deck_card_add(AppContext &app, const json &cardJson, std::unique_ptr<sql::Connection> &conn) {
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

        if (deckID == 0) {
            utils::log_error("Deck does not exist");
            // This should eventually be handled more cleanly than returning
            return;
        }

        // Check DB for card
        cardDetails card;
        card.name = cardJson["name"];
        card.ID = app.globalDBClient.get_cardID(conn, card.name);

        card.collectionID = 0;
        if (card.ID) {
            card.setCode = cardJson["set"];
            card.setID = app.globalDBClient.get_setID(conn, card.setCode);
            if (card.setID) {
                card.collectionID = app.globalDBClient.get_collectionID(conn, card.setID, card.ID);
            } else {
                // At some point this should trigger a query/scrape to add new set(s)
                /*
                    After new set(s) added, try queries again

                    Will need new mutex to lock this down 
                    and ensure that multiple threads do no attempt the same query
                */
                std::string error = "set " + card.setCode + " not present in DB";
                utils::log_error(error);
            }
        }

        // If DB query comes back empty, i.e. collectionID == 0, query scryfall for card data
        std::string result;
        std::vector<cardDetails> cards;
        if (!card.collectionID) {
            std::string query = cardJson["url"];
            app.globalClient.apiWait([&]() {
                result = requests::query_scryfall(query, app.headers);
            });

            // Process json results into cardDetails struct
            cards = process_card_json(result);

            // Add card(s) to the database
            for (const auto& c : cards) {
                cardDetails cardFace = c;
                app.globalDBClient.add_new_card(conn, cardFace);
            }
        } else {
            cards.emplace_back(card);
        }

        // Update existing fields
        // Using cards[0] because we only want to add front of DFC to decklist, not both sides
        cards[0].quantity = cardJson["quantity"].get<int>();
        bool isCommander = cardJson["isCommander"].get<bool>();
        bool isProxy = app.globalDBClient.update_collection(conn, cards[0].collectionID, cards[0].quantity);
        
        //checks if dfc, if true update the entry for the backside in addition to the front
        if (cards.size() > 1) {
            app.globalDBClient.update_collection(conn, cards[1].collectionID, cards[1].quantity);
        }
        DeckDetails dd = {deckID, cards[0].collectionID, cards[0].quantity, isProxy, isCommander};
        app.globalDBClient.update_decklist(conn, dd);
    }

    void worker_thread(AppContext &app, const json &cardJson) {
        // get connection
        std::unique_ptr<sql::Connection> conn(app.get_connection());
        conn->setAutoCommit(false);

        //std::cout << cardJson;

        if (!cardJson.contains("type")) {
            utils::log_error("Task missing type identifier");
            return;
        }

        if (cardJson["type"] == "deckCardAdd") {
            deck_card_add(app, cardJson, conn);
        } else if (cardJson["type"] == "CardImageDownload") {
            // need to do some work on this function based on new reformatting of worker thread function
            // download_card_image();
        } else {
            utils::log_error("Unkown task type");
        }
    }

}