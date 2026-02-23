#pragma once

#include <string>
#include <vector>

std::string trim(const std::string& s);
void split_cmd_args(const std::string& line, std::string& cmd, std::string& args);
std::vector<std::string> split_ws(const std::string& s);
bool next_token(const std::string& s, size_t& pos, std::string& out);
std::string rest_of_line(const std::string& s, size_t pos);