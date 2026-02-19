#include "commands.h"
#include "node.h"
#include <iostream>

void help() {
    std::cout << "Commands:\n"
              << "  help  - show this message\n"
              << "  quit  - exit the program\n";
}

void hello() {
    std::cout << "Hello from GDS!\n";
}

void info(const Node& node) {
    std::cout << "Name:   " << node.name << "\n";
    std::cout << "IP:     " << node.ip << "\n";
    std::string run_status = (node.is_running()) ? "Running" : "Not running";
    std::cout << "Status: " << run_status << "\n";
}

CommandResult handle_command(const std::string& cmd, const std::string& args, Node& node) {
    (void)args;

    if (cmd == "help" || cmd == "man")  { help(); return CommandResult::Continue; }

    if (cmd == "hello")  { hello(); return CommandResult::Continue; }
    if (cmd == "info")   { info(node); return CommandResult::Continue; }

    if (cmd == "start")  { node.start(); return CommandResult::Continue; }
    if (cmd == "stop")   { node.stop(); return CommandResult::Continue; }

    if (cmd == "quit" || cmd == "exit") return CommandResult::Quit;

    std::cout << "Unknown command: " << cmd << "\n";
    return CommandResult::Continue;
}