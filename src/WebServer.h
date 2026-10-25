/**
 * @file WebServer.h
 * @brief Defines the WebServer class that processes one request at a time.
 */
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "Request.h"

/**
 * @class WebServer
 * @brief Simulates a single web server.
 *
 * A server holds at most one request. Each call to tick() advances the
 * server by one clock cycle; when the remaining time reaches zero the request
 * is complete and the server becomes idle, ready to ask its load balancer for
 * more work.
 *
 * A server can be marked as @em draining when the load balancer wants to
 * scale down. A draining server finishes its in-flight request but accepts no
 * new work, so no request is ever lost during scale-down.
 */
class WebServer {
public:
    /**
     * @brief Constructs an idle server.
     * @param id        Identifier unique within the owning load balancer.
     * @param createdAt Simulation cycle on which the server came online.
     */
    WebServer(int id, long createdAt);

    /**
     * @brief Gives the server a new request to work on.
     * @pre The server must be idle and not draining.
     * @param request The request to process.
     */
    void assign(const Request& request);

    /**
     * @brief Advances the server by one clock cycle.
     * @return true if the current request finished on this cycle.
     */
    bool tick();

    /** @brief @return true if the server is currently processing a request. */
    bool isBusy() const;

    /** @brief @return true if the server is idle and may accept a request. */
    bool canAccept() const;

    /** @brief @return true if the server has been scheduled for removal. */
    bool isDraining() const;

    /** @brief Marks the server for removal once its current request finishes. */
    void startDraining();

    /** @brief @return The request currently held (or last finished). */
    const Request& current() const;

    /** @brief @return Cycles left on the current request (0 when idle). */
    int remaining() const;

    /** @brief @return The server's identifier. */
    int id() const;

    /** @brief @return Cycle on which this server was created. */
    long createdAt() const;

    /** @brief @return Number of requests this server has completed. */
    long processedCount() const;

private:
    int     id_;          ///< Server identifier.
    long    createdAt_;   ///< Cycle on which the server was created.
    Request current_{};   ///< Request in progress (valid only when busy_).
    int     remaining_;   ///< Cycles left on current_.
    bool    busy_;        ///< True while processing a request.
    bool    draining_;    ///< True once scheduled for removal.
    long    processed_;   ///< Completed request count.
};

#endif // WEB_SERVER_H
