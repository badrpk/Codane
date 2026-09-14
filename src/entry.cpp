#include "codane/chat_tui.hpp"

#include <iostream>
#include <string>
#include <unistd.h>

int codane_cli_main(int argc, char** argv);

int main(int argc, char** argv) {
    if (argc < 2) {
        if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
            return codane::run_chat_tui();
        }
        return codane_cli_main(argc, argv);
    }
    return codane_cli_main(argc, argv);
}
