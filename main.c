#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    #include <windows.h>
#endif

#define ROWS 10
#define COLS 20
#define AMOUNT_OF_MINES 30

#define AT(buff, x, y) buff[(y) * COLS + (x)]

char finished_game[ROWS*COLS];
char game[ROWS*COLS];
bool has_select_any_cell_at_all = false;
bool win = false;
volatile LONG user_input = ' ';

const char MINESYM  = '*';
const char BLANKSYM = '-';
const char CLEARSYM = ' ';
const char FLAGSYM  = 'F';

typedef enum {
    RENDER_RESET = 1 << 1,
    RENDER_BLANKSYM_TO_CLEARSYM = 1 << 2,
    RENDER_DEBUG = 1 << 3,

    // TODO: implement this using threadings
    RENDER_BLINKING_CURSOR = 1 << 4,
} Render_Config; 

// This struct indicates the position of the selector, that is this `[<cell>]`
struct {
    int x;
    int y;
} cursor = {0};

unsigned int bombs_flagged = 0;
unsigned int clear_cells_revealed = 0;

void process_input(void);

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    #include <conio.h>

    char get_input(void) {
        char ch = _getch();
        if (ch == 10 || ch == 13) {
            return '\n';
        }
        return ch;
    }

    DWORD WINAPI get_input_in_another_thread(LPVOID lpParam) {
        while (1) {
            char ch = _getch();
            if (ch == 10 || ch == 13) {
                ch = '\n';
            }
            InterlockedExchange(&user_input, ch);
            process_input();
        }
        return 0;
    }
#else
    #include <termios.h>
    #include <unistd.h>

    char get_input(void) {
        char ch;
        struct termios oldt, newt;
        
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        
        newt.c_lflag &= ~(ICANON | ECHO);
        
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        
        read(STDIN_FILENO, &ch, 1);
        
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        
        if (ch == 10 || ch == 13) {
            return '\n';
        }
        return ch;
    }
#endif

void init_board()
{
    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            finished_game[i * COLS + j] = BLANKSYM;
            game[i * COLS + j] = BLANKSYM;
        }
    }
}

int get_random_coord()
{
    static bool init = false;
    if (!init) {
        init = true;
        srand(time(NULL));
    }

    return (rand() % (ROWS*COLS));
}

void color_cell(char c)
{
    putchar('\033'); putchar('[');
    switch (c) {
    case '1':
        putchar('3'); putchar('4'); // blue
        break;
    case '2':
        putchar('3'); putchar('2'); // green 
        break;
    case '3':
        putchar('3'); putchar('1'); // red
        break;
    case '4':
        putchar('9'); putchar('4'); // dark blue
        break;
    case '5':
        putchar('9'); putchar('1'); // dark red
        break;
    case '6':
        putchar('3'); putchar('6'); // teal
        break;
    case '7':
        putchar('3'); putchar('0'); // black
        break;
    case '8': case '[': case ']': case 'F':
        putchar('3'); putchar('7');
        break;
    default:
        putchar('3'); putchar('7');
    }
    putchar('m');
    putchar(c);
    putchar('\033'); putchar('['); putchar('0'); putchar('m');
}

/*
 * By selector, I mean `[<cell>]`. x and y are the coordinates of the selector.
 * reset determines wether the cursor is reseted to the top position.
 */
void render(char *buff, Render_Config config)
{
    static char cursor_op = '[';
    static char cursor_cl = ']';
    static bool blinking_cursor_toggle = false;
    if (config&RENDER_BLINKING_CURSOR) {
        cursor_op = blinking_cursor_toggle ? ' ' : '[';
        cursor_cl = blinking_cursor_toggle ? ' ' : ']';
        blinking_cursor_toggle = !blinking_cursor_toggle;
    }
    if (config&RENDER_DEBUG) {
        printf("cleared cells => %u  ", clear_cells_revealed);
        printf("flagged bombs => %u", bombs_flagged);
    }
    putchar('\n');

    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            char c = buff[i * COLS + j];
            if (config&RENDER_BLANKSYM_TO_CLEARSYM && c == '-') c = ' ';
            if (cursor.x == j && cursor.y == i) {
                putchar(cursor_op);
                color_cell(c);
                putchar(cursor_cl);
            } else if (isdigit(c)) {
                putchar(' ');
                color_cell(c);
                putchar(' ');
            } else {
                putchar(' ');
                putchar(c);
                putchar(' ');
            }
        }
        putchar('\n');
    }
    if (config&RENDER_RESET) {
        printf("\033[%dA", ROWS + 1);
    }
}

void place_mines()
{
    for (size_t i = 0; i < AMOUNT_OF_MINES; ++i) {
        int coord;
        while (1) {
            coord = get_random_coord();
            if (finished_game[coord] == MINESYM
                // do not fill the cell that the user initially chooses at the
                // start of the game with a mine.
                || cursor.x + cursor.y * COLS == coord 
            ) {
                continue;
            }
            break;
        }

        finished_game[coord] = MINESYM;
    }
}

static inline bool is_pos_valid(int x, int y)
{
    return (x >= 0 && x < COLS
        &&  y >= 0 && y < ROWS);
}

