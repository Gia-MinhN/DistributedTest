#include "commands.h"

#include <iostream>
#include <algorithm>
#include <string>
#include <thread>
#include <chrono>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include "node.h"
#include "table_print.h"

void help() {
    std::cout
        << "Commands:\n"
        << "  help, man   - show this message\n"
        << "  info        - show this node's info (name, ip, status)\n"
        << "  start       - start networking + background join attempts\n"
        << "  stop        - stop networking + background threads\n"
        << "  list        - list all known members\n"
        << "  quit, exit  - exit the program\n";
}


void info(const Node& node) {
    const bool running = node.running.load();
    const bool joined  = node.joined.load();
    const bool trying  = node.attempt_join.load();
    const bool seed    = node.is_seed;

    const std::string name_str   = node.name;
    const std::string ip_str     = node.ip;
    const std::string role_str   = (seed ? "Seed" : "Node");
    const std::string inc_str    = std::to_string(node.incarnation);
    const std::string status_str = (running ? "Running" : "Not running");

    std::string joined_str =
        seed ? "Yes" :
        joined ? "Yes" :
        (running && trying) ? "Attempting" : "No";

    const size_t max_len = std::max({
        name_str.size(), ip_str.size(), role_str.size(),
        inc_str.size(), status_str.size(), joined_str.size()
    });

    const size_t label_w = 11;
    const size_t inner_w = 1 + label_w + 1 + max_len + 1;
    const size_t total_w = inner_w + 2;

    auto border = [&]() {
        std::cout << "+" << std::string(total_w - 2, '-') << "+\n";
    };

    auto row = [&](const std::string& label, const std::string& value) {
        std::string l = label;
        if (l.size() < label_w) l += std::string(label_w - l.size(), ' ');

        std::string v = value;
        if (v.size() < max_len) v += std::string(max_len - v.size(), ' ');

        std::cout << "| " << l << " " << v << " |\n";
    };

    const std::string title = "Node Info";
    border();
    {
        size_t pad_total = total_w - 2;
        size_t left = (pad_total > title.size()) ? (pad_total - title.size()) / 2 : 0;
        size_t right = (pad_total > title.size()) ? (pad_total - title.size() - left) : 0;
        std::cout << "|" << std::string(left, ' ') << title << std::string(right, ' ') << "|\n";
    }
    border();

    row("Name",        name_str);
    row("IP",          ip_str);
    row("Role",        role_str);
    row("Status",      status_str);
    row("Joined",      joined_str);
    row("Incarnation", inc_str);

    border();
}

static const char* status_str(MemberStatus s) {
    switch (s) {
        case MemberStatus::Alive:   return "Alive";
        case MemberStatus::Suspect: return "Suspect";
        case MemberStatus::Dead:    return "Dead";
        default:                    return "Unknown";
    }
}

void list_members(const Node& node) {
    std::vector<std::string> headers = {"NAME", "STATE  ", "IPV4", "LAST_SEEN_MS", "INC"};
    std::vector<std::vector<std::string>> rows;
    rows.reserve(node.membership.size());

    {
        std::lock_guard<std::mutex> lk(node.membership_mu);
        for (const auto& [name, info] : node.membership) {
            std::string shown_name = (name == node.name) ? (name + " *") : name;

            rows.push_back({
                shown_name,
                status_str(info.status),
                info.ip,
                std::to_string(info.last_seen_ms),
                std::to_string(info.incarnation)
            });
        }
    }

    print_table(headers, rows);
}

static termios oldt;
static bool saved_term = false;

void set_nonblocking(bool enable) {
    if (enable) {
        if (!saved_term) {
            tcgetattr(STDIN_FILENO, &oldt);
            saved_term = true;
        }
        termios newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    } else if (saved_term) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
    }
}

void live_list_members(const Node& node) {
    set_nonblocking(true);

    const int refresh_ms = 1000;
    std::atomic<bool> running{true};
    size_t prev_height = 0;

    std::vector<std::string> headers = {"NAME", "STATE  ", "IPV4", "LAST_SEEN_MS", "INC"};

    while (running) {
        std::vector<std::vector<std::string>> rows;

        {
            std::lock_guard<std::mutex> lk(node.membership_mu);
            for (const auto& [name, info] : node.membership) {
                std::string shown_name = (name == node.name) ? (name + " *") : name;
                rows.push_back({
                    shown_name,
                    status_str(info.status),
                    info.ip,
                    std::to_string(info.last_seen_ms),
                    std::to_string(info.incarnation)
                });
            }
        }

        print_table_live(headers, rows, prev_height);

        std::cout << "Press 'q' to quit live view...\n" << std::flush;

        char c = 0;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 'q' || c == 'Q') {
                running = false;
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(refresh_ms));
    }

    set_nonblocking(false);
    std::cout << "\nExited live view.\n";
}

CommandResult handle_command(const std::string& cmd, const std::string& args, Node& node) {
    (void)args;

    if (cmd == "help" || cmd == "man")  { help(); return CommandResult::Continue; }

    if (cmd == "info")   { info(node); return CommandResult::Continue; }

    if (cmd == "start")  { node.start(); return CommandResult::Continue; }
    if (cmd == "stop")   { node.stop(); return CommandResult::Continue; }

    if (cmd == "list") { list_members(node); return CommandResult::Continue; }
    if (cmd == "live_list") { live_list_members(node); return CommandResult::Continue; }

    if (cmd == "quit" || cmd == "exit") return CommandResult::Quit;

    std::cout << "Unknown command: " << cmd << "\n";
    return CommandResult::Continue;
}