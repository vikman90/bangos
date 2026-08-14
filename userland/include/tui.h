#ifndef TUI_H
#define TUI_H

#include <stddef.h>

#define ANSI_RESET       "\033[0m"
#define ANSI_BOLD        "\033[1m"
#define ANSI_DIM         "\033[2m"
#define ANSI_UNDERLINE   "\033[4m"

#define ANSI_BLACK       "\033[30m"
#define ANSI_RED         "\033[31m"
#define ANSI_GREEN       "\033[32m"
#define ANSI_YELLOW      "\033[33m"
#define ANSI_BLUE        "\033[34m"
#define ANSI_MAGENTA     "\033[35m"
#define ANSI_CYAN        "\033[36m"
#define ANSI_WHITE       "\033[37m"

#define ANSI_BG_BLACK    "\033[40m"
#define ANSI_BG_BLUE     "\033[44m"
#define ANSI_BG_CYAN     "\033[46m"

void tui_clear_screen(void);
void tui_print_header(const char *title);
void tui_print_divider(void);
void tui_pause(void);
int  tui_read_line(char *buf, size_t maxlen);

#endif /* TUI_H */
