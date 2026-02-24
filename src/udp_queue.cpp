#include "udp_queue.h"

#include <iostream>
#include <random>
#include <algorithm>

#include "node.h"
#include "sender.h"
#include "string_util.h"
#include "time_util.h"

static std::string serialize_members_csv_random_k(const Node& node,
                                                  const std::string& joiner_name,
                                                  size_t k) {
    std::vector<std::pair<std::string, std::string>> candidates;
    candidates.reserve(node.membership.size());

    for (const auto& [n, info] : node.membership) {
        if (n.empty() || info.ip.empty()) continue;
        if (n == joiner_name) continue;
        if (n == node.name) continue;
        candidates.push_back({n, info.ip});
    }

    if (candidates.empty() || k == 0) return "";

    std::mt19937 rng(std::random_device{}());
    std::shuffle(candidates.begin(), candidates.end(), rng);

    if (candidates.size() > k) candidates.resize(k);

    std::string out;
    bool first = true;
    for (const auto& [n, ip] : candidates) {
        if (!first) out.push_back(',');
        first = false;
        out += n;
        out.push_back('@');
        out += ip;
    }
    return out;
}

static void apply_welcome_members(Node& node, const std::string& members_csv) {
    size_t start = 0;
    while (start < members_csv.size()) {
        size_t comma = members_csv.find(',', start);
        if (comma == std::string::npos) comma = members_csv.size();

        std::string entry = members_csv.substr(start, comma - start);
        size_t at = entry.find('@');
        if (at != std::string::npos) {
            std::string n = entry.substr(0, at);
            std::string ip = entry.substr(at + 1);

            if (!n.empty() && !ip.empty()) {
                MemberInfo m;
                m.ip = ip;
                m.status = MemberStatus::Alive;
                m.last_seen_ms = now_ms();
                m.incarnation = 0;
                node.membership[n] = m;
            }
        }

        start = comma + 1;
    }
}

void UdpQueue::start(Node& n) {
    if (running_) return;
    node_ = &n;
    running_ = true;
    th_ = std::thread(&UdpQueue::worker_loop, this);
}

void UdpQueue::stop() {
    if (!running_) return;
    {
        std::lock_guard<std::mutex> lk(mu_);
        running_ = false;
    }
    cv_.notify_all();
    if (th_.joinable()) th_.join();

    std::lock_guard<std::mutex> lk(mu_);
    q_.clear();
    node_ = nullptr;
}

void UdpQueue::enqueue(const sockaddr_in& from, const char* data, size_t len) {
    UdpEvent ev;
    ev.from = from;
    ev.payload.assign(data, data + len);

    {
        std::lock_guard<std::mutex> lk(mu_);
        q_.push_back(std::move(ev));
    }
    cv_.notify_one();
}

void UdpQueue::worker_loop() {
    while (true) {
        UdpEvent ev;

        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&]{ return !running_ || !q_.empty(); });

            if (!running_ && q_.empty()) break;

            ev = std::move(q_.front());
            q_.pop_front();
        }

        if (node_) {
            handle_datagram(ev.from, ev.payload);
        }
    }
}

void UdpQueue::handle_datagram(const sockaddr_in& from, const std::string& payload) {
    (void)from;

    std::string msg = trim(payload);
    if (msg.empty()) return;

    std::string type, name, ip;
    size_t pos = 0;

    next_token(msg, pos, type);
    next_token(msg, pos, name);
    next_token(msg, pos, ip);

    std::string data = rest_of_line(msg, pos);

    // std::cout << "[UDP] " << type << " from " << name << " " << ip << " " << data << "\n";

    if (!name.empty() && !ip.empty()) {
        MemberInfo m;
        m.ip = ip;
        m.status = MemberStatus::Alive;
        m.last_seen_ms = now_ms();
        m.incarnation = 0;

        node_->membership[name] = m;
    }

    if (type == "JOIN") {
        constexpr size_t K = 3;

        std::string members = serialize_members_csv_random_k(*node_, name, K);
        std::string reply = make_msg("WELCOME", node_->name, node_->ip, members);

        send_udp(ip, reply);
        return;
    }

    if (type == "WELCOME") {
        node_->joined.store(true);
        node_->attempt_join.store(false);

        apply_welcome_members(*node_, data);
        return;
    }

    if (type == "PING") {
        std::string reply = make_msg("ACK", node_->name, node_->ip, "");
        send_udp(ip, reply);
        return;
    }

    if (type == "ACK") {
        return;
    }
}