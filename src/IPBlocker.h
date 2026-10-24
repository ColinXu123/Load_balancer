/**
 * @file IPBlocker.h
 * @brief Firewall that rejects requests from configured IPv4 ranges.
 */
#ifndef IP_BLOCKER_H
#define IP_BLOCKER_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

/**
 * @class IPBlocker
 * @brief Simulated firewall / denial-of-service filter.
 *
 * Holds a list of inclusive IPv4 ranges. A request whose source address
 * (IP In) falls in any range is rejected before it reaches a load balancer
 * queue. Ranges can be given either as @c "a.b.c.d-e.f.g.h" or in CIDR form
 * @c "a.b.c.d/n".
 */
class IPBlocker {
public:
    /**
     * @brief Adds a blocked range from its text form.
     * @param spec Range text, e.g. "10.0.0.0/8" or "192.168.1.0-192.168.1.255".
     * @return true if the range parsed and was added, false if malformed.
     */
    bool addRange(const std::string& spec);

    /**
     * @brief Tests whether an address is inside any blocked range.
     * @param ip Dotted-quad IPv4 address.
     * @return true if the address is blocked (malformed addresses are blocked).
     */
    bool isBlocked(const std::string& ip) const;

    /** @brief @return The list of blocked ranges as [low, high] integer pairs. */
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& ranges() const;

    /** @brief @return The ranges as they were written in the configuration. */
    const std::vector<std::string>& rangeSpecs() const;

    /**
     * @brief Parses a dotted-quad address into a 32-bit integer.
     * @param ip  Address text.
     * @param out Receives the parsed value on success.
     * @return true on success, false if the text is not a valid IPv4 address.
     */
    static bool parseIP(const std::string& ip, std::uint32_t& out);

    /**
     * @brief Formats a 32-bit integer as a dotted-quad address.
     * @param value Address as an integer.
     * @return Dotted-quad text.
     */
    static std::string toString(std::uint32_t value);

private:
    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges_;  ///< Inclusive ranges.
    std::vector<std::string> specs_;                               ///< Original text.
};

#endif // IP_BLOCKER_H
