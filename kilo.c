/* -------------------------------- Includes -------------------------------- */
#include <ctype.h> /* iscntrl() */
#include <errno.h> /* errno */
#include <stdio.h> /* perror(), sscanf(), snprintf() */
#include <stdlib.h> /* atexit(), exit(), realloc(), free() */
#include <string.h> /* memcpy() */
#include <sys/ioctl.h> /* ioctl() */
#include <termios.h> /* tcgetattr(), tcsetattr() */
#include <unistd.h> /* read(), write() */

/* --------------------------------- Defines -------------------------------- */
#define KILO_VERSION "0.01"
#define CTRL_KEY(k) ((k) & 0x1F) /* For mapping CTRL key combinations */
#define CURSOR_BOTTOM_RIGHT "\x1b[999C\x1b[999B"

/* ------------------------------- Declarations ------------------------------ */
void restore_term(void);
void editor_process_keypress(void);

/* ---------------------------------- Data ---------------------------------- */
/* Editor state is global. */
struct editor_config {
    int rows;
    int cols;
    struct termios orig_term;
};

struct editor_config E;

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
    if (tcgetattr(STDIN_FILENO, &E.orig_term) == -1) {
        error_handler("tcgetattr");
    }

    atexit(restore_term); 

    /*
    Enable raw mode on new terminal. Disable echoing (can't see what we type as we type) and canonical mode (read input
    byte-by-byte) instead of line-by-line. We process each keypress as it occurs, instead of processing when the user
    hits `Enter` canonical mode).
    */
    raw_term = E.orig_term;
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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_term) == -1) {
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

int get_cursor_position(int *rows, int *cols) {
    char buffer[32]; 
    uint8_t i = 0;
    
    /* Request cursor position. */
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }
    /* Read response into buffer: ESC [ Pn ; Pn R */
    while (i < sizeof(buffer) - 1) {
        buffer[i] = editor_read_key();
        if (buffer[i] == 'R') {
            break;
        }
        i++;
    }
    buffer[i] = '\0';
    printf("buffer: %s\r\n", &buffer[1]);

    if (buffer[0] != '\x1b' || buffer[1] != '[') {
        return -1;
    }
    if (sscanf(&buffer[2], "%d;%d", rows, cols) != 2) {
        return -1;
    }

    return 0;
}

int get_window_size(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, CURSOR_BOTTOM_RIGHT, 12) != 12) {
            return -1;
        }
        /* Get window size the hard way if ioctl() fails. */
        return get_cursor_position(rows, cols);
    } 
    *rows = ws.ws_row;
    *cols = ws.ws_col;

    return 0;
}

/* ------------------------------ Append Buffer ----------------------------- */
struct abuf {
    char *str;
    uint length;
};

#define ABUF_INIT {NULL, 0} // constructor for append buffer

void ab_append(struct abuf *ab, const char *s, int length) {
    /* Allocate memory: size of existing buff plus size of string to be appended. */
    char *new_buff = realloc(ab->str, ab->length + length);

    if (new_buff == NULL) {
        return;
    }

    /* make sure we don't have a memory leak here */
    memcpy(&new_buff[ab->length], s, length);
    ab->str = new_buff;
    ab->length += length;
}

/* Destructor */
void ab_free(struct abuf *ab) {
    free(ab->str);
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
            break;
        default:
            if (iscntrl(c)) { /* Check if c is a control character (nonprintable character) */
                printf("%d\r\n", c);
            } else {
                printf("%d ('%c')\r\n", c, c);
            }
    }
}

/* --------------------------------- Output --------------------------------- */
void editor_draw_rows(struct abuf *ab) {
    char col[8] = "";
    char debug[120] = "";
    char welcome[80] = "";
    int col_length;
    int welcome_length;
    int debug_length;
    int padding;

    for (uint8_t y = 0; y < E.rows; y++) {
        /* Clear each row as we write to them */
        ab_append(ab, "\x1b[K", 3);

        col_length = snprintf(col, sizeof(col), "%d ", y);
        ab_append(ab, col, col_length);

        if (y == 0) { // y == E.rows / 3)
            welcome_length = snprintf(welcome, sizeof(welcome), "Kilo editor -- Version %s", KILO_VERSION);
            /* Truncate welcome message if window width too thin. */
            if (welcome_length > E.cols) {
                welcome_length = E.cols;
            }
            /* Divide window length by 2, then subtract half of message's length (from that half-length). */
            padding = (E.cols - welcome_length) / 2;
  
            while (padding) {
                ab_append(ab, " ", 1);
                padding--;
            }
            ab_append(ab, welcome, welcome_length);
        } else {
            // ab_append(ab, "~", 1);
        }

        if (y < E.rows - 1) {
            ab_append(ab, "\r\n", 2);
       } else { // print debug info on last line 
            debug_length = snprintf(debug, sizeof(debug), "E.rows = %d, E.cols = %d", E.rows, E.cols);
            ab_append(ab, debug, debug_length);
       }
    }
}

void editor_refresh_screen(void) {
    struct abuf ab = ABUF_INIT;

    /* Hide cursor */
    ab_append(&ab, "\x1b[?25l", 6);

    /* Reposition curser to top-left corner of terminal 
    H = Resposition cursor. Default args are 1;1 (first column, first row).
    */
    ab_append(&ab, "\x1b[H", 3);
    /* Draw tildes at start of first 24 lines (terminal size TBD) and reposition cursor. */
    editor_draw_rows(&ab);
    ab_append(&ab, "\x1b[H", 3);

    /* Show cursor */
    ab_append(&ab, "\x1b[?25l", 6);

    write(STDOUT_FILENO, ab.str, ab.length);
    ab_free(&ab);
}

/* ---------------------------------- Init ---------------------------------- */
/* Initialize fields in E struct (global editor state). */
void init_editor(void) {
    if (get_window_size(&E.rows, &E.cols) == -1) {
        error_handler("get_window_size");
    }
}

int main(void) {
    init_term();
    init_editor();
    while(1) {
        editor_refresh_screen();
        editor_process_keypress();
    }

    return 0;
}
