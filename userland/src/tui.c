#include "tui.h"
#include <stdio.h>
#include <string.h>

void tui_clear_screen(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void tui_print_divider(void) {
    printf(ANSI_DIM "----------------------------------------------------------------------" ANSI_RESET "\n");
}

void tui_print_header(const char *title) {
    printf("\n" ANSI_BOLD ANSI_CYAN "======================================================================" ANSI_RESET "\n");
    printf(ANSI_BOLD ANSI_WHITE "  %s" ANSI_RESET "\n", title);
    printf(ANSI_BOLD ANSI_CYAN "======================================================================" ANSI_RESET "\n\n");
}

void tui_pause(void) {
    printf("\n" ANSI_YELLOW "Press Enter to return to main menu..." ANSI_RESET);
    fflush(stdout);
    char buf[64];
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        return;
    }
}

int tui_read_line(char *buf, size_t maxlen) {
    if (!fgets(buf, maxlen, stdin)) {
        return -1;
    }
    size_t len = strlen(buf);
    if (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[len - 1] = '\0';
    }
    if (len > 1 && (buf[len - 2] == '\n' || buf[len - 2] == '\r')) {
        buf[len - 2] = '\0';
    }
    return 0;
}
