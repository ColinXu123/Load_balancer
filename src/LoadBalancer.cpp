/**
 * @file LoadBalancer.cpp
 * @brief Implementation of the LoadBalancer class.
 */
#include "LoadBalancer.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "Logger.h"

namespace {

/// Formats a ratio as a percentage with one decimal place.
std::string percent(double numerator, double denominator) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1)
       << (denominator > 0 ? 100.0 * numerator / denominator : 0.0) << '%';
    return os.str();
}

/// Formats a double with a fixed number of decimals.
std::string fixed(double value, int decimals) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(decimals) << value;
    return os.str();
}

/// Formats a summary row as "  label ........ value".
std::string row(const std::string& label, const std::string& value) {
    std::ostringstream os;
    os << "  " << std::left << std::setw(38) << label << value;
    return os.str();
}

} // namespace

LoadBalancer::LoadBalancer(const std::string& name, const Config& config, int initialServers)
    : name_(name) {
    scaleInterval_ = std::max(0L, config.getInt("scale_interval", 250));
    lowThreshold_  = static_cast<int>(config.getInt("scale_low_threshold", 50));
    highThreshold_ = static_cast<int>(config.getInt("scale_high_threshold", 80));
    minServers_    = static_cast<int>(std::max(1L, config.getInt("min_servers", 1)));
    maxServers_    = static_cast<int>(std::max<long>(minServers_, config.getInt("max_servers", 500)));
    sampleEvery_   = std::max(0L, config.getInt("request_log_every", 100));

    const int start = std::clamp(initialServers, minServers_, maxServers_);
    for (int i = 0; i < start; ++i) {
        servers_.push_back(std::make_unique<WebServer>(nextServerId_++, 0));
    }
    stats_.initialServers = start;
    stats_.peakServers    = start;
    stats_.lowServers     = start;
}

void LoadBalancer::submit(const Request& request, bool initial) {
    queue_.push(request);
    ++stats_.generated;
    if (initial) {
        ++stats_.initialQueue;
        return;
    }
    // Requests arrive almost every cycle, so only a sample (every Nth request)
    // is logged, to show the random IPs, job types and times being generated.
    if (sampled(request)) {
        std::ostringstream os;
        os << "request #" << request.id << " " << request.jobType << " "
           << request.ipIn << " -> " << request.ipOut << " (" << request.time
           << " cycles) queued, queue=" << queue_.size();
        Logger::event(clock_, name_, LogColor::Blue, "NEW", os.str());
    }
}

bool LoadBalancer::sampled(const Request& request) const {
    return sampleEvery_ > 0 && request.id % sampleEvery_ == 0;
}

void LoadBalancer::recordBlocked(const Request& request) {
    ++stats_.blocked;
    std::ostringstream os;
    os << "request #" << request.id << " from " << request.ipIn
       << " rejected by firewall";
    Logger::event(clock_, name_, LogColor::Red, "BLOCKED", os.str());
}

void LoadBalancer::step() {
    // 1. Advance every server one cycle. Completions happen almost every cycle,
    //    so they are counted (and shown per server in the summary), not logged.
    for (auto& server : servers_) {
        if (server->tick()) {
            const Request& done = server->current();
            ++stats_.processed;
            stats_.totalServiceTime += done.time;
            if (sampled(done)) {
                std::ostringstream os;
                os << "server " << server->id() << " finished request #" << done.id
                   << " " << done.jobType << " after " << done.time << " cycles";
                Logger::event(clock_, name_, LogColor::Green, "DONE", os.str());
            }
        }
    }

    // 2. Retire draining servers that are now idle (their work is complete).
    for (auto it = servers_.begin(); it != servers_.end();) {
        if ((*it)->isDraining() && !(*it)->isBusy()) {
            std::ostringstream os;
            os << "server " << (*it)->id() << " drained and taken offline, active="
               << activeServers();
            Logger::event(clock_, name_, LogColor::Magenta, "OFFLINE", os.str());
            ++stats_.serversDrained;
            it = servers_.erase(it);
        } else {
            ++it;
        }
    }

    // 3. Give queued work to idle servers.
    dispatch();

    // 4. Statistics for this cycle.
    const long queued = static_cast<long>(queue_.size());
    stats_.queueSum += queued;
    stats_.maxQueue  = std::max(stats_.maxQueue, queued);
    stats_.serverCycles += static_cast<long>(servers_.size());
    stats_.busyServerCycles += inFlight();

    // 5. Scaling, respecting the cooldown after the last action.
    if (cooldown_ > 0) {
        --cooldown_;
    } else if (evaluateScaling()) {
        cooldown_ = scaleInterval_;
    }

    ++clock_;
}

