/**
 * @file RequestGenerator.cpp
 * @brief Implementation of the RequestGenerator class.
 */
#include "RequestGenerator.h"

#include <algorithm>

RequestGenerator::RequestGenerator(const Config& config, const IPBlocker& blocker,
                                   unsigned seed)
    : blocker_(blocker), nextId_(1) {
    seed_ = seed != 0 ? seed : std::random_device{}();
    rng_.seed(seed_);

    streamingRatio_     = std::clamp(config.getDouble("streaming_ratio", 0.35), 0.0, 1.0);
    procMin_            = static_cast<int>(config.getInt("processing_time_min", 5));
    procMax_            = static_cast<int>(config.getInt("processing_time_max", 45));
    streamMin_          = static_cast<int>(config.getInt("streaming_time_min", 40));
    streamMax_          = static_cast<int>(config.getInt("streaming_time_max", 120));
    attackProbability_  = std::clamp(config.getDouble("attack_probability", 0.05), 0.0, 1.0);
    arrivalProbability_ = std::clamp(config.getDouble("arrival_probability", 0.30), 0.0, 1.0);
    maxBurst_           = std::max(1L, config.getInt("max_burst_size", 3));

    procMin_   = std::max(1, procMin_);
    streamMin_ = std::max(1, streamMin_);
    if (procMax_ < procMin_)     procMax_ = procMin_;
    if (streamMax_ < streamMin_) streamMax_ = streamMin_;
}

Request RequestGenerator::generate(long cycle) {
    return generate(cycle, chance() < streamingRatio_ ? 'S' : 'P');
}

Request RequestGenerator::generate(long cycle, char jobType) {
    Request r;
    r.id        = nextId_++;
    r.jobType   = jobType == 'S' ? 'S' : 'P';
    r.time      = r.jobType == 'S' ? uniform(streamMin_, streamMax_)
                                   : uniform(procMin_, procMax_);
    const bool hostile = !blocker_.ranges().empty() && chance() < attackProbability_;
    r.ipIn      = hostile ? attackIP() : randomIP();
    r.ipOut     = randomIP();
    r.createdAt = cycle;
    return r;
}

int RequestGenerator::arrivalsThisCycle() {
    if (chance() >= arrivalProbability_) return 0;
    return uniform(1, maxBurst_);
}

unsigned RequestGenerator::seed() const { return seed_; }

std::string RequestGenerator::randomIP() {
    const auto value = (static_cast<std::uint32_t>(uniform(1, 254)) << 24) |
                       (static_cast<std::uint32_t>(uniform(0, 255)) << 16) |
                       (static_cast<std::uint32_t>(uniform(0, 255)) << 8) |
                        static_cast<std::uint32_t>(uniform(1, 254));
    return IPBlocker::toString(value);
}

std::string RequestGenerator::attackIP() {
    const auto& ranges = blocker_.ranges();
    const auto& r = ranges[static_cast<std::size_t>(uniform(0, static_cast<int>(ranges.size()) - 1))];
    std::uniform_int_distribution<std::uint32_t> dist(r.first, r.second);
    return IPBlocker::toString(dist(rng_));
}

int RequestGenerator::uniform(int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng_);
}

double RequestGenerator::chance() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng_);
}
