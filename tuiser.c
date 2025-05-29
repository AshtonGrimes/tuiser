#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <ncurses.h>

#define _CTRL(ch) (ch & 0x1f)

#define PRINT_ERR(str) do {\
    move(FAIL_MSG_ROW, 0);\
    clrtoeol();\
    mvprintw(FAIL_MSG_ROW, (COLS - strlen(str) + 1) / 2, "%s", str);\
} while (0);
 
typedef unsigned int uint;

typedef enum NextArg {
    NONE,
    SET_DEVICE,
    SET_BAUD,
    SET_MODE
} NextArg;

typedef enum Selection {
    FD,
    BAUD,
    SEND
} Selection;

typedef enum Mode {
    CHAR,
    GRAPH,
    HEX,
    UINT,
    INT
} Mode;
const uint NUM_MODES = 5;

const char *OPTS_HELP = "\n"    
"Options:\n"
"    -b | --baud <baud>     Set baud\n"
"    -d | --device <path>   Set device path\n"
"    -h | --help            Display this help message\n"
"    -m | --mode <mode>     Set monitor mode: char (default), graph, hex, uint, int\n"
"    -r | --read            Immediately read device (specified with -d)\n"
"    -n | --no-read         (Default) Opposite of -r\n"
"\n";

const char *ARG_MISSING_MSG = "Missing value for ";
const char *ARG_BAD_MSG = "Bad argument ";
const uint ARG_BUFFER_MAX_LEN = 32;

const char *ARG_MODE_STR[] = {
    "char",
    "graph",
    "hex",
    "uint",
    "int"
};
const char *ARG_BAD_MODE_MSG = "Bad mode argument; must be char, graph, hex, uint, or int";

const char *HELP_MSG = "Ctrl-WASD to select input, Ctrl-Z to change monitor mode, Ctrl-X to toggle monitor, Ctrl-C to exit";
const char *NO_FD_PLACEHOLDER = "<none>";
const char *FD_FAIL_MSG = "Can't access device: ";
const char *BAD_BAUD_MSG = "Bad baudrate; check `man 3 termios` for a full list of baudrates";
const char *BAUD_SET_FAIL_MSG = "Can't set baud: ";
const char *SEND_NO_FD_MSG = "No device open for I/O";
const char *TERMINAL_TOO_SMALL_MSG = "Terminal too small, use another mode";
const char *STATUS_MSG[] = {"Device: ", ", baud: ", ", monitor mode: "};
const char *STATUS_MODE_MSG[] = {"char", "graph", "hex", "uint", "int"};
const char *STATUS_OFF_MSG = " (off)";
const char *INPUT_MSG[] = {"Dev. path: ", "Baud: ", "Send: "};

const uint HELP_MSG_ROW = 7;
const uint FAIL_MSG_ROW = 9;
const uint STATUS_MSG_ROW = 10;
const uint DATA_START_ROW = 12;
const uint INPUT_ROW[] = {1, 1, 4};
const uint INPUT_PADDING = 2;

const uint DATA_WIN_SIZE[] = {
    51, // HEX; (16 * (2 hex digits + 1 space)) + (2 * 2 borders) - trailing space
    67, // UINT; (16 * (3 digits + 1 space)) + (2 * 2 borders) - trailing space
    83 // INT; (16 * (1 sign + 3 digits + 1 space)) + (2 * 2 borders) - trailing space
};
const uint DATA_NUM_WIDTH[] = {
    3, // 2 hex digits + 1 space
    4, // 3 digits + 1 space
    5 // 1 sign + 3 digits + 1 space
};
const char *DATA_NUM_FMT[] = {
    "%2hhX",
    "%3hhu",
    "%4hhi"
};
const uint MIN_GRAPH_ROWS = 5;

const int BAUD_MAP[][2] = {
    {50, B50},
    {75, B75},
    {110, B110},
    {134, B134},
    {150, B150},
    {200, B200},
    {300, B300},
    {600, B600},
    {1200, B1200},
    {1800, B1800},
    {2400, B2400},
    {4800, B4800},
    {9600, B9600},
    {19200, B19200},
    {38400, B38400},
    {57600, B57600},
    {115200, B115200},
    {230400, B230400},
    {460800, B460800},
    {500000, B500000},
    {576000, B576000},
    {921600, B921600},
    {1000000, B1000000},
    {1152000, B1152000},
    {1500000, B1500000},
    {2000000, B2000000}
};
const int BAUD_MAP_LEN = 26;

