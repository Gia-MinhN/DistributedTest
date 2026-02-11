#pragma once

#include <string>

bool send_udp(const std::string& ip, const std::string& message);
bool send_tcp(const std::string& ip, const std::string& message);
