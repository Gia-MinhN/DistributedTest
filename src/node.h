#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

#include "udp_queue.h"
#include "heartbeat.h"

enum class MemberStatus {
    Alive,
    Suspect,
    Dead
};

struct MemberInfo {
    std::string ip;

    MemberStatus status = MemberStatus::Alive;

    uint64_t last_seen_ms = 0;
    uint64_t incarnation = 0;
};


class Node {
public:
    std::string name;
    std::string ip;

    std::vector<std::string> seeds;
    bool is_seed;

    std::atomic<bool> running{false};
    std::atomic<bool> attempt_join{false};
    std::atomic<bool> joined{false};

    int udp_sock{-1};
    int tcp_sock{-1};

    UdpQueue udpq;
    Heartbeat hb;

    std::thread udp_thread;
    std::thread tcp_thread;
    std::thread join_thread;

    mutable std::mutex membership_mu;
    std::map<std::string, MemberInfo> membership;

    Node(std::vector<std::string> seeds);
    ~Node();

    bool start();
    void stop();
};
