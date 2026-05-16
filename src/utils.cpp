#include "../include/utils.hpp"
#include <iostream>
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <fstream>

namespace utils {

    std::string i_to_str(int num) {
        std::string strNum;
        strNum.reserve(11); // reserving 10 digits worth of space for int value
        char charNum = '0';

        int sizeCheck = num;
        int numSize = 1; // number of digits in input decimal number
        while (sizeCheck >= 10) {
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
        //log_info(strNum);
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

    void process_result(const std::string &result) {
        json parsedResult = json::parse(result);
        log_info(parsedResult["name"]);
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

}