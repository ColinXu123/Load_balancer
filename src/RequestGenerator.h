/**
 * @file RequestGenerator.h
 * @brief Produces random requests according to configured distributions.
 */
#ifndef REQUEST_GENERATOR_H
#define REQUEST_GENERATOR_H

#include <random>
#include <string>

#include "Config.h"
#include "IPBlocker.h"
#include "Request.h"

/**
 * @class RequestGenerator
 * @brief Random source of new requests.
 *
 * Owns the simulation's random number engine. It chooses:
 *  - the job type, streaming with probability @c streaming_ratio;
 *  - a processing time drawn uniformly from the per-type range
 *    (@c processing_time_min/max or @c streaming_time_min/max);
 *  - random source and destination addresses. With probability
 *    @c attack_probability the source is drawn from one of the firewall's
 *    blocked ranges, which simulates hostile traffic for the IPBlocker to stop.
 *
 * It also decides how many requests arrive on each cycle: with probability
 * @c arrival_probability a burst of 1 to @c max_burst_size requests arrives.
 */
class RequestGenerator {
public:
    /**
     * @brief Builds a generator from configuration.
     * @param config  Simulation configuration.
     * @param blocker Firewall whose ranges are used for attack traffic.
     * @param seed    Random seed; 0 picks a non-deterministic seed.
     */
    RequestGenerator(const Config& config, const IPBlocker& blocker, unsigned seed);

    /**
     * @brief Creates a request with a randomly chosen job type.
     * @param cycle Current simulation cycle (stored as the creation time).
     * @return The new request.
     */
    Request generate(long cycle);

    /**
     * @brief Creates a request with a fixed job type.
     * @param cycle   Current simulation cycle.
     * @param jobType 'P' or 'S'.
     * @return The new request.
     */
    Request generate(long cycle, char jobType);

    /**
     * @brief Decides how many new requests arrive on this cycle.
     * @return Zero most cycles; otherwise a burst size between 1 and max_burst_size.
     */
    int arrivalsThisCycle();

    /** @brief @return The seed actually used (useful for reproducing a run). */
    unsigned seed() const;

private:
    /// Uniform random address across the whole IPv4 space (avoiding 0.x and 255.x).
    std::string randomIP();
    /// Random address inside one of the blocked ranges.
    std::string attackIP();
    /// Uniform integer in [lo, hi].
    int uniform(int lo, int hi);
    /// Uniform double in [0, 1).
    double chance();

    const IPBlocker& blocker_;   ///< Source of attack ranges.
    std::mt19937     rng_;       ///< Random engine.
    unsigned         seed_;      ///< Seed in use.
    long             nextId_;    ///< Next request id.

    double streamingRatio_;      ///< Probability a request is streaming.
    int    procMin_, procMax_;   ///< Processing-job time range.
    int    streamMin_, streamMax_; ///< Streaming-job time range.
    double attackProbability_;   ///< Probability the source IP is hostile.
    double arrivalProbability_;  ///< Probability of arrivals on a cycle.
    int    maxBurst_;            ///< Largest burst on one cycle.
};

#endif // REQUEST_GENERATOR_H
