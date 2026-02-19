#pragma once

#include <atomic>

class Node;

void udp_receiver_loop(int sock, Node& node);
void tcp_receiver_loop(int sock, Node& node);