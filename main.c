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

char FINISHED_GAME[ROWS*COLS];
char GAME[ROWS*COLS];

size_t MINES[AMOUNT_OF_MINES];

const char MINESYM = '*';
const char BLANKSYM = '-';
const char CLEARSYM = ' ';

bool should_close = false;

void init_screen()
{
    for (size_t i = 0; i < COLS; ++i) {
        for (size_t j = 0; j < ROWS; ++j) {
            FINISHED_GAME[j * COLS + i] = BLANKSYM;
            GAME[j * COLS + i] = BLANKSYM;
        }
    }
}

void get_random(FILE *urandomfp, size_t *buffer)
{
    FILE *fp = urandomfp;
    if (fp == NULL) {
        fprintf(stderr, "file not found");
        exit(1);
    }

    fread(buffer, sizeof(size_t), 1, fp);
}

void render(char *buff)
{
    for (size_t i = 0; i < ROWS; ++i) {
        for (size_t j = 0; j < COLS; ++j) {
            printf("%c", buff[i * COLS + j]);
        }
        printf("\n");
    }
}

void render_with_user_selection(char *buff, int x, int y, bool reset)
{
    printf("\n");
    for (size_t i = 0; i < ROWS; ++i) {
        for (size_t j = 0; j < COLS; ++j) {
            char c = buff[i * COLS + j];
            if (x == j && y == i)
                printf("[%c]", c);
            else
                printf(" %c ", c);

        }
        printf("\n");
    }
    if (reset) {
        printf("\x1b[%dA", ROWS + 2);
    }
}

void render_then_reset_cursor(char *buff)
{
    for (size_t i = 0; i < ROWS; ++i) {
        for (size_t j = 0; j < COLS; ++j) {
            printf("%c", buff[i * COLS + j]);
        }
        printf("\n");
    }
    printf("\x1b[%dA", ROWS);
}

void place_mines()
{
    FILE *fp = fopen("/dev/urandom", "rb");
    if (fp == NULL) {
        fprintf(stderr, "FP is NULL");
        exit(1);
    }
    for (size_t i = 0; i < AMOUNT_OF_MINES; ++i) {
        size_t coord;
TRY_AGAIN:
        get_random(fp, &coord);
        coord = coord % (ROWS * COLS);

        if (FINISHED_GAME[coord] == MINESYM) {
            goto TRY_AGAIN;
        }

        FINISHED_GAME[coord] = MINESYM;
        MINES[i] = coord;
    }

    fclose(fp);
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
        if (AT(FINISHED_GAME, tmpx, tmpy) == MINESYM)
            total++;
    }
    // north
    tmpx = x;
    tmpy = y + 1;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(FINISHED_GAME, tmpx, tmpy) == MINESYM)
            total++;
    }
    // northeast
    tmpx = x + 1;
    tmpy = y + 1;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(FINISHED_GAME, tmpx, tmpy) == MINESYM)
            total++;
    }
    // east
    tmpx = x + 1;
    tmpy = y;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(FINISHED_GAME, tmpx, tmpy) == MINESYM)
            total++;
    }
    // southeast
    tmpx = x + 1;
    tmpy = y - 1;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(FINISHED_GAME, tmpx, tmpy) == MINESYM)
            total++;
    }
    // south 
    tmpx = x;
    tmpy = y - 1;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(FINISHED_GAME, tmpx, tmpy) == MINESYM)
            total++;
    }
    // southwest 
    tmpx = x - 1;
    tmpy = y - 1;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(FINISHED_GAME, tmpx, tmpy) == MINESYM)
            total++;
    }
    // west
    tmpx = x - 1;
    tmpy = y;
    if (is_pos_valid(tmpx, tmpy)) {
        if (AT(FINISHED_GAME, tmpx, tmpy) == MINESYM)
            total++;
    }

    return total;
}

void place_numbers(char *buff)
{
    for (size_t i = 0; i < COLS; ++i) {
        for (size_t j = 0; j < ROWS; ++j) {
            if (AT(buff, i, j) != MINESYM) {
                size_t mines = count_adjacent_mines(i, j);
                char sym;
                if (mines == 0) {
                    sym = BLANKSYM;
                } else {
                    sym = (char)(48 + mines);
                }
                AT(buff, i, j) = sym;
            }
        }
    }
}

void dfs(int x, int y)
{
    int visits[ROWS * COLS] = {0};
    int stackx[ROWS * COLS] = {0};
    int stacky[ROWS * COLS] = {0};
    size_t countx = 0;
    size_t county = 0;

    stackx[countx++] = x;
    stacky[county++] = y;

    while(countx > 0) {
        x = stackx[--countx];
        y = stacky[--county];
        AT(visits, x, y) = 1;

        if (AT(FINISHED_GAME, x, y) == MINESYM) continue;
        AT(GAME, x, y) = AT(FINISHED_GAME, x, y) == BLANKSYM ? CLEARSYM : AT(FINISHED_GAME, x, y);
        if (isdigit(AT(GAME, x, y))) continue;

        for (int i = -1; i < 2; ++i) {
            for (int j = -1; j < 2; ++j) {
                int nx = x + i;
                int ny = y + j;
                if (is_pos_valid(nx, ny)) {
                    if (AT(visits, nx, ny) == 0) {
                        stackx[countx++] = nx;
                        stacky[county++] = ny;
                    }
                }
            }
        }
    }
}

int main()
{
    char buf[255];
    int x = 0;
    int y = 0;
    char input;

    init_screen();
    place_mines();
    place_numbers(FINISHED_GAME);

    render_with_user_selection(GAME, x, y, true);
    while (!should_close) {
        if (fgets(buf, sizeof(buf), stdin) != NULL) {
            input = buf[0];
            switch (input) {
            case 'w':
                if (is_pos_valid(x, y - 1)) {
                    y--;
                } break;
            case 'a':
                if (is_pos_valid(x - 1, y)) {
                    x--;
                } break;
            case 's':
                if (is_pos_valid(x, y + 1)) {
                    y++;
                } break;
            case 'd':
                if (is_pos_valid(x + 1, y)) {
                    x++;
                } break;
            case 'y':
                if (is_pos_valid(x, y)) {
                    if (AT(FINISHED_GAME, x, y) == MINESYM) {
                        printf("Game Over");
                        render_with_user_selection(FINISHED_GAME, x, y, false);
                        exit(0);
                    } else {
                        dfs(x, y);
                    }
                } break;
            }
        }

        render_with_user_selection(GAME, x, y, true);
    }
    return 0;
}