// Global constants determined at runtime
int ROWS, COLS, DATA_ROWS;
double GRAPH_SCALAR, GRAPH_CENTER;

// Mutable global variables
struct termios global_config;
bool global_monitoring = false;
Mode global_mode = 0;
int global_fd = -1;
char *global_fd_name = NULL;

void *safe_malloc(int size) {
    void *ret = malloc(size);
    if (ret) return ret;

    if (global_fd >= 0) close(global_fd);
    free(global_fd_name);
    endwin();
    exit(1);
}

WINDOW *make_input(uint row, uint COL, uint len, const char *msg) {
    WINDOW *win = newwin(3, len, row, COL);
    // nodelay(win, true);
    wtimeout(win, 50);
    box(win, 0, 0);
    mvwprintw(win, 1, 2, "%s", msg);
    wrefresh(win);
    return win;
}

void print_status() {
    move(STATUS_MSG_ROW, 0);
    clrtoeol();

    uint index = 0;
    uint current = cfgetospeed(&global_config);
    for (uint i = 0; i < BAUD_MAP_LEN; i++) {
        if (BAUD_MAP[i][1] != current) continue;
        index = i;
        break;
    }
    char baud[8];
    snprintf(baud, 8, "%i", BAUD_MAP[index][0]);

    mvprintw(
        STATUS_MSG_ROW,
        (
            COLS -
            strlen(STATUS_MSG[0]) -
            strlen(global_fd_name) -
            strlen(STATUS_MSG[1]) -
            strlen(baud) -
            strlen(STATUS_MSG[2]) -
            strlen(STATUS_MODE_MSG[global_mode]) -
            (global_monitoring ? 0 : strlen(STATUS_OFF_MSG))
        + 1) / 2,
        "%s%s%s%s%s%s",
        STATUS_MSG[0],
        global_fd_name,
        STATUS_MSG[1],
        baud,
        STATUS_MSG[2],
        STATUS_MODE_MSG[global_mode]
    );
    if (!global_monitoring) printw("%s", STATUS_OFF_MSG);
    refresh();
}

void fd_err() {
    global_fd = -1;
    move(FAIL_MSG_ROW, 0);
    clrtoeol();
    mvprintw(
        FAIL_MSG_ROW,
        (COLS - strlen(FD_FAIL_MSG) - strlen(strerror(errno)) + 1) / 2,
        "%s%s",
        FD_FAIL_MSG,
        strerror(errno)
    );

    free(global_fd_name);
    global_fd_name = safe_malloc((strlen(NO_FD_PLACEHOLDER) + 1) * sizeof(char));
    strncpy(global_fd_name, NO_FD_PLACEHOLDER, strlen(NO_FD_PLACEHOLDER));
    print_status();
}

void try_update_fd(char *str) {
    if (global_fd >= 0) {
        close(global_fd);
        global_fd = -1;
    }
    
    if (!str) goto reopen;

    free(global_fd_name);

    uint len = strlen(str);
    if (len <= 0) {
        global_fd_name = safe_malloc((strlen(NO_FD_PLACEHOLDER) + 1) * sizeof(char));
        strncpy(global_fd_name, NO_FD_PLACEHOLDER, strlen(NO_FD_PLACEHOLDER));
        print_status();
        return;
    }

    global_fd_name = safe_malloc((len + 1) * sizeof(char));
    strncpy(global_fd_name, str, len);
    global_fd_name[len] = '\0';

    reopen:
        global_fd = open(global_fd_name, O_RDWR);
        if (!isatty(global_fd) || tcsetattr(global_fd, TCSANOW, &global_config) < 0) {
            fd_err();
            return;
        }
        tcflush(global_fd, TCIFLUSH);
        print_status();
}

