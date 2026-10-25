/**
 * @file Switch.h
 * @brief Front door of the simulation: firewall check plus job-type routing.
 */
#ifndef SWITCH_H
#define SWITCH_H

#include <memory>
#include <string>
#include <vector>

#include "Config.h"
#include "IPBlocker.h"
#include "LoadBalancer.h"
#include "RequestGenerator.h"

/**
 * @class Switch
 * @brief Routes every incoming request through the firewall to a load balancer.
 *
 * Two modes are supported:
 *  - @b Switch @b mode (@c use_switch=1): streaming ('S') requests go to a
 *    dedicated streaming balancer and processing ('P') requests to a
 *    processing balancer. The starting servers are split between the two by
 *    @c streaming_server_share. Each balancer scales and reports independently.
 *  - @b Single @b mode (@c use_switch=0): one balancer receives everything.
 *
 * Every cycle the switch asks the RequestGenerator how many requests arrive,
 * filters each through the IPBlocker, and forwards the survivors. It then
 * steps every balancer by one cycle, so all balancers share one clock.
 */
class Switch {
public:
    /**
     * @brief Builds the balancers for the chosen mode.
     * @param config       Simulation configuration.
     * @param generator    Request source.
     * @param blocker      Firewall applied before routing.
     * @param totalServers Servers to start with across all balancers.
     * @param useSwitch    true for streaming/processing split, false for one balancer.
     */
    Switch(const Config& config, RequestGenerator& generator, const IPBlocker& blocker,
           int totalServers, bool useSwitch);

    /**
     * @brief Fills each balancer's queue before the run.
     * @param perServer Requests to create per starting server (typically 100).
     */
    void fillInitialQueues(int perServer);

    /** @brief Runs one cycle: generate arrivals, route them, step every balancer. */
    void step();

    /**
     * @brief Sends one request through the firewall to its balancer.
     * @param request The request to route.
     * @param initial true during the initial queue fill.
     */
    void route(const Request& request, bool initial = false);

    /** @brief @return Current simulation cycle (shared by all balancers). */
    long clock() const;

    /** @brief @return The balancers, in creation order. */
    const std::vector<std::unique_ptr<LoadBalancer>>& balancers() const;

    /** @brief @return true when running in streaming/processing switch mode. */
    bool switchMode() const;

    /** @brief @return Requests created by the generator (accepted + blocked). */
    long totalGenerated() const;

    /** @brief @return Requests rejected by the firewall. */
    long totalBlocked() const;

    /** @brief Prints combined statistics across every balancer. */
    void printCombinedSummary() const;

private:
    /// Picks the balancer responsible for a job type.
    LoadBalancer& target(char jobType);

    RequestGenerator& generator_;                        ///< Request source.
    const IPBlocker&  blocker_;                          ///< Firewall.
    std::vector<std::unique_ptr<LoadBalancer>> lbs_;     ///< Owned balancers.
    LoadBalancer* streaming_  = nullptr;                 ///< Target for 'S'.
    LoadBalancer* processing_ = nullptr;                 ///< Target for 'P'.
    bool  useSwitch_;                                    ///< Mode flag.
    long  clock_          = 0;                           ///< Shared clock.
    long  generated_      = 0;                           ///< Requests created.
    long  blocked_        = 0;                           ///< Requests rejected.
};

#endif // SWITCH_H
