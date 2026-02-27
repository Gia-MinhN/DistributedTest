#include "node.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "join.h"
#include "net_util.h"
#include "receiver.h"
#include "sender.h"
#include "time_util.h"

static uint64_t read_or_init_incarnation(const std::string& path) {
    std::ifstream in(path);
    if (in) {
        uint64_t v = 0;
        if (in >> v) return v;
    }

    std::ofstream out(path, std::ios::trunc);
    if (out) out << -1 << "\n";
    return 0;
}

static void write_incarnation(const std::string& path, uint64_t inc) {
    std::ofstream out(path, std::ios::trunc);
    if (out) out << inc << "\n";
}

Node::Node(std::vector<std::string> s) : seeds(std::move(s)) {
    name = get_hostname();
    ip = detect_local_ip();

    if(std::find(seeds.begin(), seeds.end(), ip) != seeds.end()) {
        is_seed = true;
    } else {
        is_seed = false;
    }
}

Node::~Node() {
    stop();
}

bool Node::start() {
    if (running.load()) return true;

    incarnation = read_or_init_incarnation("incarnation");
    incarnation += 1;
    write_incarnation("incarnation", incarnation);

    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        perror("udp socket");
        return false;
    }

    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) {
        perror("tcp socket");
        close(udp_sock);
        udp_sock = -1;
        return false;
    }

    running.store(true);
    attempt_join.store(true);
    joined.store(false);

    udpq.start(*this);
    hb.start(*this);

    udp_thread = std::thread(udp_receiver_loop, udp_sock, std::ref(*this));
    tcp_thread = std::thread(tcp_receiver_loop, tcp_sock, std::ref(*this));

    if (is_seed) {
        std::cout << "Seed node: syncing with other seeds (best-effort).\n";
        attempt_join.store(false);
        joined.store(true); 

        for (const auto& seed_ip : seeds) {
            if (seed_ip == ip) continue;

            std::string msg = make_msg("JOIN", name, ip);
            send_udp(seed_ip, msg);
        }
    } else {
        std::cout << "Non-seed node: attempting to join via seeds (retrying in background).\n";
        attempt_join.store(true);
        if (!join_thread.joinable()) {
            join_thread = std::thread(attempt_join_loop, std::ref(*this));
        }
    }

    MemberInfo me;
    me.ip = ip;
    me.status = MemberStatus::Alive;
    me.last_seen_ms = now_ms();
    me.incarnation = 0;

    {
        std::lock_guard<std::mutex> lk(membership_mu);
        membership[name] = me;
    }

    std::cout << "Node [" << name << "@" << ip << "] started.\n";
    return true;
}

void Node::stop() {
    if (!running.exchange(false)) return;

    attempt_join.store(false);
    joined.store(false);

    if (tcp_sock >= 0) {
        shutdown(tcp_sock, SHUT_RDWR);
    }

    if (udp_sock >= 0) close(udp_sock);
    if (tcp_sock >= 0) close(tcp_sock);

    udpq.stop();
    hb.stop();

    if (udp_thread.joinable()) udp_thread.join();
    if (tcp_thread.joinable()) tcp_thread.join();
    if (join_thread.joinable()) join_thread.join();

    udp_sock = -1;
    tcp_sock = -1;

    {
        std::lock_guard<std::mutex> lk(membership_mu);
        membership.clear();
    }

    std::cout << "Node [" << name << "@" << ip << "] stopped.\n";
}
