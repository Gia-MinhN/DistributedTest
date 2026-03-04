#pragma once

#include <thread>
#include <vector>
#include <random>
#include <string>
#include <unordered_map>
#include <mutex>

class Node;

enum class Phase {
    None,
    Direct,
    Indirect
};

struct Probe {
    Phase phase = Phase::None;
    uint64_t deadline_ms = 0;
};

class Heartbeat {
public:
    void start(Node& node);
    void stop();

    std::unordered_map<std::string, Probe> probes_;
    std::mutex probes_mu_;

    void clear_probe(const std::string& target);

private:
    void loop();

    Node* node_ = nullptr;
    std::thread th_;

    std::vector<std::string> rr_peers_;
    size_t rr_idx_ = 0;
    std::mt19937 rr_rng_{std::random_device{}()};
};