void LoadBalancer::dispatch() {
    for (auto& server : servers_) {
        if (queue_.empty()) return;
        if (server->canAccept()) {
            Request next = queue_.pop();
            stats_.totalWait += clock_ - next.createdAt;
            ++stats_.started;
            server->assign(next);
            if (sampled(next)) {
                std::ostringstream os;
                os << "request #" << next.id << " " << next.jobType << " -> server "
                   << server->id() << " (waited " << clock_ - next.createdAt
                   << " cycles, needs " << next.time << ")";
                Logger::event(clock_, name_, LogColor::Plain, "ASSIGN", os.str());
            }
        }
    }
}

bool LoadBalancer::evaluateScaling() {
    const long active = activeServers();
    const long queued = static_cast<long>(queue_.size());

    if (queued > static_cast<long>(highThreshold_) * active && active < maxServers_) {
        addServer();
        return true;
    }
    if (queued < static_cast<long>(lowThreshold_) * active && active > minServers_) {
        removeServer();
        return true;
    }
    return false;
}

void LoadBalancer::addServer() {
    servers_.push_back(std::make_unique<WebServer>(nextServerId_++, clock_));
    ++stats_.serversAdded;
    trackServerCount();
    std::ostringstream os;
    os << "queue " << queue_.size() << " > " << highThreshold_ << " x "
       << activeServers() - 1 << " -> server " << servers_.back()->id()
       << " added, active=" << activeServers();
    Logger::event(clock_, name_, LogColor::Yellow, "SCALE+", os.str());
}

void LoadBalancer::removeServer() {
    const int before = activeServers();

    // Prefer an idle server: it can leave immediately with nothing in flight.
    auto idle = std::find_if(servers_.begin(), servers_.end(),
                             [](const std::unique_ptr<WebServer>& s) { return s->canAccept(); });

    std::ostringstream os;
    os << "queue " << queue_.size() << " < " << lowThreshold_ << " x " << before << " -> ";
    if (idle != servers_.end()) {
        os << "idle server " << (*idle)->id() << " removed";
        servers_.erase(idle);
    } else {
        // Otherwise drain the busy server closest to finishing.
        WebServer* pick = nullptr;
        for (auto& s : servers_) {
            if (s->isDraining()) continue;
            if (!pick || s->remaining() < pick->remaining()) pick = s.get();
        }
        if (!pick) return;
        pick->startDraining();
        os << "server " << pick->id() << " draining (finishing request #"
           << pick->current().id << ", " << pick->remaining() << " cycles left)";
    }
    ++stats_.serversRemoved;
    trackServerCount();
    os << ", active=" << activeServers();
    Logger::event(clock_, name_, LogColor::Magenta, "SCALE-", os.str());
}

void LoadBalancer::trackServerCount() {
    const int active = activeServers();
    stats_.peakServers = std::max(stats_.peakServers, active);
    stats_.lowServers  = std::min(stats_.lowServers, active);
}

long LoadBalancer::clock() const { return clock_; }

const std::string& LoadBalancer::name() const { return name_; }

std::size_t LoadBalancer::queueSize() const { return queue_.size(); }

int LoadBalancer::activeServers() const {
    return static_cast<int>(std::count_if(servers_.begin(), servers_.end(),
        [](const std::unique_ptr<WebServer>& s) { return !s->isDraining(); }));
}

int LoadBalancer::totalServers() const { return static_cast<int>(servers_.size()); }

int LoadBalancer::inFlight() const {
    return static_cast<int>(std::count_if(servers_.begin(), servers_.end(),
        [](const std::unique_ptr<WebServer>& s) { return s->isBusy(); }));
}

int LoadBalancer::idleServers() const {
    return static_cast<int>(std::count_if(servers_.begin(), servers_.end(),
        [](const std::unique_ptr<WebServer>& s) { return !s->isBusy(); }));
}

