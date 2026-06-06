#pragma once

#include <nlohmann/json.hpp>
#include <sw/redis++/redis++.h>
#include <unordered_set>

using json = nlohmann::json;
using TimePoint = std::chrono::steady_clock::time_point;
namespace chrono = std::chrono;

namespace utils {

    constexpr std::array<std::string_view, 32> ColorNames = {
        "Colorless",
        "Green",
        "Red",
        "Gruul",
        "Black",
        "Golgari",
        "Rakdos",
        "Jund",
        "Blue",
        "Simic",
        "Izzet",
        "Temur",
        "Dimir",
        "Sultai",
        "Grixis",
        "Glint",
        "White",
        "Selesnya",
        "Boros",
        "Naya",
        "Orhzov",
        "Abzan",
        "Mardu",
        "Dune",
        "Azorius",
        "Bant",
        "Jeskai",
        "Ink",
        "Esper",
        "Witch",
        "Yore",
        "Rainbow"
    };

    std::string i_to_str(int num);
    void log_info(const std::string& message); // thread safe logging using stdout
    void log_error(const std::string& message); // thread safe logging using cerr
    std::string get_env_var(std::string path, std::string varName); // pulls email from env file
    void process_result(const std::string &result); // function for parsing and checking queried data
    void replace_char(std::string &inputStr, const char checkChar, const char replaceChar);
    void extract_types(const std::string &inputString, std::unordered_set<std::string> &inputSet);
    std::string clean_name(const std::string &cardName);
    std::string_view get_color_name(const std::vector<std::string> &colors);

}