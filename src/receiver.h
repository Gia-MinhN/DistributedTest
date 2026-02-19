#pragma once

#include <atomic>
#include <cstddef>
#include <netinet/in.h>

class Node;

void udp_parser(Node& node, const sockaddr_in& from, const char* data, size_t len);

void udp_receiver_loop(int sock, Node& node);
void tcp_receiver_loop(int sock, Node& node);