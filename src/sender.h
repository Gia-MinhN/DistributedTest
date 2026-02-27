#pragma once

#include <string>

#include "node.h"

inline std::string make_msg(const std::string& type,
                            const Node& node,
                            const std::string& data = "") {
    const std::string inc = std::to_string(node.incarnation);

    if (data.empty()) {
        return type + " " + node.name + " " + node.ip + " " + inc;
    }
    return type + " " + node.name + " " + node.ip + " " + inc + " " + data;
}

bool send_udp(const std::string& ip, const std::string& message);
bool send_tcp(const std::string& ip, const std::string& message);