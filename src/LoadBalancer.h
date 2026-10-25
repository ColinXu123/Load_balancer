/**
 * @file LoadBalancer.h
 * @brief Defines the LoadBalancer class and its statistics record.
 */
#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H

#include <memory>
#include <string>
#include <vector>

#include "Config.h"
#include "Request.h"
#include "RequestQueue.h"
#include "WebServer.h"

/**
 * @struct LoadBalancerStats
 * @brief Counters collected by one load balancer over a run.
 */
struct LoadBalancerStats {
    long initialQueue      = 0;  ///< Requests in the queue before cycle 0.
    long generated         = 0;  ///< Requests accepted into this balancer's queue.
    long blocked           = 0;  ///< Requests for this balancer stopped by the firewall.
    long processed         = 0;  ///< Requests completed by its servers.
    long started           = 0;  ///< Requests handed to a server.
    long totalWait         = 0;  ///< Sum of queue wait (cycles) over started requests.
    long totalServiceTime  = 0;  ///< Sum of processing time of completed requests.
    long queueSum          = 0;  ///< Sum of end-of-cycle queue sizes (for the average).
    long maxQueue          = 0;  ///< Largest end-of-cycle queue size.
    long serverCycles      = 0;  ///< Sum over cycles of servers online.
    long busyServerCycles  = 0;  ///< Sum over cycles of servers busy.
    int  initialServers    = 0;  ///< Servers at start.
    int  peakServers       = 0;  ///< Most active servers at once.
    int  lowServers        = 0;  ///< Fewest active servers at once.
    int  serversAdded      = 0;  ///< Scale-up actions.
    int  serversRemoved    = 0;  ///< Scale-down actions.
    int  serversDrained    = 0;  ///< Busy servers that drained, then went offline.
};

/**
 * @class LoadBalancer
 * @brief Owns a request queue and a pool of web servers, and runs its clock.
 *
 * On every call to step() the balancer:
 *  1. advances each server one cycle and records completions;
 *  2. retires draining servers that have finished their last request;
 *  3. hands queued requests to idle servers (FIFO);
 *  4. records queue and utilization statistics;
 *  5. applies the scaling rule, unless it is still in a cooldown period.
 *
 * Scaling rule (thresholds are configurable, defaults 50 and 80):
 *  - queue &gt; 80 x active servers: add one server;
 *  - queue &lt; 50 x active servers: remove one server;
 *  - after either action, wait @c scale_interval cycles before checking again.
 *
 * Removal never discards work. An idle server is removed at once; otherwise
 * the busy server closest to finishing is put into draining mode and leaves
 * the pool only after it completes its request.
 */
class LoadBalancer {
public:
    /**
     * @brief Creates a balancer with an initial pool of servers.
     * @param name           Display name used in logs, e.g. "LB-Streaming".
     * @param config         Simulation configuration (scaling parameters).
     * @param initialServers Number of servers to start with (at least 1).
     */
    LoadBalancer(const std::string& name, const Config& config, int initialServers);

    /**
     * @brief Accepts a request that passed the firewall into the queue.
     * @param request The request to enqueue.
     * @param initial true when filling the queue before the run starts.
     */
    void submit(const Request& request, bool initial = false);

    /**
     * @brief Records a request for this balancer that the firewall rejected.
     * @param request The rejected request (logged, never queued).
     */
    void recordBlocked(const Request& request);

    /** @brief Runs one clock cycle (see the class description). */
    void step();

    /** @brief @return The balancer's current clock value. */
    long clock() const;

    /** @brief @return The display name. */
    const std::string& name() const;

    /** @brief @return Number of requests waiting in the queue. */
    std::size_t queueSize() const;

    /** @brief @return Servers accepting work (excludes draining servers). */
    int activeServers() const;

    /** @brief @return All servers online, including draining ones. */
    int totalServers() const;

    /** @brief @return Requests currently being processed by servers. */
    int inFlight() const;

    /**
     * @brief Counts inactive servers: online but not processing a request.
     * @return Number of idle servers (draining servers are always busy, so
     *         every idle server is also an active one waiting for work).
     */
    int idleServers() const;

    /**
     * @brief Counts servers scheduled for removal that are finishing their last request.
     * @return Number of draining servers.
     */
    int drainingServers() const;

    /** @brief @return Statistics gathered so far. */
    const LoadBalancerStats& stats() const;

    /** @brief @return A one-line snapshot: queue, active/inactive/draining servers, rejected count and current utilization (busy / online servers right now). */
    std::string statusLine() const;

    /** @brief Prints this balancer's end-of-run statistics through the Logger. */
    void printSummary() const;

private:
    /// Moves queued requests onto idle servers.
    void dispatch();
    /// Applies the scaling rule; returns true if a scaling action was taken.
    bool evaluateScaling();
    /// Brings one new server online.
    void addServer();
    /// Removes an idle server, or puts the busy server nearest completion into draining.
    void removeServer();
    /// Updates peak/low server counters.
    void trackServerCount();
    /// True if this request is one of the sampled ones followed in the verbose log.
    bool sampled(const Request& request) const;

    std::string name_;                                  ///< Display name.
    RequestQueue queue_;                                ///< Pending requests.
    std::vector<std::unique_ptr<WebServer>> servers_;   ///< Server pool.
    long clock_          = 0;                           ///< Current cycle.
    long cooldown_       = 0;                           ///< Cycles left before next scaling check.
    int  nextServerId_   = 1;                           ///< Id for the next server created.

    long scaleInterval_;   ///< Cooldown after a scaling action.
    int  lowThreshold_;    ///< Remove a server below lowThreshold_ x active.
    int  highThreshold_;   ///< Add a server above highThreshold_ x active.
    int  minServers_;      ///< Floor on active servers.
    int  maxServers_;      ///< Ceiling on active servers.
    long sampleEvery_;     ///< Follow every Nth request in the verbose log (0 = none).

    LoadBalancerStats stats_;  ///< Running statistics.
};

#endif // LOAD_BALANCER_H