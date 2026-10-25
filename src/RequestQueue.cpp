/**
 * @file RequestQueue.cpp
 * @brief Implementation of the RequestQueue class.
 */
#include "RequestQueue.h"

void RequestQueue::push(const Request& request) {
    items_.push(request);
}

Request RequestQueue::pop() {
    Request front = items_.front();
    items_.pop();
    return front;
}

bool RequestQueue::empty() const {
    return items_.empty();
}

std::size_t RequestQueue::size() const {
    return items_.size();
}
