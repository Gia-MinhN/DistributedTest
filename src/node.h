#pragma once
#include <atomic>
#include <thread>
#include <string>

class Node {
public:
    std::string alias;
    std::string ip;

    Node(std::string a, std::string i);
    ~Node();

    bool start();
    void stop();

    bool join(const std::string& seed_ip, const std::string& msg);

    bool is_running() const { return running.load(); }

private:
    std::atomic<bool> running{false};

    int udp_sock{-1};
    int tcp_sock{-1};

    std::thread udp_thread;
    std::thread tcp_thread;
};
