#include "codane/tui.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace codane {
namespace {

class RawMode {
public:
    RawMode() {
        if (tcgetattr(STDIN_FILENO, &old_) != 0) return;
        termios raw = old_;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) active_ = true;
    }

    ~RawMode() {
        if (active_) tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_);
    }

    RawMode(const RawMode&) = delete;
    RawMode& operator=(const RawMode&) = delete;

private:
    termios old_{};
    bool active_ = false;
};

void clear_screen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

std::string prompt(const std::string& label) {
    std::cout << label << std::flush;
    std::string value;
    std::getline(std::cin, value);
    return value;
}

void pause_after_action() {
    std::cout << "\nPress Enter to return to Codane..." << std::flush;
    std::string ignored;
    std::getline(std::cin, ignored);
}

int spawn_cli(const std::string& executable, const std::vector<std::string>& args) {
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "codane: fork failed: " << std::strerror(errno) << '\n';
        return 2;
    }
    if (pid == 0) {
        std::vector<std::string> owned;
        owned.reserve(args.size() + 1);
        owned.push_back(executable);
        owned.insert(owned.end(), args.begin(), args.end());

        std::vector<char*> argv;
        argv.reserve(owned.size() + 1);
        for (auto& item : owned) argv.push_back(item.data());
        argv.push_back(nullptr);

        execvp(executable.c_str(), argv.data());
        std::cerr << "codane: exec failed: " << std::strerror(errno) << '\n';
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        std::cerr << "codane: waitpid failed: " << std::strerror(errno) << '\n';
        return 2;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

int choose(const std::vector<std::string>& items, int selected, const std::string& provider) {
    RawMode raw;
    for (;;) {
        clear_screen();
        std::cout << "\033[1;36m╔══════════════════════════════════════╗\n"
                     "║               CODANE                 ║\n"
                     "╠══════════════════════════════════════╣\n\033[0m";
        std::cout << "  Provider: \033[1m" << provider << "\033[0m\n\n";
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (static_cast<int>(i) == selected) {
                std::cout << "\033[7m  > " << items[i] << "  \033[0m\n";
            } else {
                std::cout << "    " << items[i] << '\n';
            }
        }
        std::cout << "\n  ↑/↓ move   Enter select   q exit\n" << std::flush;

        unsigned char ch = 0;
        if (read(STDIN_FILENO, &ch, 1) != 1) return static_cast<int>(items.size()) - 1;
        if (ch == 'q' || ch == 'Q') return static_cast<int>(items.size()) - 1;
        if (ch == '\r' || ch == '\n') return selected;
        if (ch == 27) {
            std::array<unsigned char, 2> seq{};
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;
            if (seq[0] == '[' && seq[1] == 'A') {
                selected = (selected + static_cast<int>(items.size()) - 1) % static_cast<int>(items.size());
            } else if (seq[0] == '[' && seq[1] == 'B') {
                selected = (selected + 1) % static_cast<int>(items.size());
            }
        }
    }
}

} // namespace

int run_tui(const std::string& executable) {
    std::string provider = "codex";
    int selected = 0;
    const std::vector<std::string> items = {
        "New Run",
        "Validate Graph",
        "Resume Run",
        "Run Status",
        "Inspect Run",
        "Cancel Run",
        "Switch Provider",
        "CLI Help",
        "Exit",
    };

    for (;;) {
        const int choice = choose(items, selected, provider);
        selected = choice;
        clear_screen();

        if (choice == 0) {
            const auto graph = prompt("Graph JSON path: ");
            if (!graph.empty()) spawn_cli(executable, {"run", graph, "--provider", provider});
            pause_after_action();
        } else if (choice == 1) {
            const auto graph = prompt("Graph JSON path: ");
            if (!graph.empty()) spawn_cli(executable, {"validate", graph});
            pause_after_action();
        } else if (choice == 2) {
            const auto id = prompt("Run ID: ");
            if (!id.empty()) spawn_cli(executable, {"resume", id});
            pause_after_action();
        } else if (choice == 3) {
            const auto id = prompt("Run ID: ");
            if (!id.empty()) spawn_cli(executable, {"status", id});
            pause_after_action();
        } else if (choice == 4) {
            const auto id = prompt("Run ID: ");
            if (!id.empty()) spawn_cli(executable, {"inspect", id});
            pause_after_action();
        } else if (choice == 5) {
            const auto id = prompt("Run ID: ");
            if (!id.empty()) spawn_cli(executable, {"cancel", id});
            pause_after_action();
        } else if (choice == 6) {
            provider = provider == "codex" ? "fake" : "codex";
        } else if (choice == 7) {
            spawn_cli(executable, {"--help"});
            pause_after_action();
        } else {
            clear_screen();
            return 0;
        }
    }
}

} // namespace codane
