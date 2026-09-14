#include "codane/tui.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
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

enum class Key {
    Unknown,
    Up,
    Down,
    Left,
    Right,
    Enter,
    Backspace,
    NewFolder,
    Quit,
};

Key read_key() {
    unsigned char ch = 0;
    if (read(STDIN_FILENO, &ch, 1) != 1) return Key::Quit;
    if (ch == '\r' || ch == '\n') return Key::Enter;
    if (ch == 127 || ch == 8) return Key::Backspace;
    if (ch == 'q' || ch == 'Q') return Key::Quit;
    if (ch == 'n' || ch == 'N') return Key::NewFolder;
    if (ch != 27) return Key::Unknown;

    std::array<unsigned char, 2> seq{};
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return Key::Unknown;
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return Key::Unknown;
    if (seq[0] != '[') return Key::Unknown;
    if (seq[1] == 'A') return Key::Up;
    if (seq[1] == 'B') return Key::Down;
    if (seq[1] == 'C') return Key::Right;
    if (seq[1] == 'D') return Key::Left;
    return Key::Unknown;
}

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

bool is_json_file(const std::filesystem::directory_entry& entry) {
    std::error_code ec;
    if (!entry.is_regular_file(ec) || ec) return false;
    auto ext = entry.path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext == ".json";
}

struct BrowserEntry {
    std::filesystem::path path;
    bool directory = false;
};

std::vector<BrowserEntry> graph_entries(const std::filesystem::path& directory,
                                        std::string& error) {
    std::vector<BrowserEntry> entries;
    std::error_code ec;
    std::filesystem::directory_iterator it(directory, ec);
    if (ec) {
        error = ec.message();
        return entries;
    }

    for (const auto& item : it) {
        std::error_code type_ec;
        const bool is_dir = item.is_directory(type_ec);
        if (type_ec) continue;
        if (!is_dir && !is_json_file(item)) continue;
        entries.push_back({item.path(), is_dir});
    }

    auto lower_name = [](const std::filesystem::path& path) {
        auto text = path.filename().string();
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return text;
    };
    std::sort(entries.begin(), entries.end(), [&](const BrowserEntry& a, const BrowserEntry& b) {
        if (a.directory != b.directory) return a.directory > b.directory;
        return lower_name(a.path) < lower_name(b.path);
    });
    return entries;
}

void render_browser(const std::filesystem::path& directory,
                    const std::vector<BrowserEntry>& entries,
                    std::size_t selected,
                    const std::string& message) {
    constexpr std::size_t page_size = 12;
    clear_screen();
    std::cout << "\033[1;36m╔════════════════════════════════════════════╗\n"
                 "║          CODANE FILE BROWSER              ║\n"
                 "╚════════════════════════════════════════════╝\033[0m\n";
    std::cout << "\n  Folder: " << directory.string() << "\n";
    std::cout << "  Showing folders and JSON graph files\n\n";

    if (entries.empty()) {
        std::cout << "    (no folders or .json files here)\n";
    } else {
        const std::size_t start = selected >= page_size ? selected - page_size + 1 : 0;
        const std::size_t end = std::min(entries.size(), start + page_size);
        for (std::size_t i = start; i < end; ++i) {
            const auto prefix = entries[i].directory ? "[DIR] " : "[JSON]";
            const auto name = entries[i].path.filename().string();
            if (i == selected) {
                std::cout << "\033[7m  > " << prefix << " " << name << "  \033[0m\n";
            } else {
                std::cout << "    " << prefix << " " << name << '\n';
            }
        }
        if (entries.size() > page_size) {
            std::cout << "\n  " << (selected + 1) << "/" << entries.size() << '\n';
        }
    }

    if (!message.empty()) std::cout << "\n  \033[1;33m" << message << "\033[0m\n";
    std::cout << "\n  ↑/↓ move   Enter open/select   ←/Backspace up\n"
                 "  N new folder                         Q cancel\n"
              << std::flush;
}

std::optional<std::filesystem::path> browse_for_graph() {
    std::error_code ec;
    std::filesystem::path current = std::filesystem::current_path(ec);
    if (ec) current = ".";
    current = std::filesystem::absolute(current, ec);
    if (ec) current = ".";

    std::size_t selected = 0;
    std::string message;

    for (;;) {
        std::string listing_error;
        auto entries = graph_entries(current, listing_error);
        if (!listing_error.empty()) message = "Cannot open folder: " + listing_error;
        if (entries.empty()) selected = 0;
        else if (selected >= entries.size()) selected = entries.size() - 1;

        Key key = Key::Unknown;
        {
            RawMode raw;
            render_browser(current, entries, selected, message);
            key = read_key();
        }
        message.clear();

        if (key == Key::Quit) return std::nullopt;
        if (key == Key::Up && !entries.empty()) {
            selected = selected == 0 ? entries.size() - 1 : selected - 1;
        } else if (key == Key::Down && !entries.empty()) {
            selected = (selected + 1) % entries.size();
        } else if ((key == Key::Left || key == Key::Backspace) && current.has_parent_path()) {
            const auto parent = current.parent_path();
            if (parent != current && !parent.empty()) {
                current = parent;
                selected = 0;
            }
        } else if ((key == Key::Enter || key == Key::Right) && !entries.empty()) {
            const auto& picked = entries[selected];
            if (picked.directory) {
                current = picked.path;
                selected = 0;
            } else if (key == Key::Enter) {
                return picked.path;
            }
        } else if (key == Key::NewFolder) {
            clear_screen();
            std::cout << "Create folder inside:\n  " << current.string() << "\n\n";
            const auto name = prompt("Folder name: ");
            if (name.empty()) {
                message = "Folder creation cancelled.";
                continue;
            }
            if (name == "." || name == ".." || name.find('/') != std::string::npos ||
                name.find('\\') != std::string::npos) {
                message = "Use a simple folder name without / or \\.";
                continue;
            }
            const auto new_dir = current / name;
            std::error_code create_ec;
            if (std::filesystem::create_directory(new_dir, create_ec)) {
                current = new_dir;
                selected = 0;
                message = "Folder created.";
            } else if (create_ec) {
                message = "Could not create folder: " + create_ec.message();
            } else {
                message = "Folder already exists.";
            }
        }
    }
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

        const auto key = read_key();
        if (key == Key::Quit) return static_cast<int>(items.size()) - 1;
        if (key == Key::Enter) return selected;
        if (key == Key::Up) {
            selected = (selected + static_cast<int>(items.size()) - 1) % static_cast<int>(items.size());
        } else if (key == Key::Down) {
            selected = (selected + 1) % static_cast<int>(items.size());
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
            const auto graph = browse_for_graph();
            if (graph) {
                clear_screen();
                spawn_cli(executable, {"run", graph->string(), "--provider", provider});
                pause_after_action();
            }
        } else if (choice == 1) {
            const auto graph = browse_for_graph();
            if (graph) {
                clear_screen();
                spawn_cli(executable, {"validate", graph->string()});
                pause_after_action();
            }
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
