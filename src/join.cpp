#include "join.h"
#include "sender.h"

#include <chrono>
#include <iostream>
#include <thread>

std::vector<std::string> introducer_ips = {
    "10.221.62.250",
    "10.221.62.146"
};

void attempt_join_loop(std::atomic<bool>& running,
                       std::atomic<bool>& attempt_join,
                       std::atomic<bool>& joined,
                       const std::string& name,
                       const std::string& ip) {
    
    using namespace std::chrono_literals;

    const auto retry_period = 2000ms;
    
    int retry_count = 0;
    while (running.load() && attempt_join.load() && !joined.load()) {
        retry_count += 1;
        // std::cout << "Attempting join (" << retry_count << ")\n";
        for (const auto& intro : introducer_ips) {
            if (!running.load() || !attempt_join.load() || joined.load()) break;

            std::string msg = "JOIN " + name + " " + ip;

            if (!send_udp(intro, msg)) {
                continue;
            }
        }

        std::this_thread::sleep_for(retry_period);
    }

    if (joined.load()) {
        std::cout << "Join succeeded (ACK received).\n";
    } else if (!attempt_join.load()) {
        std::cout << "Join cancelled.\n";
    }
}
