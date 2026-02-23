#pragma once

#include <cstddef>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

#include <netinet/in.h>

class Node;

struct UdpEvent {
    sockaddr_in from{};
    std::string payload;
};

class UdpQueue {
public:
    void start(Node& node);
    void stop();

    void enqueue(const sockaddr_in& from, const char* data, size_t len);

private:
    void worker_loop();
    void handle_datagram(const sockaddr_in& from, const std::string& payload);

private:
    Node* node_ = nullptr;
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<UdpEvent> q_;
    std::thread th_;
    bool running_ = false;
};