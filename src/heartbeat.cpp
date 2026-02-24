#include "heartbeat.h"

#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>

#include "node.h"
#include "sender.h"
#include "time_util.h"
#include "membership_config.h"

static std::string build_piggy_data(Node& node, size_t k) {
    uint64_t self_inc = 0;
    auto it = node.membership.find(node.name);
    if (it != node.membership.end()) self_inc = it->second.incarnation;

    // name@ip@inc@S@lastSeen,...
    std::vector<std::string> entries;
    entries.reserve(node.membership.size());

    for (const auto& [n, info] : node.membership) {
        if (n.empty() || info.ip.empty()) continue;
        if (n == node.name) continue;

        std::string e;
        e.reserve(n.size() + info.ip.size() + 40);
        e += n;
        e.push_back('@');
        e += info.ip;
        e.push_back('@');
        e += std::to_string(info.incarnation);
        e.push_back('@');
        e.push_back(info.status == MemberStatus::Alive ? 'A' :
                    info.status == MemberStatus::Suspect ? 'S' : 'D');
        e.push_back('@');
        e += std::to_string(info.last_seen_ms);

        entries.push_back(std::move(e));
    }

    if (!entries.empty() && k > 0) {
        static thread_local std::mt19937 rng(std::random_device{}());
        std::shuffle(entries.begin(), entries.end(), rng);
        if (entries.size() > k) entries.resize(k);
    } else {
        entries.clear();
    }

    std::string csv;
    for (size_t i = 0; i < entries.size(); i++) {
        if (i) csv.push_back(',');
        csv += entries[i];
    }

    std::string out = "inc=" + std::to_string(self_inc);
    if (!csv.empty()) out += " " + csv;
    return out;
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
        std::string piggy_data;

        {
            std::lock_guard<std::mutex> lk(node_->membership_mu);

            // refresh self
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

            piggy_data = build_piggy_data(*node_, PIGGY_K);
        }

        std::sort(peer_ips.begin(), peer_ips.end());
        peer_ips.erase(std::unique(peer_ips.begin(), peer_ips.end()), peer_ips.end());

        if (peer_ips != rr_peers_) {
            rr_peers_ = peer_ips;
            rr_idx_ = 0;
            std::shuffle(rr_peers_.begin(), rr_peers_.end(), rr_rng_);
        }

        std::vector<std::string> to_ping;
        to_ping.reserve(FANOUT);

        for (size_t i = 0; i < FANOUT && !rr_peers_.empty(); i++) {
            if (rr_idx_ >= rr_peers_.size()) {
                rr_idx_ = 0;
                std::shuffle(rr_peers_.begin(), rr_peers_.end(), rr_rng_);
            }
            to_ping.push_back(rr_peers_[rr_idx_++]);
        }

        for (const auto& peer_ip : to_ping) {
            std::string msg = make_msg("PING", node_->name, node_->ip, piggy_data);
            (void)send_udp(peer_ip, msg);
        }

        std::this_thread::sleep_for(milliseconds(TICK_MS));
    }
}