/**
 * @file Config.h
 * @brief Loads simulation parameters from a key=value configuration file.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <map>
#include <string>
#include <vector>

/**
 * @class Config
 * @brief Simple key=value configuration store with typed getters.
 *
 * File format: one @c key=value pair per line. Blank lines and lines starting
 * with @c '#' are ignored, and anything after a @c '#' on a line is a comment.
 * The key @c blocked_range may appear many times; every occurrence is kept.
 * Every getter takes a default that is used when the key is absent or
 * malformed, so the simulation runs even without a configuration file.
 */
class Config {
public:
    /**
     * @brief Reads settings from a file.
     * @param path Path to the configuration file.
     * @return true if the file was opened, false if it could not be read.
     */
    bool load(const std::string& path);

    /**
     * @brief Sets or overrides a value (used for command-line overrides).
     * @param key   Setting name.
     * @param value Setting value as text.
     */
    void set(const std::string& key, const std::string& value);

    /**
     * @brief Reads an integer setting.
     * @param key      Setting name.
     * @param fallback Value returned if the key is missing or malformed.
     * @return The parsed value or @p fallback.
     */
    long getInt(const std::string& key, long fallback) const;

    /**
     * @brief Reads a floating-point setting.
     * @param key      Setting name.
     * @param fallback Value returned if the key is missing or malformed.
     * @return The parsed value or @p fallback.
     */
    double getDouble(const std::string& key, double fallback) const;

    /**
     * @brief Reads a boolean setting (1/0, true/false, yes/no, on/off).
     * @param key      Setting name.
     * @param fallback Value returned if the key is missing or malformed.
     * @return The parsed value or @p fallback.
     */
    bool getBool(const std::string& key, bool fallback) const;

    /**
     * @brief Reads a text setting.
     * @param key      Setting name.
     * @param fallback Value returned if the key is missing.
     * @return The stored text or @p fallback.
     */
    std::string getString(const std::string& key, const std::string& fallback) const;

    /** @brief @return Every @c blocked_range entry in file order. */
    const std::vector<std::string>& blockedRanges() const;

    /** @brief @return Path of the file last loaded, or empty if none. */
    const std::string& sourcePath() const;

private:
    std::map<std::string, std::string> values_;  ///< Scalar settings.
    std::vector<std::string> blockedRanges_;     ///< Repeated blocked_range entries.
    std::string sourcePath_;                     ///< File that was loaded.
};

#endif // CONFIG_H
