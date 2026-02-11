#pragma once

#include <atomic>

void udp_receiver_loop(int sock, std::atomic<bool>& running);
void tcp_receiver_loop(int sock, std::atomic<bool>& running);