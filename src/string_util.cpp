#include "string_util.h"

#include <cctype>

std::string trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace((unsigned char)s[a])) a++;
    size_t b = s.size();
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

void split_cmd_args(const std::string& line, std::string& cmd, std::string& args) {
    std::string t = trim(line);
    if (t.empty()) { cmd.clear(); args.clear(); return; }

    size_t i = 0;
    while (i < t.size() && !std::isspace((unsigned char)t[i])) i++;

    cmd = t.substr(0, i);
    args = trim(t.substr(i));
}

std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out;
    size_t pos = 0;
    std::string tok;
    while (next_token(s, pos, tok)) out.push_back(tok);
    return out;
}

bool next_token(const std::string& s, size_t& pos, std::string& out) {
    while (pos < s.size() && std::isspace((unsigned char)s[pos])) pos++;
    if (pos >= s.size()) return false;

    size_t start = pos;
    while (pos < s.size() && !std::isspace((unsigned char)s[pos])) pos++;

    out = s.substr(start, pos - start);
    return true;
}

std::string rest_of_line(const std::string& s, size_t pos) {
    while (pos < s.size() && std::isspace((unsigned char)s[pos])) pos++;
    if (pos >= s.size()) return "";
    return s.substr(pos);
}