void try_set_baud(int baud) {
    int found = -1;
    for (int i = 0; i < BAUD_MAP_LEN; i++) {
         if (BAUD_MAP[i][0] == baud) {
             found = i;
             break;
         }
    }
    if (found < 0) {
        PRINT_ERR(BAD_BAUD_MSG);
        refresh();
        return;
    }

    if (
        cfsetispeed(&global_config, BAUD_MAP[found][1]) < 0 ||
        cfsetospeed(&global_config, BAUD_MAP[found][1]) < 0
    ) {
        mvprintw(
            FAIL_MSG_ROW,
            (COLS - strlen(BAUD_SET_FAIL_MSG) - strlen(strerror(errno)) + 1) / 2,
            "%s%s",
            BAUD_SET_FAIL_MSG,
            strerror(errno)
        );
        fd_err();
        return;
     }

     if (global_fd >= 0) try_update_fd(NULL);
     print_status();
 }

void handle_enter(char *str, Selection sel) {
    move(FAIL_MSG_ROW, 0);
    clrtoeol();

    switch (sel) {
        case FD: {
            try_update_fd(str);
            print_status();
            break;
        }
        case BAUD: {
            if (strlen(str) <= 0) break;
            try_set_baud(atoi(str));
            break;
        }
        case SEND: {
            if (global_fd < 0) {
                PRINT_ERR(SEND_NO_FD_MSG);
                print_status();
                break;
            }
            uint len = strlen(str);
            char *buffer = safe_malloc((len + 1) * sizeof(char));
            strncpy(buffer, str, len);
            int ret = write(global_fd, buffer, len);
            free(buffer);
            if (ret < 0) fd_err();
            print_status();
            break;
        }
    }
}

