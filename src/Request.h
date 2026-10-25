/**
 * @file Request.h
 * @brief Defines the Request structure that represents a single web request.
 */
#ifndef REQUEST_H
#define REQUEST_H

#include <string>

/**
 * @struct Request
 * @brief A single unit of work flowing through the simulation.
 *
 * Every request carries a source and destination IPv4 address, the number of
 * clock cycles a web server needs to finish it, and a job type character
 * (@c 'P' for processing, @c 'S' for streaming). The id and creation cycle
 * are bookkeeping fields used for logging and wait-time statistics.
 */
struct Request {
    long        id;         ///< Unique, monotonically increasing request id.
    std::string ipIn;       ///< Source IPv4 address in dotted-quad form.
    std::string ipOut;      ///< Destination IPv4 address in dotted-quad form.
    int         time;       ///< Clock cycles required to process the request.
    char        jobType;    ///< 'P' = processing, 'S' = streaming.
    long        createdAt;  ///< Simulation cycle on which the request was created.
};

/**
 * @brief Returns a human-readable name for a job type character.
 * @param jobType Either 'P' or 'S'.
 * @return "Processing", "Streaming", or "Unknown".
 */
inline const char* jobTypeName(char jobType) {
    switch (jobType) {
        case 'P': return "Processing";
        case 'S': return "Streaming";
        default:  return "Unknown";
    }
}

#endif // REQUEST_H