size_t count_adjacent_mines(size_t x, size_t y) 
{
    int tmpx, tmpy;
    size_t total = 0;
    // northwest
    tmpx = x - 1;
    tmpy = y + 1;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(finished_game, tmpx, tmpy) == MINESYM)
            total++;
    }
    // north
    tmpx = x;
    tmpy = y + 1;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(finished_game, tmpx, tmpy) == MINESYM)
            total++;
    }
    // northeast
    tmpx = x + 1;
    tmpy = y + 1;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(finished_game, tmpx, tmpy) == MINESYM)
            total++;
    }
    // east
    tmpx = x + 1;
    tmpy = y;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(finished_game, tmpx, tmpy) == MINESYM)
            total++;
    }
    // southeast
    tmpx = x + 1;
    tmpy = y - 1;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(finished_game, tmpx, tmpy) == MINESYM)
            total++;
    }
    // south 
    tmpx = x;
    tmpy = y - 1;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(finished_game, tmpx, tmpy) == MINESYM)
            total++;
    }
    // southwest 
    tmpx = x - 1;
    tmpy = y - 1;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(finished_game, tmpx, tmpy) == MINESYM)
            total++;
    }
    // west
    tmpx = x - 1;
    tmpy = y;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(finished_game, tmpx, tmpy) == MINESYM)
            total++;
    }

    return total;
}

void place_numbers()
{
    for (size_t i = 0; i < COLS; ++i) {
        for (size_t j = 0; j < ROWS; ++j) {
            if (AT(finished_game, i, j) != MINESYM) {
                size_t mines = count_adjacent_mines(i, j);
                char sym;
                if (mines == 0) {
                    sym = BLANKSYM;
                } else {
                    sym = (char)(48 + mines);
                }
                AT(finished_game, i, j) = sym;
            }
        }
    }
}

/*
 * flood-fill the surrounding cells of selected cell 
 */
void reveal_area()
{
    int x = cursor.x;
    int y = cursor.y;
    int visits[ROWS * COLS] = {0};
    int stackx[ROWS * COLS] = {0};
    int stacky[ROWS * COLS] = {0};
    size_t countx = 0;
    size_t county = 0;

    stackx[countx++] = x;
    stacky[county++] = y;
    AT(visits, x, y) = 1;
    
    while(countx > 0) {
        x = stackx[--countx];
        y = stacky[--county];

        if (AT(game, x, y) != BLANKSYM) continue;
        if (AT(finished_game, x, y) == MINESYM) continue;
        AT(game, x, y) = AT(finished_game, x, y) == BLANKSYM ? CLEARSYM : AT(finished_game, x, y);
        clear_cells_revealed++;
        if (isdigit(AT(game, x, y))) continue;

        for (int i = -1; i < 2; ++i) {
            for (int j = -1; j < 2; ++j) {
                int nx = x + i;
                int ny = y + j;
                if (is_pos_valid(nx, ny)) {
                    if (AT(visits, nx, ny) == 0) {
                        stackx[countx++] = nx;
                        stacky[county++] = ny;
                        AT(visits, nx, ny) = 1;
                    }
                }
            }
        }
    }
}

void process_input()
{
    switch ((char)user_input) {
    case 'w': // up
        if (is_pos_valid(cursor.x, cursor.y - 1)) {
            cursor.y--;
        } break;
    case 'a': // left
        if (is_pos_valid(cursor.x - 1, cursor.y)) {
            cursor.x--;
        } break;
    case 's': // down
        if (is_pos_valid(cursor.x, cursor.y + 1)) {
            cursor.y++;
        } break;
    case 'd': // right
        if (is_pos_valid(cursor.x + 1, cursor.y)) {
            cursor.x++;
        } break;
    case '\n': // select
        if (is_pos_valid(cursor.x, cursor.y)) {
            // Make sure that the player wouldn't click any cell for the
            // first time and get surprised by a mine.
            if (!has_select_any_cell_at_all) {
                has_select_any_cell_at_all = true;
                place_mines();
                place_numbers();
            }
            if (AT(finished_game, cursor.x, cursor.y) == MINESYM) {
                goto LOSE;
            } else {
                if (AT(game, cursor.x, cursor.y) != BLANKSYM) break;
                reveal_area();
            }
        } break;
    case ' ': // flag
        if (is_pos_valid(cursor.x, cursor.y)) {
            // toggle flag
            if (AT(game, cursor.x, cursor.y) == FLAGSYM) {
               AT(game, cursor.x, cursor.y) = BLANKSYM;
               if (AT(finished_game, cursor.x, cursor.y) == MINESYM) bombs_flagged--;
               break;
            }
            if (AT(game, cursor.x, cursor.y) != BLANKSYM) break;
            AT(game, cursor.x, cursor.y) = FLAGSYM;
            if (AT(finished_game, cursor.x, cursor.y) == MINESYM) bombs_flagged++;
        } break;
    case 'c':
        goto CLOSE;
    }

    win = (clear_cells_revealed == ROWS * COLS - AMOUNT_OF_MINES);
    if (win) goto WIN;
    return;

// There's literally no reason to use `goto`. I'm gonna be honest, I use `goto`
// because all cool programmers abuse it ;-).
WIN:
    render(game, RENDER_BLANKSYM_TO_CLEARSYM);
    printf("WIN\n");
    exit(0);
LOSE:
    render(finished_game, RENDER_BLANKSYM_TO_CLEARSYM);
    printf("Game Over\n");
    exit(0);
CLOSE:
    render(game, 0);
    exit(0);

}

int main(int argc, char **argv)
{
    HANDLE thread = CreateThread(NULL, 0, get_input_in_another_thread, NULL, 0, NULL);
    if (thread == NULL) {
        return 1;
    }

    init_board();
    unsigned int ingame_render_config = RENDER_RESET | RENDER_BLINKING_CURSOR;
    
    printf("INSTRUCTIONS\n");
    printf("\n");
    printf("    WASD     => directions\n");
    printf("    <ENTER>  => select\n");
    printf("    <SPACE>  => flag\n");
    printf("    c        => close\n");

    render(game, ingame_render_config);
    while (1) {
        render(game, ingame_render_config);
        Sleep(200);
    }

    return 0;
}
