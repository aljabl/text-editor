#include <ctype.h> /* iscntrl() */
#include <stdio.h>
#include <stdlib.h> /* atexit() */
#include <string.h>
#include <termios.h> /* tcgetattr(), tcsetattr() */
#include <unistd.h> /* read() */

void restore_term(void);

// TODO: limit use of globals
struct termios orig_term;

struct termios init_term(void) {
    // struct termios orig_term;
    struct termios raw_term;

    // Get attributes of current (original) terminal.
    tcgetattr(STDIN_FILENO, &orig_term);

    atexit(restore_term); // can also not do this and keep orig_term as local. we'd pass orig_term to restore_term.

    /*
    Enable raw mode on new terminal. Disable echoing (can't see what we type as we type) and canonical mode (read input
    byte-by-byte) instead of line-by-line. We process each keypress as it occurs, instead of processing when the user
    hits `Enter` canonical mode).
    */
    raw_term = orig_term;
    raw_term.c_lflag &= ~(ECHO | ICANON | ISIG); 
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_term); // try TCSANOW. TCSAFLUSH discards unread input

    return orig_term;

}

void restore_term(void) {
    printf("Restoring original terminal.\n");
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
}


void enableRawMode(void) {
    // disable raw mode at exit
    // atexit(disableRawMode);
    // struct termios raw = orig_termios;
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);

    /* Disabling local flags ECHO and ICANON */
    raw.c_lflag &= ~(ECHO | ICANON | ISIG); /* Disable echoing (can't see what we type as we type) and canonical mode (read
                                        input byte-by-byte) instead of line-by-line. We process each keypress as it
                                        occurs, instead of processing when the user hits `Enter` (canonical mode). */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(void) {
    char c;

    // read one byte at a time 
    struct termios orig_term;
    orig_term = init_term();
    while (read(STDIN_FILENO, &c, 1) && c != 'q') {
        if (iscntrl(c)) { /* Check if c is a control character (nonprintable character)*/
            printf("%d\n", c);
        } else {
            printf("%d ('%c')\n", c, c);
        }
    }
    return 0;
}
