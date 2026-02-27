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

static std::string build_piggy_data(Node& node,
                                    const std::string& exclude_name,
                                    size_t k) {
    std::vector<std::string> entries;
    entries.reserve(node.membership.size());

    for (const auto& [n, info] : node.membership) {
        if (n.empty() || info.ip.empty()) continue;
        if (n == node.name) continue;
        if (n == exclude_name) continue;

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

    if (entries.empty() || k == 0) return "";

    static thread_local std::mt19937 rng(std::random_device{}());
    std::shuffle(entries.begin(), entries.end(), rng);
    if (entries.size() > k) entries.resize(k);

    std::string csv;
    for (size_t i = 0; i < entries.size(); i++) {
        if (i) csv.push_back(',');
        csv += entries[i];
    }
    return csv;
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

        std::vector<std::string> peer_names;
        {
            std::lock_guard<std::mutex> lk(node_->membership_mu);

            auto& me = node_->membership[node_->name];
            me.ip = node_->ip;
            me.status = MemberStatus::Alive;
            me.last_seen_ms = now;

            peer_names.reserve(node_->membership.size());

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
                    peer_names.push_back(n);
                }
            }
        }

        std::sort(peer_names.begin(), peer_names.end());
        peer_names.erase(std::unique(peer_names.begin(), peer_names.end()), peer_names.end());

        if (peer_names != rr_peers_) {
            rr_peers_ = peer_names;
            rr_idx_ = 0;
            std::shuffle(rr_peers_.begin(), rr_peers_.end(), rr_rng_);
        }

        std::vector<std::string> targets;
        targets.reserve(FANOUT);

        for (size_t i = 0; i < FANOUT && !rr_peers_.empty(); i++) {
            if (rr_idx_ >= rr_peers_.size()) {
                rr_idx_ = 0;
                std::shuffle(rr_peers_.begin(), rr_peers_.end(), rr_rng_);
            }
            targets.push_back(rr_peers_[rr_idx_++]);
        }

        for (const auto& target_name : targets) {
            std::string target_ip;
            std::string piggy;

            {
                std::lock_guard<std::mutex> lk(node_->membership_mu);

                auto it = node_->membership.find(target_name);
                if (it == node_->membership.end()) continue;
                if (it->second.status == MemberStatus::Dead) continue;

                target_ip = it->second.ip;
                if (target_ip.empty()) continue;

                piggy = build_piggy_data(*node_, target_name, PIGGY_K);
            }

            std::string msg = make_msg("PING", *node_, piggy);
            (void)send_udp(target_ip, msg);
        }

        std::this_thread::sleep_for(milliseconds(TICK_MS));
    }
}