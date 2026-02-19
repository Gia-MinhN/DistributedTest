#pragma once
#include <atomic>
#include <string>
#include <vector>

extern std::vector<std::string> introducer_ips;

void attempt_join_loop(std::atomic<bool>& running,
                       std::atomic<bool>& attempt_join,
                       std::atomic<bool>& joined,
                       const std::string& name,
                       const std::string& ip);