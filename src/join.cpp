#include "join.h"
#include "node.h"
#include "sender.h"

#include <chrono>
#include <thread>

void attempt_join_loop(Node& node) {
    using namespace std::chrono_literals;

    while (node.running.load() && node.attempt_join.load() && !node.joined.load()) {
        for (const auto& seed_ip : node.seeds) {
            if (!node.running.load() || !node.attempt_join.load() || node.joined.load())
                break;

            std::string msg = make_msg("JOIN", node.name, node.ip, "");
            (void)send_udp(seed_ip, msg);
        }

        std::this_thread::sleep_for(750ms);
    }
}