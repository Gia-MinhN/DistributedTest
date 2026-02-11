#include "node.h"
#include "netutil.h"

#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "receiver.h"
#include "sender.h"


Node::Node() {
    name = get_hostname();
    ip = detect_local_ip();
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

    udp_thread = std::thread(udp_receiver_loop, udp_sock, std::ref(running));
    tcp_thread = std::thread(tcp_receiver_loop, tcp_sock, std::ref(running));

    return true;
}

void Node::stop() {
    if (!running.exchange(false)) {
        return;
    }

    if (tcp_sock >= 0) {
        shutdown(tcp_sock, SHUT_RDWR);
    }

    if (udp_sock >= 0) close(udp_sock);
    if (tcp_sock >= 0) close(tcp_sock);

    if (udp_thread.joinable()) udp_thread.join();
    if (tcp_thread.joinable()) tcp_thread.join();

    udp_sock = -1;
    tcp_sock = -1;
}

bool Node::join(const std::string& seed_ip, const std::string& msg) {
    return send_udp(seed_ip, msg);
}
