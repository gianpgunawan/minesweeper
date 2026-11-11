#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#define ROWS 20
#define COLS 30
#define AMOUNT_OF_MINES 60

#define AT(buff, x, y) buff[(y) * COLS + (x)]

char finished_game[ROWS*COLS];
char game[ROWS*COLS];

size_t mines[AMOUNT_OF_MINES];

const char MINESYM = '*';
const char BLANKSYM = '-';
const char CLEARSYM = ' ';
const char FLAGSYM = 'F';

typedef enum {
    RENDER_RESET = 1 << 1,
    RENDER_BLANKSYM_TO_CLEARSYM = 1 << 2
} Render_Config; 

// This struct indicates the position of the selector, that is this `[<cell>]`
struct {
    unsigned int x;
    unsigned int y;
} cursor = {0};

unsigned int bombs_flagged = 0;
unsigned int clear_cells_revealed = 0;

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    #include <conio.h>

    char get_input(void) {
        char ch = _getch();
        if (ch == 10 || ch == 13) {
            return '\n';
        }
        return ch;
    }

#else
    #include <termios.h>

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
    putchar('\033');
    putchar('[');
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
    case '8':
    case '[':
    case ']':
    case 'F':
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
    putchar('\n');
    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            char c = buff[i * COLS + j];
            if (config&RENDER_BLANKSYM_TO_CLEARSYM && c == '-') c = ' ';
            if (cursor.x == j && cursor.y == i) {
                putchar('[');
                color_cell(c);
                putchar(']');
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
        printf("\x1b[%dA", ROWS + 1);
    }
}

void place_mines()
{
    for (size_t i = 0; i < AMOUNT_OF_MINES; ++i) {
        int coord;
TRY_AGAIN:
        coord = get_random_coord();

        if (finished_game[coord] == MINESYM) {
            goto TRY_AGAIN;
        }

        finished_game[coord] = MINESYM;
        mines[i] = coord;
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

int main()
{
    init_board();
    place_mines();
    place_numbers();
    
    printf("INSTRUCTIONS\n");
    printf("\n");
    printf("    WASD     => directions\n");
    printf("    <ENTER>  => select\n");
    printf("    <SPACE>  => flag\n");
    printf("    c        => close\n");
    render(game, RENDER_RESET);
    char input = ' ';
    while (1) {
        input = get_input();
        switch (input) {
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
                if (AT(finished_game, cursor.x, cursor.y) == MINESYM) {
                    goto LOSE;
                } else {
                    reveal_area();
                }
            } break;
        case ' ': // flag
            if (is_pos_valid(cursor.x, cursor.y)) {
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

        bool win = bombs_flagged == AMOUNT_OF_MINES;
        win = win || clear_cells_revealed == ROWS * COLS - AMOUNT_OF_MINES;
        if (win) goto WIN;
        
        render(game, RENDER_RESET);
    }
// There's literally no reason to use `goto`. I'm gonna be honest, I use `goto`
// because all cool programmers abuse it ;-).
WIN:
    render(game, RENDER_BLANKSYM_TO_CLEARSYM);
    printf("WIN\n");
    return 0;
LOSE:
    render(finished_game, RENDER_BLANKSYM_TO_CLEARSYM);
    printf("Game Over\n");
    return 0;
CLOSE:
    render(game, RENDER_BLANKSYM_TO_CLEARSYM);
    return 0;

    return 0;
}
