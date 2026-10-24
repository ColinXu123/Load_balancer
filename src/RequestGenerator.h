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
 * It also decides how many requests arrive on each cycle. The average rate
 * is @c arrival_rate requests per cycle, multiplied by a traffic level that
 * changes over time so the load is not flat:
 *  - a slow wave (@c traffic_wave_amplitude, @c traffic_wave_period) that
 *    raises and lowers traffic like busy and quiet hours of a day;
 *  - random @b spikes (@c spike_chance, @c spike_multiplier) where traffic
 *    jumps for a while, like a flash sale or a viral link;
 *  - random @b lulls (@c lull_chance, @c lull_multiplier) where traffic
 *    nearly stops, like an upstream outage or the middle of the night.
 * Spikes and lulls last between @c event_min_length and @c event_max_length
 * cycles. The number of arrivals on a cycle is drawn from a Poisson
 * distribution with the resulting mean.
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
     *
     * Advances the traffic pattern by one cycle (starting or ending spikes and
     * lulls, which are logged), then draws the arrival count.
     * @param cycle Current simulation cycle.
     * @return Number of new requests, usually 0-2 at normal load.
     */
    int arrivalsThisCycle(long cycle);

    /** @brief @return The traffic multiplier used on the most recent cycle (1.0 = normal). */
    double trafficLevel() const;

    /** @brief @return "spike", "lull" or "normal" for the most recent cycle. */
    const char* trafficState() const;

    /** @brief @return Number of traffic spikes started so far. */
    int spikeCount() const;

    /** @brief @return Number of traffic lulls started so far. */
    int lullCount() const;

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
    double arrivalRate_;         ///< Average new requests per cycle at normal traffic.
    double waveAmplitude_;       ///< Size of the slow wave (0 = flat, 0.5 = +/-50%).
    long   wavePeriod_;          ///< Cycles for one full wave.
    double spikeChance_;         ///< Per-cycle chance a spike starts.
    double spikeMultiplier_;     ///< Traffic multiplier during a spike.
    double lullChance_;          ///< Per-cycle chance a lull starts.
    double lullMultiplier_;      ///< Traffic multiplier during a lull.
    int    eventMin_, eventMax_; ///< Length range of spikes and lulls, in cycles.

    long   eventLeft_  = 0;      ///< Cycles left in the current spike or lull.
    double eventMult_  = 1.0;    ///< Multiplier of the current event (1.0 if none).
    char   eventType_  = 'N';    ///< 'S' spike, 'L' lull, 'N' none.
    double level_      = 1.0;    ///< Traffic multiplier on the last cycle.
    int    spikes_     = 0;      ///< Spikes started.
    int    lulls_      = 0;      ///< Lulls started.
};

#endif // REQUEST_GENERATOR_H