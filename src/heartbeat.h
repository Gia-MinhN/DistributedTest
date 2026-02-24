#pragma once

#include <thread>

class Node;

class Heartbeat {
public:
    void start(Node& node);
    void stop();

private:
    void loop();

    Node* node_ = nullptr;
    std::thread th_;
};