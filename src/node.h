#pragma once
#include <atomic>
#include <thread>
#include <string>

class Node {
public:
    std::string name;
    std::string ip;

    std::atomic<bool> running{false};
    std::atomic<bool> attempt_join{false};
    std::atomic<bool> joined{false};

    int udp_sock{-1};
    int tcp_sock{-1};

    std::thread udp_thread;
    std::thread tcp_thread;
    std::thread join_thread;

    Node();
    ~Node();

    bool start();
    void stop();

    bool is_running() const { return running.load(); }
};
