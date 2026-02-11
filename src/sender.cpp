#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static const uint16_t PORT = 9000;

bool parse_ipv4(const std::string& ip, in_addr& out) {
    return inet_pton(AF_INET, ip.c_str(), &out) == 1;
}

bool send_udp(const std::string& ip, const std::string& message) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("udp socket"); return false; }

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(PORT);
    if (!parse_ipv4(ip, dst.sin_addr)) {
        std::cerr << "Invalid IPv4 address: " << ip << "\n";
        close(sock);
        return false;
    }

    ssize_t n = sendto(sock, message.data(), message.size(), 0,
                       (sockaddr*)&dst, sizeof(dst));
    if (n < 0) { perror("udp sendto"); close(sock); return false; }

    close(sock);
    return true;
}

bool send_all(int sock, const char* data, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(sock, data + off, len - off, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        off += (size_t)n;
    }
    return true;
}

bool send_tcp(const std::string& ip, const std::string& message) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("tcp socket"); return false; }

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(PORT);
    if (!parse_ipv4(ip, dst.sin_addr)) {
        std::cerr << "Invalid IPv4 address: " << ip << "\n";
        close(sock);
        return false;
    }

    if (connect(sock, (sockaddr*)&dst, sizeof(dst)) < 0) {
        perror("tcp connect");
        close(sock);
        return false;
    }

    bool ok = send_all(sock, message.data(), message.size());
    if (!ok) perror("tcp send");

    close(sock);
    return ok;
}