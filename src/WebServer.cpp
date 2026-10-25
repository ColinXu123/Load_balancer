/**
 * @file WebServer.cpp
 * @brief Implementation of the WebServer class.
 */
#include "WebServer.h"

WebServer::WebServer(int id, long createdAt)
    : id_(id), createdAt_(createdAt), remaining_(0),
      busy_(false), draining_(false), processed_(0) {}

void WebServer::assign(const Request& request) {
    current_   = request;
    // A request always occupies the server for at least one cycle.
    remaining_ = request.time > 0 ? request.time : 1;
    busy_      = true;
}

bool WebServer::tick() {
    if (!busy_) {
        return false;
    }
    --remaining_;
    if (remaining_ <= 0) {
        busy_ = false;
        remaining_ = 0;
        ++processed_;
        return true;
    }
    return false;
}

bool WebServer::isBusy() const { return busy_; }

bool WebServer::canAccept() const { return !busy_ && !draining_; }

bool WebServer::isDraining() const { return draining_; }

void WebServer::startDraining() { draining_ = true; }

const Request& WebServer::current() const { return current_; }

int WebServer::remaining() const { return remaining_; }

int WebServer::id() const { return id_; }

long WebServer::createdAt() const { return createdAt_; }

long WebServer::processedCount() const { return processed_; }
