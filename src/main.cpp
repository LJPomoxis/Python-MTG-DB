#include <iostream>
#include "../include/app.hpp"

int main() {
    app::AppConfig appCfg;
    appCfg.redisUri = "tcp://127.0.0.1:6379";
    appCfg.envFilePath = "/var/www/mtgwebapp/.env";
    appCfg.dbCfg = app::config_db_conn(appCfg.envFilePath);

    try {
        app::AppContext appCtx(appCfg);
        app::app_loop(appCtx);
    } catch (const std::exception &e) {
        // Connection error
        std::cerr << "Fatal connection error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}