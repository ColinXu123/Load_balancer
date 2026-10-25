/**
 * @file Switch.cpp
 * @brief Implementation of the Switch class.
 */
#include "Switch.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "Logger.h"

Switch::Switch(const Config& config, RequestGenerator& generator, const IPBlocker& blocker,
               int totalServers, bool useSwitch)
    : generator_(generator), blocker_(blocker), useSwitch_(useSwitch) {
    if (useSwitch_ && totalServers >= 2) {
        const double share = std::clamp(config.getDouble("streaming_server_share", 0.6), 0.0, 1.0);
        int streamServers = static_cast<int>(std::lround(totalServers * share));
        streamServers = std::clamp(streamServers, 1, totalServers - 1);
        lbs_.push_back(std::make_unique<LoadBalancer>("LB-Streaming", config, streamServers));
        lbs_.push_back(std::make_unique<LoadBalancer>("LB-Processing", config,
                                                      totalServers - streamServers));
        streaming_  = lbs_[0].get();
        processing_ = lbs_[1].get();
    } else {
        useSwitch_ = false;
        lbs_.push_back(std::make_unique<LoadBalancer>("LB-Main", config, totalServers));
        streaming_ = processing_ = lbs_[0].get();
    }
}

void Switch::fillInitialQueues(int perServer) {
    for (auto& lb : lbs_) {
        const long target = static_cast<long>(perServer) * lb->activeServers();
        // Keep generating until the queue holds exactly the target; requests the
        // firewall blocks along the way are still counted as blocked.
        while (lb->stats().initialQueue < target) {
            // In switch mode each balancer is filled with its own job type.
            Request r = !useSwitch_ ? generator_.generate(0)
                        : generator_.generate(0, lb.get() == streaming_ ? 'S' : 'P');
            route(r, true);
        }
    }
}

void Switch::step() {
    const int arrivals = generator_.arrivalsThisCycle(clock_);
    for (int i = 0; i < arrivals; ++i) {
        route(generator_.generate(clock_));
    }
    for (auto& lb : lbs_) {
        lb->step();
    }
    ++clock_;
}

void Switch::route(const Request& request, bool initial) {
    ++generated_;
    LoadBalancer& lb = target(request.jobType);
    if (blocker_.isBlocked(request.ipIn)) {
        ++blocked_;
        lb.recordBlocked(request);
        return;
    }
    lb.submit(request, initial);
}

LoadBalancer& Switch::target(char jobType) {
    return jobType == 'S' ? *streaming_ : *processing_;
}

long Switch::clock() const { return clock_; }

const std::vector<std::unique_ptr<LoadBalancer>>& Switch::balancers() const { return lbs_; }

bool Switch::switchMode() const { return useSwitch_; }

long Switch::totalGenerated() const { return generated_; }

long Switch::totalBlocked() const { return blocked_; }

void Switch::printCombinedSummary() const {
    long accepted = 0, processed = 0, queued = 0, inflight = 0, idle = 0, draining = 0;
    long added = 0, removed = 0, serverCycles = 0, busyCycles = 0, queueSum = 0, maxQ = 0;
    int initialServers = 0, finalServers = 0;
    for (const auto& lb : lbs_) {
        const auto& s = lb->stats();
        accepted       += s.generated;
        processed      += s.processed;
        queued         += static_cast<long>(lb->queueSize());
        inflight       += lb->inFlight();
        idle           += lb->idleServers();
        draining       += lb->drainingServers();
        added          += s.serversAdded;
        removed        += s.serversRemoved;
        serverCycles   += s.serverCycles;
        busyCycles     += s.busyServerCycles;
        queueSum       += s.queueSum;
        maxQ           += s.maxQueue;
        initialServers += s.initialServers;
        finalServers   += lb->activeServers();
    }
    const double cycles = static_cast<double>(clock_);

    auto row = [](const std::string& label, const std::string& value) {
        std::ostringstream os;
        os << "  " << std::left << std::setw(38) << label << value;
        return os.str();
    };
    auto fixed = [](double v, int d) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(d) << v;
        return os.str();
    };

    Logger::always("--- All balancers combined ---", LogColor::Bold);
    Logger::always(row("Total simulation duration (cycles)", std::to_string(clock_)));
    Logger::always(row("Servers (initial -> final)",
                       std::to_string(initialServers) + " -> " + std::to_string(finalServers)));
    Logger::always(row("Servers added / removed",
                       std::to_string(added) + " / " + std::to_string(removed)));
    Logger::always(row("Active servers at end",
                       std::to_string(finalServers) + " (" +
                       std::to_string(finalServers - idle) + " busy)"));
    Logger::always(row("Inactive (idle) servers at end", std::to_string(idle)));
    Logger::always(row("Draining servers at end", std::to_string(draining)));
    Logger::always(row("Average inactive servers per cycle",
                       fixed(cycles > 0 ? (serverCycles - busyCycles) / cycles : 0.0, 2)));
    Logger::always(row("Requests generated (incl. initial)", std::to_string(generated_)));
    Logger::always(row("Requests rejected by firewall",
                       std::to_string(blocked_) + " (" +
                       fixed(generated_ ? 100.0 * blocked_ / generated_ : 0.0, 2) + "%)"));
    Logger::always(row("Requests accepted", std::to_string(accepted)));
    Logger::always(row("Requests processed", std::to_string(processed)));
    Logger::always(row("Remaining in queue", std::to_string(queued)));
    Logger::always(row("In flight at end", std::to_string(inflight)));
    Logger::always(row("Average total queue size", fixed(cycles > 0 ? queueSum / cycles : 0.0, 1)));
    if (lbs_.size() > 1) {
        Logger::always(row("Sum of per-balancer max queues", std::to_string(maxQ)));
    } else {
        Logger::always(row("Maximum queue size", std::to_string(maxQ)));
    }
    Logger::always(row("Average server utilization",
                       fixed(serverCycles ? 100.0 * busyCycles / serverCycles : 0.0, 1) + "%"));
    Logger::always(row("Requests completed per cycle",
                       fixed(cycles > 0 ? processed / cycles : 0.0, 4)));
    Logger::always(row("Accounting check",
                       accepted == processed + queued + inflight ? "OK (accepted = processed + queued + in flight)"
                                                                 : "MISMATCH"));
}