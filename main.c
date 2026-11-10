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
#define AMOUNT_OF_MINES 100

#define AT(buff, x, y) buff[(y) * COLS + (x)]

char finished_game[ROWS*COLS];
char game[ROWS*COLS];

size_t mines[AMOUNT_OF_MINES];

const char MINESYM = '*';
const char BLANKSYM = '-';
const char CLEARSYM = ' ';
const char FLAGSYM = 'F';

// This struct indicates the position of the selector, that is this `[<cell>]`
struct {
    int x;
    int y;
} cursor = {0};

bool should_close = false;

unsigned int bombs_flagged = 0;
unsigned int clear_cells_revealed = 0;

void init_board()
{
    for (size_t i = 0; i < COLS; ++i) {
        for (size_t j = 0; j < ROWS; ++j) {
            finished_game[j * COLS + i] = BLANKSYM;
            game[j * COLS + i] = BLANKSYM;
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

    return (rand() % (ROWS*COLS + 1));
}

/*
 * By selector, I mean `[<cell>]`. x and y are the coordinates of the selector.
 * reset determines wether the cursor is reseted to the top position.
 */
void render(char *buff, bool reset)
{
    printf("\n");
    for (size_t i = 0; i < ROWS; ++i) {
        for (size_t j = 0; j < COLS; ++j) {
            char c = buff[i * COLS + j];
            if (cursor.x == j && cursor.y == i) {
                putchar('[');
                putchar(c);
                putchar(']');
            } else {
                putchar(' ');
                putchar(c);
                putchar(' ');
            }
        }
        putchar('\n');
    }
    if (reset) {
        printf("\x1b[%dA", ROWS + 2);
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

    char buf[255];
    char input;

    render(game, true);
    while (!should_close) {
        if (fgets(buf, sizeof(buf), stdin) != NULL) {
            input = buf[0];
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
            case 'y': // select
                if (is_pos_valid(cursor.x, cursor.y)) {
                    if (AT(finished_game, cursor.x, cursor.y) == MINESYM) {
                        render(finished_game, false);
                        printf("Game Over");
                        exit(0);
                    } else {
                        reveal_area();
                    }
                } break;
            case 'f': // flag
                if (is_pos_valid(cursor.x, cursor.y)) {
                    AT(game, cursor.x, cursor.y) = FLAGSYM;
                    if (AT(finished_game, cursor.x, cursor.y) == MINESYM) {
                        bombs_flagged++;
                    }
                } break;
            }
        }
        bool win = clear_cells_revealed + bombs_flagged == ROWS * COLS;
        render(game, !win);
        if (win) {
            printf("WIN \n");
            should_close = true;
        }
    }
    return 0;
}
