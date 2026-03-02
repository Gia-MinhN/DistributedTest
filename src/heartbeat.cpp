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

        // -------------------------------------------------
        // Handle probe timeouts
        // -------------------------------------------------
        for (auto it = probes_.begin(); it != probes_.end(); ) {
            std::string target = it->first;
            Probe& p = it->second;

            if (now < p.deadline_ms) {
                ++it;
                continue;
            }

            if (p.phase == Phase::Direct) {
                // Escalate to indirect probe
                p.phase = Phase::Indirect;
                p.deadline_ms = now + INDIRECT_TIMEOUT_MS;

                // Send PING-REQ to k helpers
                std::vector<std::string> helpers = rr_peers_;
                std::shuffle(helpers.begin(), helpers.end(), rr_rng_);

                size_t sent = 0;
                for (const auto& helper : helpers) {
                    if (helper == target) continue;
                    if (sent >= FANOUT) break;

                    std::string helper_ip;
                    {
                        std::lock_guard<std::mutex> lk(node_->membership_mu);
                        auto it2 = node_->membership.find(helper);
                        if (it2 == node_->membership.end()) continue;
                        helper_ip = it2->second.ip;
                    }

                    std::string msg = make_msg("PING-REQ", *node_, target);
                    (void)send_udp(helper_ip, msg);
                    sent++;
                }

                ++it;
            }
            else {
                // Indirect failed, mark Suspect
                {
                    std::lock_guard<std::mutex> lk(node_->membership_mu);
                    auto it2 = node_->membership.find(target);
                    if (it2 != node_->membership.end() &&
                        it2->second.status == MemberStatus::Alive) {
                        it2->second.status = MemberStatus::Suspect;
                    }
                }

                it = probes_.erase(it);
            }
        }

        // -------------------------------------------------
        // Membership aging (Suspect -> Dead)
        // -------------------------------------------------
        std::vector<std::string> peer_names;

        {
            std::lock_guard<std::mutex> lk(node_->membership_mu);

            auto& me = node_->membership[node_->name];
            me.ip = node_->ip;
            me.status = MemberStatus::Alive;
            me.last_seen_ms = now;

            for (auto& [n, info] : node_->membership) {
                if (n == node_->name) continue;

                if (info.status == MemberStatus::Suspect) {
                    uint64_t age = now - info.last_seen_ms;
                    if (age > SUSPECT_MS) {
                        info.status = MemberStatus::Dead;
                    }
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

        // -------------------------------------------------
        // Start new probes
        // -------------------------------------------------
        for (size_t i = 0; i < FANOUT && !rr_peers_.empty(); i++) {
            if (rr_idx_ >= rr_peers_.size()) {
                rr_idx_ = 0;
                std::shuffle(rr_peers_.begin(), rr_peers_.end(), rr_rng_);
            }

            const std::string& target = rr_peers_[rr_idx_++];

            if (probes_.count(target)) continue;

            std::string target_ip;
            {
                std::lock_guard<std::mutex> lk(node_->membership_mu);
                auto it = node_->membership.find(target);
                if (it == node_->membership.end()) continue;
                target_ip = it->second.ip;
            }

            std::string piggy = build_piggy_data(*node_, target, PIGGY_K);
            std::string msg = make_msg("PING", *node_, piggy);
            (void)send_udp(target_ip, msg);

            probes_[target] = {
                Phase::Direct,
                now + PING_TIMEOUT_MS
            };
        }

        std::this_thread::sleep_for(milliseconds(TICK_MS));
    }
}