/**
 * @file IPBlocker.cpp
 * @brief Implementation of the IPBlocker firewall.
 */
#include "IPBlocker.h"

#include <sstream>

namespace {

/// Removes leading and trailing whitespace.
std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

} // namespace

bool IPBlocker::parseIP(const std::string& ip, std::uint32_t& out) {
    std::istringstream in(trim(ip));
    std::uint32_t value = 0;
    for (int part = 0; part < 4; ++part) {
        int octet = -1;
        if (!(in >> octet) || octet < 0 || octet > 255) return false;
        value = (value << 8) | static_cast<std::uint32_t>(octet);
        if (part < 3) {
            char dot = 0;
            if (!(in >> dot) || dot != '.') return false;
        }
    }
    char extra = 0;
    if (in >> extra) return false;  // trailing garbage
    out = value;
    return true;
}

std::string IPBlocker::toString(std::uint32_t value) {
    std::ostringstream os;
    os << ((value >> 24) & 0xFF) << '.' << ((value >> 16) & 0xFF) << '.'
       << ((value >> 8) & 0xFF) << '.' << (value & 0xFF);
    return os.str();
}

bool IPBlocker::addRange(const std::string& rawSpec) {
    const std::string spec = trim(rawSpec);
    std::uint32_t low = 0, high = 0;

    const auto slash = spec.find('/');
    const auto dash  = spec.find('-');
    if (slash != std::string::npos) {
        std::uint32_t base = 0;
        int bits = -1;
        try {
            bits = std::stoi(spec.substr(slash + 1));
        } catch (...) {
            return false;
        }
        if (!parseIP(spec.substr(0, slash), base) || bits < 0 || bits > 32) return false;
        const std::uint32_t mask = bits == 0 ? 0u : (0xFFFFFFFFu << (32 - bits));
        low  = base & mask;
        high = low | ~mask;
    } else if (dash != std::string::npos) {
        if (!parseIP(spec.substr(0, dash), low) || !parseIP(spec.substr(dash + 1), high)) {
            return false;
        }
        if (low > high) std::swap(low, high);
    } else {
        if (!parseIP(spec, low)) return false;
        high = low;
    }

    ranges_.emplace_back(low, high);
    specs_.push_back(spec);
    return true;
}

bool IPBlocker::isBlocked(const std::string& ip) const {
    std::uint32_t value = 0;
    if (!parseIP(ip, value)) return true;
    for (const auto& r : ranges_) {
        if (value >= r.first && value <= r.second) return true;
    }
    return false;
}

const std::vector<std::pair<std::uint32_t, std::uint32_t>>& IPBlocker::ranges() const {
    return ranges_;
}

const std::vector<std::string>& IPBlocker::rangeSpecs() const {
    return specs_;
}
