/* -------------------------------- Includes -------------------------------- */
#include <ctype.h> /* iscntrl() */
#include <errno.h> /* errno */
#include <stdio.h> /* perror() */
#include <stdlib.h> /* atexit(), exit() */
#include <string.h>
#include <termios.h> /* tcgetattr(), tcsetattr() */
#include <unistd.h> /* read(), write() */

/* --------------------------------- Defines -------------------------------- */
#define CTRL_KEY(k) ((k) & 0x1F) /* For mapping CTRL key combinations */

/* ------------------------------- Declarations ------------------------------ */
void restore_term(void);

/* ---------------------------------- Data ---------------------------------- */
// TODO: limit use of globals
struct termios orig_term;

/* -------------------------------- Terminal -------------------------------- */
void error_handler(const char *s) {
    /* Clear screen and resposition cursor to top-left on error exit. */
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void init_term(void) {
    struct termios raw_term;

    // Get attributes of current (original) terminal.
    if (tcgetattr(STDIN_FILENO, &orig_term) == -1) {
        error_handler("tcgetattr");
    }

    atexit(restore_term); // can also not do this and keep orig_term as local. we'd pass orig_term to restore_term.

    /*
    Enable raw mode on new terminal. Disable echoing (can't see what we type as we type) and canonical mode (read input
    byte-by-byte) instead of line-by-line. We process each keypress as it occurs, instead of processing when the user
    hits `Enter` canonical mode).
    */
    raw_term = orig_term;
    /*
    OUTPUT FLAGS
        OPOST: output processing features
    */ 
   raw_term.c_oflag &= ~(OPOST); // Now need to use \r\n when printing
    /*
    INPUT FLAGS
        BRKINT: (when enabled) break condition causes SIGINT signal to be sent to program
        INPCK: parity checking
        ISTRIP: strips 8th bit of each input byte
        IXON: Ctrl-S and Ctrl-Q
        ICRNL: translate carriage returns into newlines
    */ 
    raw_term.c_iflag &= ~(BRKINT | INPCK | ISTRIP | IXON | ICRNL);
    /*
    LOCAL FLAGS
        ECHO: disable echoing
        ICANON: canonical mode
        ISIG: Ctrl-C and Ctrl-Z
        IEXTEN: Ctrl-V and Ctrl-O (Macs)
    */
    raw_term.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); 
    /*
    CONTROL FLAGS & CONTROL CHARACTERS
        CS8: sets character size to 8 bits per byte
        VMIN: minimum number of bytes of input needed before read() can return
        VTIME: maximum amount of time (in ms) to wait before read() returns
    */
    raw_term.c_cflag |= (CS8);
    raw_term.c_cc[VMIN] = 0; // read() returns as soon as there is input to be read
    raw_term.c_cc[VTIME] = 1; // 1ms
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_term) == -1) { // try TCSANOW. TCSAFLUSH discards unread input
        error_handler("tcsetattr");
    }
}

void restore_term(void) {
    printf("Restoring original terminal.\r\n");
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term) == -1) {
        error_handler("tcsetattr");
    }
}

char editor_read_key(void) {
    int ret;
    char c;

    while((ret = read(STDIN_FILENO, &c, 1)) != 1) {
        if (ret == -1 && errno != EAGAIN) {
            error_handler("read");
        }
    }
    return c;
}

/* ---------------------------------- Input --------------------------------- */

void editor_process_keypress(void) {
    char c = editor_read_key();

    /* CTRL key combination mapping */
    switch(c) {
        case CTRL_KEY('q'):
            /* Clear screen and resposition cursor to top-left on exit. */
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break; // chris is judging this 
        default:
            if (iscntrl(c)) { /* Check if c is a control character (nonprintable character) */
                printf("%d\r\n", c);
            } else {
                printf("%d ('%c')\r\n", c, c);
            }
    }
}

/* --------------------------------- Output --------------------------------- */
void editor_refresh_screen(void) {
    /* Write 4 byte escape sequence to terminal.
    \x1b (decimal 27) = escape character
    2J = clear entire screen
    */
    write(STDOUT_FILENO, "\x1b[2J", 4);
    /* Reposition curser to top-left corner of terminal 
    H = Resposition cursor. Default args are 1;1 (first column, first row).
    */
    write(STDOUT_FILENO, "\x1b[H", 3);
}

/* ---------------------------------- Init ---------------------------------- */
int main(void) {
    init_term();
    while(1) {
        editor_refresh_screen();
        editor_process_keypress();
    }

    return 0;
}
