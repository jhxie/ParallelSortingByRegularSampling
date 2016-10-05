#ifndef MACRO_H
#define MACRO_H

#define _BSD_SOURCE /* Definition for random() is exposed when defined */
#define _POSIX_C_SOURCE 200809L

/* ANSI Color Escape Sequences */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#endif /* MACRO_H */
