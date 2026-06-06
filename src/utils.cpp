#include "../include/utils.hpp"
#include <iostream>
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <fstream>
#include <unordered_set>
#include <string>

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

    void replace_char(std::string &inputStr, const char checkChar, const char replaceChar) {
        int i = 0;
        while (inputStr[i] != '\0') {
            if (inputStr[i] == checkChar) {
                inputStr[i] = replaceChar;
            }
            ++i;
        }
    }

    void extract_types(const std::string &inputString, std::unordered_set<std::string> &inputSet) {
        std::istringstream input(inputString);
        std::string keyword;

        while(input >> keyword) {
            inputSet.insert(keyword);
        }
    }

    std::string clean_name(const std::string &cardName) {
        std::string cleanName;

        int i = 0;
        while (cardName[i] != '\0') {
            char curr = cardName[i];
            if (curr == ' ' || (curr >= 'a' && curr <= 'z')) {
                cleanName.push_back(curr);
            } else if (curr >= 'A' && curr <= 'Z') {
                char temp = curr - 'A' + 'a';
                cleanName.push_back(temp);
            }

            i++;
        }

        return cleanName;
    }

    std::string_view get_color_name(const std::vector<std::string> &colors) {
        if (colors.empty()) return ColorNames[0];

        std::string color;
        int colorKey = 0;

        for (const auto& c : colors) {
            color += c;
        }

        size_t pos = color.find("W");
        if (pos != std::string::npos) colorKey |= 0b10000;
        pos = color.find("U");
        if (pos != std::string::npos) colorKey |= 0b1000;
        pos = color.find("B");
        if (pos != std::string::npos) colorKey |= 0b100;
        pos = color.find("R");
        if (pos != std::string::npos) colorKey |= 0b10;
        pos = color.find("G");
        if (pos != std::string::npos) colorKey |= 0b1;

        return ColorNames[colorKey];
    }
}