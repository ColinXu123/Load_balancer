/**
 * @file Logger.h
 * @brief Colored terminal and plain-text file logging with verbose/quiet modes.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <string>

/**
 * @enum LogColor
 * @brief ANSI colors used to tell event types apart on the terminal.
 */
enum class LogColor {
    Plain,    ///< Default terminal color.
    Blue,     ///< Request generated.
    Green,    ///< Request completed.
    Red,      ///< Request blocked by the firewall.
    Yellow,   ///< Server added.
    Magenta,  ///< Server removed or draining.
    Cyan,     ///< Status / periodic checkpoint.
    Bold      ///< Headings and summaries.
};

/**
 * @class Logger
 * @brief Process-wide logger shared by every simulation component.
 *
 * Two logging modes:
 *  - @b Verbose: a log of the events where something changes, through
 *    event(): servers added, drained and taken offline, requests rejected by
 *    the firewall, and traffic spikes and lulls, plus periodic status lines.
 *    Routine work that happens nearly every clock cycle (requests arriving,
 *    being assigned and finishing) is only logged for a sample of requests
 *    (every @c request_log_every th one), so the log is not one line per cycle.
 *  - @b Quiet: only the starting configuration and the ending summary,
 *    which totals every kind of event. event() prints nothing.
 * always() prints in both modes and is used for the summaries.
 *
 * Messages go to the terminal (with ANSI color when enabled) and, if a log
 * file is open, to that file without color codes.
 */
class Logger {
public:
    /**
     * @brief Configures the logger. Call once before logging.
     * @param verbose  true for verbose mode, false for quiet mode.
     * @param color    true to emit ANSI color codes on the terminal.
     * @param logPath  File to mirror output into; empty disables the file.
     *                 Missing parent folders are created automatically.
     * @return true if the log file opened (or none was requested).
     */
    static bool init(bool verbose, bool color, const std::string& logPath);

    /**
     * @brief Logs an event where something changed (verbose mode only).
     * @param cycle  Simulation cycle on which the event happened.
     * @param source Component name, e.g. "LB-Streaming".
     * @param color  Terminal color for the line.
     * @param tag    Short event label, e.g. "SCALE+" or "BLOCKED".
     * @param text   Event details.
     */
    static void event(long cycle, const std::string& source, LogColor color,
                      const std::string& tag, const std::string& text);

    /**
     * @brief Logs a line in both verbose and quiet mode.
     * @param text  Line to print.
     * @param color Terminal color for the line.
     */
    static void always(const std::string& text, LogColor color = LogColor::Plain);

    /** @brief @return true when running in verbose mode. */
    static bool verbose();

    /** @brief Flushes and closes the log file. */
    static void close();

private:
    /// Writes one line to the terminal and the file.
    static void write(const std::string& text, LogColor color);

    static bool verbose_;          ///< Verbose (true) or quiet (false) mode.
    static bool color_;            ///< Whether ANSI colors are enabled.
    static std::ofstream file_;    ///< Optional plain-text log file.
};

#endif // LOGGER_H