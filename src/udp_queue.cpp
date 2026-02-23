#include "udp_queue.h"
#include "node.h"
#include "sender.h"
#include "string_util.h"
#include "time_util.h"

void UdpQueue::start(Node& n) {
    if (running) return;
    node = &n;
    running = true;
    th = std::thread(&UdpQueue::worker_loop, this);
}

void UdpQueue::stop() {
    if (!running) return;
    {
        std::lock_guard<std::mutex> lk(mu);
        running = false;
    }
    cv.notify_all();
    if (th.joinable()) th.join();

    std::lock_guard<std::mutex> lk(mu);
    q.clear();
    node = nullptr;
}

void UdpQueue::enqueue(const sockaddr_in& from, const char* data, size_t len) {
    UdpEvent ev;
    ev.from = from;
    ev.payload.assign(data, data + len);

    {
        std::lock_guard<std::mutex> lk(mu);
        // if (q_.size() > 10000) return;
        q.push_back(std::move(ev));
    }
    cv.notify_one();
}

void UdpQueue::worker_loop() {
    while (true) {
        UdpEvent ev;

        {
            std::unique_lock<std::mutex> lk(mu);
            cv.wait(lk, [&]{ return !running || !q.empty(); });

            if (!running && q.empty()) break;

            ev = std::move(q.front());
            q.pop_front();
        }

        if (node) {
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

    if (!name.empty() && !ip.empty()) {
        MemberInfo m;
        m.ip = ip;
        m.status = MemberStatus::Alive;
        m.last_seen_ms = now_ms();
        m.incarnation = 0;

        node->membership[name] = m;
    }

    if (type == "JOIN") {
        std::string reply = make_msg("WELCOME", node->name, node->ip);
        (void)send_udp(ip, reply);
        return;
    }

    if (type == "WELCOME") {
        node->joined.store(true);
        node->attempt_join.store(false);
        return;
    }

    if (type == "SEEDHELLO") {
        return;
    }

    // std::cout << "[UDP] " << type << " from " << name << " " << ip << " " << rest << "\n";
}