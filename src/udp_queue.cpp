#include "udp_queue.h"

#include <iostream>

#include "node.h"
#include "sender.h"
#include "string_util.h"
#include "time_util.h"

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
        std::string reply = make_msg("WELCOME", node_->name, node_->ip);
        (void)send_udp(ip, reply);
        return;
    }

    if (type == "WELCOME") {
        node_->joined.store(true);
        node_->attempt_join.store(false);
        return;
    }

    if (type == "SEEDHELLO") {
        return;
    }
}