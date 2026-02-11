#include "netutil.h"

#include <limits.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <string>

std::string get_hostname() {
    char buf[HOST_NAME_MAX + 1];
    if (gethostname(buf, sizeof(buf)) == 0) {
        buf[sizeof(buf) - 1] = '\0';
        return std::string(buf);
    }
    return "unknown";
}

std::string detect_local_ip() {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return "";

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(8999);
    inet_pton(AF_INET, "8.8.8.8", &dst.sin_addr);

    if (connect(s, (sockaddr*)&dst, sizeof(dst)) < 0) {
        close(s);
        return "";
    }

    sockaddr_in local{};
    socklen_t len = sizeof(local);
    if (getsockname(s, (sockaddr*)&local, &len) < 0) {
        close(s);
        return "";
    }

    close(s);

    char buf[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf))) return "";
    return std::string(buf);
}