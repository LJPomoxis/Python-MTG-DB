#pragma once

#include <nlohmann/json.hpp>
#include <sw/redis++/redis++.h>
#include <unordered_set>

using json = nlohmann::json;
using TimePoint = std::chrono::steady_clock::time_point;
namespace chrono = std::chrono;

namespace utils {

    std::string i_to_str(int num);
    void log_info(const std::string& message); // thread safe logging using stdout
    void log_error(const std::string& message); // thread safe logging using cerr
    std::string get_env_var(std::string path, std::string varName); // pulls email from env file
    void process_result(const std::string &result); // function for parsing and checking queried data
    void replace_char(std::string &inputStr, const char checkChar, const char replaceChar);
    void extract_types(const std::string &inputString, std::unordered_set<std::string> &inputSet);

}