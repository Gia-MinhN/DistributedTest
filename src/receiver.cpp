#include "receiver.h"
#include "node.h"
#include "sender.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>

static const uint16_t PORT = 9000;

using namespace std;

static string trim(string s) {
    size_t a = 0;
    while (a < s.size() && isspace((unsigned char)s[a])) a++;
    size_t b = s.size();
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

static bool next_token(const string& s, size_t& pos, string& out) {
    while (pos < s.size() && isspace((unsigned char)s[pos])) pos++;
    if (pos >= s.size()) return false;
    size_t start = pos;
    while (pos < s.size() && !isspace((unsigned char)s[pos])) pos++;
    out = s.substr(start, pos - start);
    return true;
}

void udp_parser(Node& node, const sockaddr_in& from, const char* data, size_t len) {
    (void)from;

    string msg(data, data + len);
    msg = trim(msg);
    if (msg.empty()) return;

    string type, name, ip, rest;
    size_t pos = 0;

    next_token(msg, pos, type);
    next_token(msg, pos, name);
    next_token(msg, pos, ip);

    while (pos < msg.size() && isspace((unsigned char)msg[pos])) pos++;
    if (pos < msg.size()) rest = msg.substr(pos);

    // cout << "TYPE=" << (type.empty() ? "<none>" : type)
    //      << " NAME=" << (name.empty() ? "<none>" : name)
    //      << " IP="   << (ip.empty() ? "<none>" : ip)
    //      << " DATA=" << (rest.empty() ? "<none>" : rest)
    //      << "\n";

    if (type == "JOIN") {
        std::string msg = make_msg("WELCOME", node.name, node.ip);
        send_udp(ip, msg);
    }

    if (type == "WELCOME") {
        node.joined.store(true);
        node.attempt_join.store(false);
    }
}

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

        udp_parser(node, from, buffer, (size_t)n);
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
