#pragma once
#include "node.h"
#include <string>

enum class CommandResult {
    Continue,
    Quit
};

CommandResult handle_command(const std::string& cmd, const std::string& args, Node& node);

void help();
void hello();
void info(const Node& node);