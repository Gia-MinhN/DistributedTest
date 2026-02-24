#include "commands.h"

#include <iostream>

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
    std::cout << "Name:   " << node.name << "\n";
    std::cout << "IP:     " << node.ip << "\n";

    bool running = node.running.load();
    bool joined  = node.joined.load();
    bool trying  = node.attempt_join.load();
    bool seed    = (node.name == "node0");

    std::cout << "Status: " << (running ? "Running" : "Not running") << "\n";

    std::string joined_str;
    if (seed) {
        joined_str = "Yes";
    } else if (joined) {
        joined_str = "Yes";
    } else if (running && trying) {
        joined_str = "Attempting";
    } else {
        joined_str = "No";
    }

    std::cout << "Joined: " << joined_str << "\n";
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
    std::vector<std::string> headers = {"NAME", "STATE", "IPV4", "LAST_SEEN_MS", "INC"};
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

CommandResult handle_command(const std::string& cmd, const std::string& args, Node& node) {
    (void)args;

    if (cmd == "help" || cmd == "man")  { help(); return CommandResult::Continue; }

    if (cmd == "info")   { info(node); return CommandResult::Continue; }

    if (cmd == "start")  { node.start(); return CommandResult::Continue; }
    if (cmd == "stop")   { node.stop(); return CommandResult::Continue; }

    if (cmd == "list") { list_members(node); return CommandResult::Continue; }

    if (cmd == "quit" || cmd == "exit") return CommandResult::Quit;

    std::cout << "Unknown command: " << cmd << "\n";
    return CommandResult::Continue;
}