/**
 * @file Logger.cpp
 * @brief Implementation of the Logger class.
 */
#include "Logger.h"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

bool          Logger::verbose_ = false;
bool          Logger::color_   = true;
std::ofstream Logger::file_;

namespace {

/// Maps a LogColor to its ANSI escape sequence.
const char* ansi(LogColor color) {
    switch (color) {
        case LogColor::Blue:    return "\033[34m";
        case LogColor::Green:   return "\033[32m";
        case LogColor::Red:     return "\033[31;1m";
        case LogColor::Yellow:  return "\033[33;1m";
        case LogColor::Magenta: return "\033[35;1m";
        case LogColor::Cyan:    return "\033[36m";
        case LogColor::Bold:    return "\033[1m";
        case LogColor::Plain:   default: return "";
    }
}

const char* kReset = "\033[0m";

} // namespace

bool Logger::init(bool verbose, bool color, const std::string& logPath) {
    verbose_ = verbose;
    color_   = color;
    if (file_.is_open()) file_.close();
    if (logPath.empty()) return true;
    // Create any missing parent folders (e.g. "logs/") so --log just works.
    const std::filesystem::path parent = std::filesystem::path(logPath).parent_path();
    if (!parent.empty()) {
        std::error_code ignored;
        std::filesystem::create_directories(parent, ignored);
    }
    file_.open(logPath, std::ios::out | std::ios::trunc);
    return file_.is_open();
}

void Logger::event(long cycle, const std::string& source, LogColor color,
                   const std::string& tag, const std::string& text) {
    if (!verbose_) return;  // quiet mode only prints the start and end summaries
    std::ostringstream line;
    line << "[cycle " << std::setw(7) << cycle << "] "
         << std::left << std::setw(16) << ("[" + source + "]") << ' '
         << std::setw(8) << tag << std::right << ' ' << text;
    write(line.str(), color);
}

void Logger::always(const std::string& text, LogColor color) {
    write(text, color);
}

bool Logger::verbose() { return verbose_; }

void Logger::close() {
    std::cout.flush();
    if (file_.is_open()) file_.close();
}

void Logger::write(const std::string& text, LogColor color) {
    if (color_ && color != LogColor::Plain) {
        std::cout << ansi(color) << text << kReset << '\n';
    } else {
        std::cout << text << '\n';
    }
    if (file_.is_open()) file_ << text << '\n';
}