#pragma once

#include <string>

inline std::string make_msg(const std::string& type,
                            const std::string& name,
                            const std::string& ip,
                            const std::string& data = "") {
    if (data.empty()) {
        return type + " " + name + " " + ip;
    }
    return type + " " + name + " " + ip + " " + data;
}

bool send_udp(const std::string& ip, const std::string& message);
bool send_tcp(const std::string& ip, const std::string& message);
