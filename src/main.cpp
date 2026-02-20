#include <iostream>
#include <string>
#include <cctype>
#include <thread>
#include <atomic>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "commands.h"
#include "receiver.h"
#include "node.h"

using namespace std;

static string trim(const string& s) {
    size_t a = 0;
    while (a < s.size() && isspace((unsigned char)s[a])) a++;
    size_t b = s.size();
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

static void split_cmd_args(const string& line, string& cmd, string& args) {
    string t = trim(line);
    if (t.empty()) { cmd.clear(); args.clear(); return; }

    size_t i = 0;
    while (i < t.size() && !isspace((unsigned char)t[i])) i++;
    cmd = t.substr(0, i);
    args = trim(t.substr(i));
}

int main() {
    bool auto_start = true;

    Node node;

    if (auto_start) node.start();

    cout << "Welcome to GDS! Use \"help\" to view commands.\n";

    string line;

    CommandResult res = CommandResult::Continue;

    while (res != CommandResult::Quit) {
        cout << "gds> " << flush;
        if (!getline(cin, line)) break;

        string cmd, args;
        split_cmd_args(line, cmd, args);
        if (cmd.empty()) continue;

        res = handle_command(cmd, args, node);
    }

    node.stop();
    cout << "Goodbye!\n";
    return 0;
}