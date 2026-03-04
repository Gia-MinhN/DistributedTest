#pragma once

#include <string>

std::string get_hostname();
std::string detect_local_ip();
bool is_valid_ipv4(const std::string& s);