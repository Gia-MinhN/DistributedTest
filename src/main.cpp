#include <iostream>
#include <string>
#include <cctype>
#include <thread>
#include <atomic>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <limits.h>

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


static string get_hostname() {
    char buf[HOST_NAME_MAX + 1];
    if (gethostname(buf, sizeof(buf)) == 0) {
        buf[sizeof(buf) - 1] = '\0';
        return string(buf);
    }
    return "unknown";
}

int main() {
    string hostname = get_hostname();

    Node node(hostname, "test_ip");
    node.start();

    cout << "Welcome to GDS! Use \"help\" to view commands.\n";

    string line;
    while (node.is_running()) {
        cout << "gds> " << flush;
        if (!getline(cin, line)) break;

        string cmd, args;
        split_cmd_args(line, cmd, args);
        if (cmd.empty()) continue;

        CommandResult res = handle_command(cmd, args, node);
        if (res == CommandResult::Quit) {
            node.stop();
        }
    }

    cout << "Goodbye!\n";
    return 0;
}