int LoadBalancer::drainingServers() const {
    return static_cast<int>(std::count_if(servers_.begin(), servers_.end(),
        [](const std::unique_ptr<WebServer>& s) { return s->isDraining(); }));
}

const LoadBalancerStats& LoadBalancer::stats() const { return stats_; }

std::string LoadBalancer::statusLine() const {
    std::ostringstream os;
    os << std::left << std::setw(14) << name_ << std::right
       << " queue=" << std::setw(6) << queue_.size()
       << "  active=" << std::setw(3) << activeServers()
       << "  inactive=" << std::setw(3) << idleServers()
       << "  draining=" << std::setw(2) << drainingServers()
       << "  per-server=" << std::setw(6)
       << fixed(activeServers() ? static_cast<double>(queue_.size()) / activeServers() : 0.0, 1)
       << "  done=" << std::setw(7) << stats_.processed
       << "  rejected=" << std::setw(5) << stats_.blocked
       << "  util now=" << std::setw(6)
       << percent(static_cast<double>(inFlight()), static_cast<double>(totalServers()));
    return os.str();
}

void LoadBalancer::printSummary() const {
    const auto& s = stats_;
    const double cycles = static_cast<double>(clock_);

    Logger::always("--- " + name_ + " ---", LogColor::Bold);
    Logger::always(row("Servers (initial -> final)",
                       std::to_string(s.initialServers) + " -> " + std::to_string(activeServers())));
    Logger::always(row("Servers (low / peak)",
                       std::to_string(s.lowServers) + " / " + std::to_string(s.peakServers)));
    Logger::always(row("Servers added / removed",
                       std::to_string(s.serversAdded) + " / " + std::to_string(s.serversRemoved)));
    Logger::always(row("Active servers at end",
                       std::to_string(activeServers()) + " (" +
                       std::to_string(activeServers() - idleServers()) + " busy)"));
    Logger::always(row("Inactive (idle) servers at end", std::to_string(idleServers())));
    Logger::always(row("Draining servers at end", std::to_string(drainingServers())));
    Logger::always(row("Average inactive servers per cycle",
                       fixed(cycles > 0 ? (s.serverCycles - s.busyServerCycles) / cycles : 0.0, 2)));
    Logger::always(row("Servers drained before removal", std::to_string(s.serversDrained)));
    {
        // Evidence that work is spread across the pool: requests finished by
        // each server that is still online at the end.
        std::ostringstream os;
        for (const auto& server : servers_) {
            os << "#" << server->id() << ":" << server->processedCount() << " ";
        }
        Logger::always(row("Requests done per online server", os.str()));
    }
    Logger::always(row("Initial queue", std::to_string(s.initialQueue)));
    Logger::always(row("Requests accepted (incl. initial)", std::to_string(s.generated)));
    Logger::always(row("Requests rejected by firewall",
                       std::to_string(s.blocked) + " (" +
                       fixed(s.generated + s.blocked
                                 ? 100.0 * s.blocked / (s.generated + s.blocked) : 0.0, 2) + "%)"));
    Logger::always(row("Requests processed", std::to_string(s.processed)));
    Logger::always(row("Remaining in queue", std::to_string(queue_.size())));
    Logger::always(row("In flight at end", std::to_string(inFlight())));
    Logger::always(row("Average queue size", fixed(cycles > 0 ? s.queueSum / cycles : 0.0, 1)));
    Logger::always(row("Maximum queue size", std::to_string(s.maxQueue)));
    Logger::always(row("Average servers online",
                       fixed(cycles > 0 ? s.serverCycles / cycles : 0.0, 2)));
    Logger::always(row("Average server utilization",
                       percent(static_cast<double>(s.busyServerCycles),
                               static_cast<double>(s.serverCycles))));
    Logger::always(row("Average wait in queue (cycles)",
                       fixed(s.started ? static_cast<double>(s.totalWait) / s.started : 0.0, 1)));
    Logger::always(row("Average processing time (cycles)",
                       fixed(s.processed ? static_cast<double>(s.totalServiceTime) / s.processed : 0.0, 1)));
    Logger::always(row("Requests completed per cycle",
                       fixed(cycles > 0 ? s.processed / cycles : 0.0, 4)));
}