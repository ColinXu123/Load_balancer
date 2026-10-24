/**
 * @file RequestGenerator.cpp
 * @brief Implementation of the RequestGenerator class.
 */
#include "RequestGenerator.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "Logger.h"

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
    arrivalRate_        = std::max(0.0, config.getDouble("arrival_rate", 0.6));
    waveAmplitude_      = std::clamp(config.getDouble("traffic_wave_amplitude", 0.5), 0.0, 1.0);
    wavePeriod_         = std::max(1L, config.getInt("traffic_wave_period", 25000));
    spikeChance_        = std::clamp(config.getDouble("spike_chance", 0.00005), 0.0, 1.0);
    spikeMultiplier_    = std::max(0.0, config.getDouble("spike_multiplier", 3.0));
    lullChance_         = std::clamp(config.getDouble("lull_chance", 0.00005), 0.0, 1.0);
    lullMultiplier_     = std::max(0.0, config.getDouble("lull_multiplier", 0.1));
    eventMin_           = static_cast<int>(std::max(1L, config.getInt("event_min_length", 1000)));
    eventMax_           = static_cast<int>(std::max<long>(eventMin_, config.getInt("event_max_length", 5000)));

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

int RequestGenerator::arrivalsThisCycle(long cycle) {
    // 1. Spikes and lulls: finish the current one, or maybe start a new one.
    if (eventLeft_ > 0) {
        if (--eventLeft_ == 0) {
            Logger::major(cycle, "Traffic", LogColor::Cyan, "TRAFFIC",
                          std::string(eventType_ == 'S' ? "spike" : "lull") +
                          " over, traffic back to normal");
            eventType_ = 'N';
            eventMult_ = 1.0;
        }
    } else {
        const double r = chance();
        const bool spike = r < spikeChance_;
        const bool lull  = !spike && r < spikeChance_ + lullChance_;
        if (spike || lull) {
            eventType_ = spike ? 'S' : 'L';
            eventMult_ = spike ? spikeMultiplier_ : lullMultiplier_;
            eventLeft_ = uniform(eventMin_, eventMax_);
            spike ? ++spikes_ : ++lulls_;
            std::ostringstream os;
            os << (spike ? "SPIKE: traffic x" : "LULL: traffic x") << eventMult_
               << " for " << eventLeft_ << " cycles";
            Logger::major(cycle, "Traffic", spike ? LogColor::Yellow : LogColor::Cyan,
                          "TRAFFIC", os.str());
        }
    }

    // 2. Slow wave, like busy and quiet hours of a day.
    const double pi   = 3.14159265358979323846;
    const double wave = 1.0 + waveAmplitude_ * std::sin(2.0 * pi * static_cast<double>(cycle) /
                                                        static_cast<double>(wavePeriod_));
    level_ = std::max(0.0, wave * eventMult_);

    // 3. Number of arrivals this cycle, averaging arrivalRate_ x level_.
    const double mean = arrivalRate_ * level_;
    if (mean <= 0.0) return 0;
    std::poisson_distribution<int> dist(mean);
    return dist(rng_);
}

double RequestGenerator::trafficLevel() const { return level_; }

const char* RequestGenerator::trafficState() const {
    return eventType_ == 'S' ? "spike" : eventType_ == 'L' ? "lull" : "normal";
}

int RequestGenerator::spikeCount() const { return spikes_; }

int RequestGenerator::lullCount() const { return lulls_; }

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