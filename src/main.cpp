#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "commands.h"
#include "node.h"
#include "string_util.h"

static std::vector<std::string> load_seeds_file(const std::string& path) {
    std::vector<std::string> seeds;
    std::ifstream in(path);
    if (!in) return seeds;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        seeds.push_back(line);
    }
    return seeds;
}

int main() {
    bool auto_start = false;

    std::vector<std::string> seeds = load_seeds_file("seeds.conf");
    Node node(std::move(seeds));
    if (auto_start) node.start();

    std::cout << "Welcome to GDS! Use \"help\" to view commands.\n";

    std::string line;
    CommandResult res = CommandResult::Continue;

    while (res != CommandResult::Quit) {
        std::cout << "gds> " << std::flush;
        if (!std::getline(std::cin, line)) break;

        std::string cmd, args;
        split_cmd_args(line, cmd, args);
        if (cmd.empty()) continue;

        res = handle_command(cmd, args, node);
    }

    node.stop();
    std::cout << "Goodbye!\n";
    return 0;
}