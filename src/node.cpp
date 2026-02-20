#include "node.h"

#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <chrono>

#include "netutil.h"
#include "receiver.h"
#include "sender.h"
#include "join.h" 

static uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

Node::Node() {
    name = get_hostname();
    ip = detect_local_ip();

    is_seed = (name == "node0" || name == "node9");
}

Node::~Node() {
    stop();
}

bool Node::start() {
    if (running.load()) return true;

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

    udp_thread = std::thread(udp_receiver_loop, udp_sock, std::ref(*this));
    tcp_thread = std::thread(tcp_receiver_loop, tcp_sock, std::ref(*this));

    if (is_seed) {
        std::cout << "This is a seed node, no join attempt needed.\n";
        attempt_join.store(false);
        joined.store(true); 
    } else {
        std::cout << "Join attempts will be made in the background.\n";
        attempt_join.store(true);
        if (!join_thread.joinable()) {
            join_thread = std::thread(
                attempt_join_loop,
                std::ref(running),
                std::ref(attempt_join),
                std::ref(joined),
                std::cref(name),
                std::cref(ip)
            );
        }
    }

    MemberInfo me;
    me.ip = ip;
    me.status = MemberStatus::Alive;
    me.last_seen_ms = now_ms();
    me.incarnation = 0;

    membership.insert({name, me});

    std::cout << "This node (" << name << ", " << ip << ") is now running.\n";
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

    if (udp_thread.joinable()) udp_thread.join();
    if (tcp_thread.joinable()) tcp_thread.join();
    if (join_thread.joinable()) join_thread.join();

    udp_sock = -1;
    tcp_sock = -1;

    membership.clear();

    std::cout << "This node (" << name << ") is no longer running.\n";
}
