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

void Heartbeat::clear_probe(const std::string& target) {
    std::lock_guard<std::mutex> lk(probes_mu_);
    probes_.erase(target);
}

void Heartbeat::loop() {
    using namespace std::chrono;

    while (node_ && node_->running.load()) {

        const uint64_t now = now_ms();
        
        // timeouts
        std::vector<std::string> escalate_to_indirect;
        std::vector<std::string> escalate_to_suspect;

        {
            std::lock_guard<std::mutex> lk(probes_mu_);

            for (auto it = probes_.begin(); it != probes_.end(); ) {

                if (now < it->second.deadline_ms) {
                    ++it;
                    continue;
                }

                if (it->second.phase == Phase::Direct) {
                    escalate_to_indirect.push_back(it->first);

                    it->second.phase = Phase::Indirect;
                    it->second.deadline_ms = now + INDIRECT_TIMEOUT_MS;
                    ++it;
                }
                else {
                    escalate_to_suspect.push_back(it->first);
                    it = probes_.erase(it);
                }
            }
        }

        // if no ack, ping-req
        for (const auto& target : escalate_to_indirect) {

            std::string target_ip;

            {
                std::lock_guard<std::mutex> lk(node_->membership_mu);
                auto mit = node_->membership.find(target);
                if (mit != node_->membership.end())
                    target_ip = mit->second.ip;
            }

            if (target_ip.empty())
                continue;

            std::vector<std::string> helpers = rr_peers_;
            std::shuffle(helpers.begin(), helpers.end(), rr_rng_);

            size_t sent = 0;

            for (const auto& helper : helpers) {
                if (helper == target) continue;
                if (sent >= FANOUT) break;

                std::string helper_ip;

                {
                    std::lock_guard<std::mutex> lk(node_->membership_mu);
                    auto mit = node_->membership.find(helper);
                    if (mit != node_->membership.end())
                        helper_ip = mit->second.ip;
                }

                if (helper_ip.empty())
                    continue;

                std::string target_info = target + "@" + target_ip;
                std::string msg = make_msg("PING-REQ", *node_, target_info);

                send_udp(helper_ip, msg);
                sent++;
            }
        }

        // if no ack-req, mark as suspect
        if (!escalate_to_suspect.empty()) {

            std::lock_guard<std::mutex> lk(node_->membership_mu);

            for (const auto& target : escalate_to_suspect) {

                auto mit = node_->membership.find(target);
                if (mit == node_->membership.end())
                    continue;

                if (mit->second.status == MemberStatus::Alive) {
                    mit->second.status = MemberStatus::Suspect;
                    mit->second.suspect_since_ms = now;
                }
            }
        }

        // membership aging
        std::vector<std::string> peer_names;

        {
            std::lock_guard<std::mutex> lk(node_->membership_mu);

            auto& me = node_->membership[node_->name];
            me.status = MemberStatus::Alive;
            me.last_seen_ms = now;

            for (auto& [name, info] : node_->membership) {

                if (name == node_->name)
                    continue;

                if (info.status == MemberStatus::Suspect &&
                    now - info.suspect_since_ms > SUSPECT_MS) {
                    info.status = MemberStatus::Dead;
                }

                if (info.status != MemberStatus::Dead &&
                    !info.ip.empty()) {
                    peer_names.push_back(name);
                }
            }
        }

        // shuffled round robin
        std::sort(peer_names.begin(), peer_names.end());
        peer_names.erase(std::unique(peer_names.begin(), peer_names.end()),
                         peer_names.end());

        if (peer_names != rr_peers_) {
            rr_peers_ = peer_names;
            rr_idx_ = 0;
            std::shuffle(rr_peers_.begin(), rr_peers_.end(), rr_rng_);
        }

        // one direct ping
        if (!rr_peers_.empty()) {

            if (rr_idx_ >= rr_peers_.size()) {
                rr_idx_ = 0;
                std::shuffle(rr_peers_.begin(), rr_peers_.end(), rr_rng_);
            }

            const std::string target = rr_peers_[rr_idx_++];

            bool already_probing = false;

            {
                std::lock_guard<std::mutex> lk(probes_mu_);
                already_probing = probes_.count(target);
            }

            if (!already_probing) {

                std::string target_ip;

                {
                    std::lock_guard<std::mutex> lk(node_->membership_mu);
                    auto mit = node_->membership.find(target);
                    if (mit != node_->membership.end())
                        target_ip = mit->second.ip;
                }

                if (!target_ip.empty()) {

                    std::string piggy = build_piggy_data(*node_, target, PIGGY_K);

                    std::string msg = make_msg("PING", *node_, piggy);
                    send_udp(target_ip, msg);

                    {
                        std::lock_guard<std::mutex> lk(probes_mu_);
                        probes_[target] = {
                            Phase::Direct,
                            now + PING_TIMEOUT_MS
                        };
                    }
                }
            }
        }

        // sleep remainder of tick
        const uint64_t time_taken = now_ms() - now;

        if (time_taken < TICK_MS)
            std::this_thread::sleep_for(milliseconds(TICK_MS - time_taken));
    }
}