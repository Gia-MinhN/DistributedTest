#include "udp_queue.h"

#include <algorithm>
#include <cctype>
#include <random>
#include <string>
#include <vector>
#include <mutex>

#include "node.h"
#include "sender.h"
#include "string_util.h"
#include "time_util.h"
#include "membership_config.h"

static int status_rank(MemberStatus s) {
    switch (s) {
        case MemberStatus::Alive:   return 0;
        case MemberStatus::Suspect: return 1;
        case MemberStatus::Dead:    return 2;
        default:                    return 0;
    }
}

static char status_char(MemberStatus s) {
    switch (s) {
        case MemberStatus::Alive:   return 'A';
        case MemberStatus::Suspect: return 'S';
        case MemberStatus::Dead:    return 'D';
        default:                    return 'A';
    }
}

static MemberStatus status_from_char(char c) {
    if (c == 'A') return MemberStatus::Alive;
    if (c == 'S') return MemberStatus::Suspect;
    if (c == 'D') return MemberStatus::Dead;
    return MemberStatus::Alive;
}


// name@ip@inc@S@lastSeen
static std::string make_entry(const std::string& name, const MemberInfo& info) {
    std::string e;
    e.reserve(name.size() + info.ip.size() + 32);
    e += name;
    e.push_back('@');
    e += info.ip;
    e.push_back('@');
    e += std::to_string(info.incarnation);
    e.push_back('@');
    e.push_back(status_char(info.status));
    e.push_back('@');
    e += std::to_string(info.last_seen_ms);
    return e;
}

static std::string piggyback_csv_random_k(const Node& node,
                                         const std::string& exclude_name,
                                         size_t k) {
    std::vector<std::string> entries;
    entries.reserve(node.membership.size());

    for (const auto& [n, info] : node.membership) {
        if (n.empty() || info.ip.empty()) continue;
        if (n == exclude_name) continue;
        if (n == node.name) continue;
        entries.push_back(make_entry(n, info));
    }

    if (entries.empty() || k == 0) return "";

    static thread_local std::mt19937 rng(std::random_device{}());
    std::shuffle(entries.begin(), entries.end(), rng);
    if (entries.size() > k) entries.resize(k);

    std::string out;
    for (size_t i = 0; i < entries.size(); i++) {
        if (i) out.push_back(',');
        out += entries[i];
    }
    return out;
}

static void merge_member(Node& node,
                         const std::string& name,
                         const std::string& ip,
                         const std::string& inc_str,
                         MemberStatus st,
                         uint64_t last_seen,
                         bool direct) {
    MemberInfo& cur = node.membership[name];

    uint64_t inc = 0;
    try { inc = (uint64_t)std::stoull(inc_str); } catch (...) { inc = 0; }

    if (!ip.empty()) cur.ip = ip;

    if (inc > cur.incarnation) {
        cur.incarnation = inc;
        cur.status = st;
        if (direct) cur.last_seen_ms = last_seen;
        return;
    }

    if (inc < cur.incarnation) {
        return;
    }

    if (direct) {
        cur.status = MemberStatus::Alive;
        cur.last_seen_ms = last_seen;
        return;
    }

    if (st == MemberStatus::Alive) return;

    if (status_rank(st) > status_rank(cur.status)) {
        cur.status = st;
    }
}

static void apply_piggyback(Node& node, const std::string& csv) {
    if (csv.empty()) return;

    size_t start = 0;
    while (start < csv.size()) {
        size_t comma = csv.find(',', start);
        if (comma == std::string::npos) comma = csv.size();

        std::string entry = csv.substr(start, comma - start);

        size_t a = entry.find('@');
        size_t b = (a == std::string::npos) ? std::string::npos : entry.find('@', a + 1);
        size_t c = (b == std::string::npos) ? std::string::npos : entry.find('@', b + 1);
        size_t d = (c == std::string::npos) ? std::string::npos : entry.find('@', c + 1);

        if (a != std::string::npos && b != std::string::npos &&
            c != std::string::npos && d != std::string::npos) {

            std::string n = entry.substr(0, a);
            std::string ip = entry.substr(a + 1, b - (a + 1));
            std::string inc_s = entry.substr(b + 1, c - (b + 1));
            char st_c = entry[c + 1];

            if (n == node.name) { start = comma + 1; continue; }

            if (!n.empty() && !ip.empty()) {
                merge_member(node, n, ip, inc_s, status_from_char(st_c), 0, false);
            }
        }

        start = comma + 1;
    }
}

static std::string build_piggy_data(Node& node,
                                    const std::string& exclude_name,
                                    size_t k) {
    std::lock_guard<std::mutex> lk(node.membership_mu);
    return piggyback_csv_random_k(node, exclude_name, k);
}

void UdpQueue::start(Node& node) {
    if (running_) return;
    node_ = &node;
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

        if (node_) handle_datagram(ev.from, ev.payload);
    }
}

void UdpQueue::handle_datagram(const sockaddr_in& from, const std::string& payload) {
    (void)from;
    if (!node_) return;

    std::string msg = trim(payload);
    if (msg.empty()) return;

    std::string type, sender_name, sender_ip, sender_inc;
    size_t pos = 0;

    // parse payload
    next_token(msg, pos, type);
    next_token(msg, pos, sender_name);
    next_token(msg, pos, sender_ip);
    next_token(msg, pos, sender_inc);

    std::string data = rest_of_line(msg, pos);

    const uint64_t now = now_ms();
    {
        std::lock_guard<std::mutex> lk(node_->membership_mu);
        apply_piggyback(*node_, data);
        if (!sender_name.empty() && !sender_ip.empty()) {
            merge_member(*node_, sender_name, sender_ip, sender_inc, MemberStatus::Alive, now, true);
        }
    }

    if (type == "JOIN") {
        std::string piggy_msg = build_piggy_data(*node_, sender_name, PIGGY_K);
        std::string reply = make_msg("WELCOME", *node_, piggy_msg);
        send_udp(sender_ip, reply);
        return;
    }

    if (type == "WELCOME") {
        node_->joined.store(true);
        node_->attempt_join.store(false);
        return;
    }

    if (type == "PING") {
        std::string piggy_msg = build_piggy_data(*node_, sender_name, PIGGY_K);
        std::string reply = make_msg("ACK", *node_, piggy_msg);
        send_udp(sender_ip, reply);
        return;
    }

    if (type == "ACK") {
        return;
    }
}