int main(int argc, char **argv) {
    initscr();
    clear();
    noecho();
    // nodelay(stdscr, true);
    timeout(50); // 50 ms; need some delay or an infinite loop will hog the CPU
    raw();

    global_config.c_iflag = IGNBRK | IGNPAR;
    global_config.c_cflag = CS8;
    cfsetispeed(&global_config, B115200);
    cfsetospeed(&global_config, B115200);
    global_fd_name = safe_malloc((strlen(NO_FD_PLACEHOLDER) + 1) * sizeof(char));
    strncpy(global_fd_name, NO_FD_PLACEHOLDER, strlen(NO_FD_PLACEHOLDER));
 
    NextArg next = NONE;
    for (int i = 1; i < argc; i++) {
        if (next != NONE) {
            switch (next) {
                case SET_DEVICE:
                    try_update_fd(argv[i]);
                    break;
                case SET_BAUD:
                    try_set_baud(atoi(argv[i]));
                    break;
                case SET_MODE:
                    for (int j = 0; j < NUM_MODES; j++) {
                        if (!strcmp(ARG_MODE_STR[j], argv[i])) {
                            global_mode = j;
                            goto skip;
                        }
                    }
                    PRINT_ERR(ARG_BAD_MODE_MSG);
                    skip:
                        break;
                default: // Just to keep the compiler quiet
                    break;
            }
            next = NONE;
            continue;
        }

        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            endwin();
            printf("%s", OPTS_HELP);
            free(global_fd_name);
            if (global_fd >= 0) close(global_fd);
            return 0;
        }
        if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--read"))
            global_monitoring = true;
        else if (!strcmp(argv[i], "-n") || !strcmp(argv[i], "--no-read"))
            global_monitoring = false;
        else if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--baud"))
            next = SET_BAUD;
        else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--device"))
            next = SET_DEVICE;
        else if (!strcmp(argv[i], "-m") || !strcmp(argv[i], "--mode"))
            next = SET_MODE;
        else mvprintw(
            FAIL_MSG_ROW,
            (COLS - strlen(ARG_BAD_MSG) - strlen(argv[i]) + 1) / 2,
            "%s%s",
            ARG_BAD_MSG,
            argv[i]
        );
    }
    if (next != NONE) {
        mvprintw(
            FAIL_MSG_ROW,
            (COLS - strlen(ARG_MISSING_MSG) - strlen(argv[argc - 1]) + 1) / 2,
            "%s%s",
            ARG_MISSING_MSG,
            argv[argc - 1]
        );
    }

    getmaxyx(stdscr, ROWS, COLS);
    DATA_ROWS = ROWS - DATA_START_ROW;
    DATA_ROWS -= (DATA_ROWS + 1) & 1; // Must be an odd number, or values close to 0 would be split between two rows
    GRAPH_CENTER = DATA_START_ROW + (DATA_ROWS / 2);
    GRAPH_SCALAR = (double)0x80 / ((DATA_ROWS - 1) / 2); // Effectively rounds values to the nearest row, so top and bottom rows map to half as many values as the other rows
 
    const uint max_input_len[] = {
        ((COLS - (3 * INPUT_PADDING)) / 2) + ((COLS + 1) % 2),
        ((COLS - (3 * INPUT_PADDING)) / 2),
        COLS - (2 * INPUT_PADDING)
    };
    const uint max_text_len[3] = {
        max_input_len[0] - strlen(INPUT_MSG[0]),
        max_input_len[1] - strlen(INPUT_MSG[1]),
        max_input_len[2] - strlen(INPUT_MSG[2])
    };

    const uint OFFSET[] = {
        strlen(INPUT_MSG[FD]) + 2,
        strlen(INPUT_MSG[BAUD]) + 2,
        strlen(INPUT_MSG[SEND]) + 2
    };
    const uint COL[] = {
        INPUT_PADDING,
        COLS - INPUT_PADDING - max_input_len[BAUD],
        INPUT_PADDING
    };

    char *text[3];
    for (int i = 0; i < 3; i++) {
        int len = max_text_len[i] + 1;
        if (len <= 0) {
            fprintf(stderr, "Terminal too narrow to safe_malloc an appropriate buffer");
            return 1;
        }
        text[i] = safe_malloc(len * sizeof(char));
        text[i][len - 1] = '\0';
    }
    refresh();

    WINDOW *input_box[] = {
        make_input(INPUT_ROW[FD], COL[FD], max_input_len[FD], INPUT_MSG[FD]),
        make_input(INPUT_ROW[BAUD], COL[BAUD], max_input_len[BAUD], INPUT_MSG[BAUD]),
        make_input(INPUT_ROW[SEND], COL[SEND], max_input_len[SEND], INPUT_MSG[SEND])
    };

    if (COLS > strlen(HELP_MSG)) {
        mvprintw(HELP_MSG_ROW, (COLS - strlen(HELP_MSG) + 1) / 2, "%s", HELP_MSG);
        refresh();
    }
 
    Selection sel = 0;
    uint current_len[] = {0, 0, 0};
    const uint BUFFER_SIZE = 64;
    char buffer[BUFFER_SIZE];
    int input;
    int cursor_row = 0, cursor_col = 0;
    WINDOW *data_win = NULL;

    print_status();
    while (1) {
        wmove(input_box[sel], 1, OFFSET[sel] + current_len[sel]);
        input = wgetch(input_box[sel]);
        switch (input) {
            case _CTRL('c'):
                goto exit;
            case _CTRL('x'):
                if (!global_monitoring) {
                    move(DATA_START_ROW, 0);
                    clrtobot();
                    if (global_fd >= 0) tcflush(global_fd, TCIFLUSH);
                } else {
                    delwin(data_win);
                    data_win = NULL;
                }
                global_monitoring = !global_monitoring;
                cursor_row = 0;
                cursor_col = 0;
                print_status();
                break;
            case _CTRL('z'):
                global_mode++;
                if (global_mode >= NUM_MODES) global_mode = 0;
                if (data_win) {
                    delwin(data_win);
                    data_win = NULL;
                }
                cursor_row = 0;
                cursor_col = 0;
                move(DATA_START_ROW, 0);
                clrtobot();
                print_status();
                break;

            case KEY_UP: // Using hjkl causes a conflict; Ctrl-J is the same as `\n`
            case _CTRL('w'):
                if (sel == SEND) sel = FD;
                break;
            case KEY_RIGHT:
            case _CTRL('a'):
                /*if (sel == BAUD)*/ sel = FD;
                break;
            case KEY_DOWN:
            case _CTRL('s'):
                sel = SEND;
                break;
            case KEY_LEFT:
            case _CTRL('d'):
                /*if (sel == FD)*/ sel = BAUD;
                break;

            case KEY_BACKSPACE:
            case KEY_DL:
            case 0x7F:
                if (current_len[sel] == 0) break;
                current_len[sel]--;
                text[sel][current_len[sel]] = '\0';
                wmove(input_box[sel], 1, OFFSET[sel] + current_len[sel]);
                wclrtoeol(input_box[sel]);
                box(input_box[sel], 0, 0);
                wrefresh(input_box[sel]);
                break;

            case KEY_ENTER:
            case '\n':
                handle_enter(text[sel], sel);
                break;

            case ERR:
                break;
            default:
                if (
                    current_len[sel] == max_text_len[sel] ||
                    input > 0x7F ||
                    input == _CTRL(input)
                ) break;
                text[sel][current_len[sel]] = input;
                current_len[sel]++;
                text[sel][current_len[sel]] = '\0'; // Safe due to +1 in buffer size
                waddch(input_box[sel], input);
        }

        if (!global_monitoring) continue;

        if (global_fd < 0) {
            global_monitoring = false;
            move(FAIL_MSG_ROW, 0);
            clrtoeol();
            PRINT_ERR(SEND_NO_FD_MSG);
            print_status();
            continue;
        }

        int len = read(global_fd, &buffer, BUFFER_SIZE - 1);
        if (len < 0) {
            global_monitoring = false;
            fd_err();
            print_status();
            continue;
        }
        if (len == 0) continue;

        if (global_mode == GRAPH && ROWS - DATA_START_ROW < MIN_GRAPH_ROWS) {
            too_small:
                global_monitoring = false;
                PRINT_ERR(TERMINAL_TOO_SMALL_MSG);
                print_status();
                continue;
        }
        
        if (global_mode >= HEX && !data_win) {
            if (COLS <= DATA_WIN_SIZE[global_mode - 2]) goto too_small;

            move(FAIL_MSG_ROW, 0);
            clrtoeol();
            data_win = newwin(
                DATA_ROWS,
                DATA_WIN_SIZE[global_mode - 2],
                DATA_START_ROW,
                (COLS - DATA_WIN_SIZE[global_mode - 2] + 1) / 2
            );
            wrefresh(data_win);
        }

        switch (global_mode) {
            case CHAR:
                if (cursor_row == 0) cursor_row = DATA_START_ROW;
                move(cursor_row, cursor_col);
                buffer[len] = '\0';
                for (int i = 0; i < len; i++) {
                    if (buffer[i] == '\n') {
                        addch('\n');
                        continue;
                    }
                    if (buffer[i] < 0x20 || buffer[i] >= 0x7f) {
                        printw("<0x%hhX>", buffer[i]);
                        continue;
                    }
                    addch(buffer[i]);
                }
                refresh();
                getyx(stdscr, cursor_row, cursor_col);
                if (cursor_row >= ROWS - 1 && cursor_col >= COLS - 1) {
                    global_monitoring = false;
                    print_status();
                }
                continue;
            case GRAPH:
                for (int i = 0; i < len; i++) {
                    if (cursor_col == COLS - 1) cursor_col = 0;
                    for (int i = DATA_START_ROW; i < ROWS; i++) {
                        move(i, cursor_col);
                        addch(' ');
                    }
                    cursor_row = GRAPH_CENTER - (int)round((double)buffer[i] / GRAPH_SCALAR);
                    move(cursor_row, cursor_col);
                    addch('X');
                    cursor_col++;
                }
                refresh();
                continue;
            default:
                for (int i = 0; i < len; i++) {
                    if (cursor_col >= 16) {
                        cursor_row++;
                        cursor_col = 0;
                    }
                    if (cursor_row >= DATA_ROWS) {
                        global_monitoring = false;
                        print_status();
                        break;
                    }
                    mvwprintw(
                        data_win,
                        cursor_row + 1,
                        (cursor_col * DATA_NUM_WIDTH[global_mode - 2]) + 2, // + 1 from border + 1 for padding
                        DATA_NUM_FMT[global_mode - 2],
                        buffer[i]
                    );    
                    cursor_col++;
                }
                wrefresh(data_win);
                continue;
        }
    }

    exit:
        if (global_fd >= 0) close(global_fd);
        free(global_fd_name);
        endwin();
        return 0;
}
