#include "heartbeat.h"

#include <chrono>
#include <random>
#include <string>
#include <vector>

#include "node.h"
#include "sender.h"
#include "time_util.h"

static const uint64_t SUSPECT_MS = 3000;
static const uint64_t DEAD_MS    = 8000;
static const uint64_t TICK_MS    = 1000;

static std::string pick_random(const std::vector<std::string>& v) {
    if (v.empty()) return "";
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, v.size() - 1);
    return v[dist(rng)];
}

void Heartbeat::start(Node& node) {
    if (th_.joinable()) return;
    node_ = &node;
    th_ = std::thread(&Heartbeat::loop, this);
}

void Heartbeat::stop() {
    if (th_.joinable()) th_.join();
    node_ = nullptr;
}

void Heartbeat::loop() {
    using namespace std::chrono;

    while (node_ && node_->running.load()) {
        const uint64_t now = now_ms();

        std::vector<std::string> peer_ips;

        {
            std::lock_guard<std::mutex> lk(node_->membership_mu);

            auto& me = node_->membership[node_->name];
            me.ip = node_->ip;
            me.status = MemberStatus::Alive;
            me.last_seen_ms = now;

            peer_ips.reserve(node_->membership.size());
            for (auto& [n, info] : node_->membership) {
                if (n == node_->name) continue;

                const uint64_t age =
                    (info.last_seen_ms == 0 || now < info.last_seen_ms) ? 0 : (now - info.last_seen_ms);

                if (info.status != MemberStatus::Dead && age > DEAD_MS) {
                    info.status = MemberStatus::Dead;
                } else if (info.status == MemberStatus::Alive && age > SUSPECT_MS) {
                    info.status = MemberStatus::Suspect;
                }

                if (info.status != MemberStatus::Dead && !info.ip.empty()) {
                    peer_ips.push_back(info.ip);
                }
            }
        }

        std::string peer_ip = pick_random(peer_ips);
        if (!peer_ip.empty()) {
            std::string msg = make_msg("PING", node_->name, node_->ip);
            send_udp(peer_ip, msg);
        }

        std::this_thread::sleep_for(milliseconds(TICK_MS));
    }
}