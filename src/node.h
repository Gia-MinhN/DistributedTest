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

    uint64_t suspect_since_ms = 0;
};


class Node {
public:
    std::string name;
    std::string ip;

    uint64_t incarnation = 0;

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

    std::mutex cli_ping_mu_;
    std::condition_variable cli_ping_cv_;
    std::unordered_map<std::string, bool> cli_ping_results_;

    Node(std::vector<std::string> seeds);
    ~Node();

    bool start();
    void stop();

    uint64_t set_incarnation(uint64_t new_inc);

    bool ping_test(std::string arg);
};
