#pragma once

#include <thread>
#include <vector>
#include <random>
#include <string>

class Node;

class Heartbeat {
public:
    void start(Node& node);
    void stop();

private:
    void loop();

    Node* node_ = nullptr;
    std::thread th_;

    std::vector<std::string> rr_peers_;
    size_t rr_idx_ = 0;
    std::mt19937 rr_rng_{std::random_device{}()};
};