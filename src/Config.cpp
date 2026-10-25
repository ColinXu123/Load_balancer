/**
 * @file Config.cpp
 * @brief Implementation of the Config class.
 */
#include "Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace {

/// Removes leading and trailing whitespace.
std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

/// Returns a lower-case copy of a string.
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

} // namespace

bool Config::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    sourcePath_ = path;

    std::string line;
    while (std::getline(in, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = trim(line);
        if (line.empty()) continue;

        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key   = lower(trim(line.substr(0, eq)));
        const std::string value = trim(line.substr(eq + 1));
        if (key.empty()) continue;

        if (key == "blocked_range") {
            if (!value.empty()) blockedRanges_.push_back(value);
        } else {
            values_[key] = value;
        }
    }
    return true;
}

void Config::set(const std::string& key, const std::string& value) {
    values_[lower(key)] = value;
}

long Config::getInt(const std::string& key, long fallback) const {
    const auto it = values_.find(key);
    if (it == values_.end()) return fallback;
    try {
        std::size_t used = 0;
        const long v = std::stol(it->second, &used);
        return used == it->second.size() ? v : fallback;
    } catch (...) {
        return fallback;
    }
}

double Config::getDouble(const std::string& key, double fallback) const {
    const auto it = values_.find(key);
    if (it == values_.end()) return fallback;
    try {
        std::size_t used = 0;
        const double v = std::stod(it->second, &used);
        return used == it->second.size() ? v : fallback;
    } catch (...) {
        return fallback;
    }
}

bool Config::getBool(const std::string& key, bool fallback) const {
    const auto it = values_.find(key);
    if (it == values_.end()) return fallback;
    const std::string v = lower(it->second);
    if (v == "1" || v == "true" || v == "yes" || v == "on")  return true;
    if (v == "0" || v == "false" || v == "no" || v == "off") return false;
    return fallback;
}

std::string Config::getString(const std::string& key, const std::string& fallback) const {
    const auto it = values_.find(key);
    return it == values_.end() ? fallback : it->second;
}

const std::vector<std::string>& Config::blockedRanges() const { return blockedRanges_; }

const std::string& Config::sourcePath() const { return sourcePath_; }
