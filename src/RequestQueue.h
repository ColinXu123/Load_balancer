/**
 * @file RequestQueue.h
 * @brief FIFO queue of pending requests owned by a load balancer.
 */
#ifndef REQUEST_QUEUE_H
#define REQUEST_QUEUE_H

#include <cstddef>
#include <queue>

#include "Request.h"

/**
 * @class RequestQueue
 * @brief A first-in, first-out queue of Request objects.
 *
 * This is a thin wrapper around @c std::queue that gives the load balancer a
 * small, intention-revealing interface and keeps the underlying container an
 * implementation detail.
 */
class RequestQueue {
public:
    /**
     * @brief Appends a request to the back of the queue.
     * @param request The request to enqueue (copied).
     */
    void push(const Request& request);

    /**
     * @brief Removes and returns the request at the front of the queue.
     * @pre The queue must not be empty.
     * @return The oldest pending request.
     */
    Request pop();

    /**
     * @brief Checks whether the queue holds no requests.
     * @return true if empty, false otherwise.
     */
    bool empty() const;

    /**
     * @brief Returns the number of pending requests.
     * @return Current queue length.
     */
    std::size_t size() const;

private:
    std::queue<Request> items_;  ///< Underlying FIFO storage.
};

#endif // REQUEST_QUEUE_H
