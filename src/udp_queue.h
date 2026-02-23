#pragma once
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <string>
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
    Node* node = nullptr;
    std::mutex mu;
    std::condition_variable cv;
    std::deque<UdpEvent> q;
    std::thread th;
    bool running = false;
};