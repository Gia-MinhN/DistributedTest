#include "receiver.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <iostream>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "node.h"

static const uint16_t PORT = 9000;

void udp_receiver_loop(int sock, Node& node) {
    if (sock < 0) { perror("udp socket"); return; }

    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 50 * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("udp bind");
        return;
    }

    while (node.running.load()) {
        char buffer[2048];

        sockaddr_in from{};
        socklen_t fromlen = sizeof(from);

        ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromlen);
        if (n < 0) {
            if (!node.running.load()) break;

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            if (errno == EINTR) continue;
            if (errno == EBADF || errno == EINVAL) break;

            perror("udp recvfrom");
            continue;
        }

        node.udpq.enqueue(from, buffer, (size_t)n);
    }
}

void tcp_receiver_loop(int sock, Node& node) {
    if (sock < 0) { perror("tcp socket"); return; }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("tcp bind");
        return;
    }

    if (listen(sock, 5) < 0) {
        perror("tcp listen");
        return;
    }

    while (node.running.load()) {
        sockaddr_in peer{};
        socklen_t peerlen = sizeof(peer);

        int client_sock = accept(sock, (sockaddr*)&peer, &peerlen);
        if (client_sock < 0) {
            if (!node.running.load()) break;
            if (errno == EBADF || errno == EINVAL) break;
            if (errno == EINTR) continue;
            perror("tcp accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
        std::cout << "[TCP connected " << ip << ":" << ntohs(peer.sin_port) << "]\n";

        char buffer[1024];
        while (node.running.load()) {
            int n = recv(client_sock, buffer, sizeof(buffer), 0);
            if (n <= 0) break;

            std::cout << "[TCP] ";
            std::cout.write(buffer, n);
            std::cout << "\n" << std::flush;
        }

        close(client_sock);
        std::cout << "[TCP disconnected]\n";
    }
}
