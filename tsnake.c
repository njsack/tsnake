#include <ncurses.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define INITIAL_DELAY 70000
#define SPEED_STEP 5000
#define LEVEL_UP_SCORE 10
#define STATUS_HEIGHT 4

typedef struct snakeSegment {
    int x, y;
    struct snakeSegment *next;
} snakeSegment;

snakeSegment *snake = NULL;
int termWidth, termHeight;
int dirX = 1, dirY = 0;
int foodX, foodY;
int gameOver = 0, gamePaused = 0;
int score = 0, highScore = 0;
int delay = INITIAL_DELAY;

/* function declarations */

void init();
void reset();
void cleanup();
void addSegment(int x, int y);
void removeTail();
void placeFood();
void update();
void draw();
void drawStatus();
void input();
void gameOverScreen();
void saveHighScore();
void loadHighScore();
char* getScorePath();
int collision(int x, int y);
int quitting = 0;

/* snake functions */

void addSegment(int x, int y) {
    snakeSegment *segment = malloc(sizeof(snakeSegment));

    if (!segment) {
        endwin();
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }

    segment->x = x;
    segment->y = y;
    segment->next = snake;
    snake = segment;
}

void removeTail() {
    snakeSegment *cur = snake;
    if (!cur || !cur->next) return;

    while (cur->next && cur->next->next)
        cur = cur->next;

    free(cur->next);
    cur->next=NULL;
}

int collision(int x, int y) {
    if (x < 0 || x >= termWidth || y < 0 || y >= termHeight -
        STATUS_HEIGHT)
        return 1;
    for (snakeSegment *s = snake; s; s = s->next)
        if (s->x == x && s->y == y)
            return 1;
    return 0;
}

void placeFood() {
    do {
        foodX = rand() % termWidth;
        foodY = rand() % termHeight - (STATUS_HEIGHT + 1);
    } while (collision(foodX, foodY));
}

/* game logic */

void update() {
    int newX = snake->x + dirX;
    int newY = snake->y + dirY;

    if (collision(newX, newY)) {
        gameOver = 1;
        return;
    }

    if (newX == foodX && newY == foodY) {
        addSegment(newX, newY);
        score++;
        if (score % LEVEL_UP_SCORE == 0 && delay > 50000)
            delay -= SPEED_STEP;
        placeFood();
    } else {
        addSegment(newX, newY);
        removeTail();
    }
}

void draw() {
    erase();

    mvaddch(0, 0, ACS_ULCORNER);
    mvhline(0, 1, ACS_HLINE, termWidth);
    mvaddch(0, termWidth + 1, ACS_URCORNER);

    for (int y = 1; y < termHeight - (STATUS_HEIGHT - 1); y++) {
        mvaddch(y, 0, ACS_VLINE);
        mvaddch(y, termWidth + 1, ACS_VLINE);
    }

    mvprintw(0, 2, " Score: %d  High: %d %s", score, highScore,
        gamePaused ? "[PAUSED] " : "");

    mvaddch(foodY + 1, foodX + 1, 'O');

    for (snakeSegment *s = snake; s; s = s->next) {
        mvaddch(s->y + 1, s->x + 1, '#');
    }

    drawStatus();
    refresh();
}

void drawStatus() {
    int level = score / LEVEL_UP_SCORE + 1;
    int progress = score % LEVEL_UP_SCORE;

    int bar_width = termWidth;
    int filled = (progress * bar_width) / LEVEL_UP_SCORE;

    int status_y = termHeight - (STATUS_HEIGHT - 1);

    mvaddch(status_y, 0, ACS_LTEE);
    mvhline(status_y, 1, ACS_HLINE, termWidth);
    mvaddch(status_y, termWidth + 1, ACS_RTEE);

    mvaddch(status_y + 1, 0, ACS_VLINE);
    mvaddch(status_y + 1, termWidth + 1, ACS_VLINE);

    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            attron(COLOR_PAIR(1));
            mvaddch(status_y + 1, 1 + i, ACS_BLOCK);
            attroff(COLOR_PAIR(1));
        } else {
            mvaddch(status_y + 2, 1 + i, ' ');
        }
    }

    mvaddch(status_y + 2, 0, ACS_LLCORNER);
    mvhline(status_y + 2, 1, ACS_HLINE, termWidth);
    mvaddch(status_y + 2, termWidth + 1, ACS_LRCORNER);

    mvprintw(status_y, 2, " Speed Level: %d ", level);
}

void input() {
    int ch = getch();
    switch (ch) {
        case 'p':
        case 'P':
            gamePaused = !gamePaused;
            break;
        case 'q':
        case 'Q': quitting = 1; gameOver = 1; break;
        case KEY_UP: if (dirY == 0) { dirX = 0; dirY = -1; } break;
        case KEY_DOWN: if (dirY == 0) { dirX = 0; dirY = 1; } break;
        case KEY_LEFT: if (dirX == 0) { dirX = -1; dirY = 0; } break;
        case KEY_RIGHT: if (dirX == 0) { dirX = 1; dirY = 0; } break;
    }
}

/* high score persistence */

char* getScorePath() {
    static char path[256];
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    snprintf(path, sizeof(path), "%s/.tsnake_score", home);
    return path;
}

void loadHighScore() {
    FILE *f = fopen(getScorePath(), "r");
    if (f) {
        fscanf(f, "%d", &highScore);
        fclose(f);
    }
}

void saveHighScore() {
    if (score > highScore) {
        FILE *f = fopen(getScorePath(), "w");
        if (f) {
            fprintf(f, "%d\n", score);
            fclose(f);
        }
    }
}

void gameOverScreen() {
    nodelay(stdscr, FALSE);

    mvprintw(termHeight / 2, (termWidth - 17) / 2, "*** GAME OVER ***");
    mvprintw(termHeight / 2 + 1, (termWidth - 35) / 2,
        "Press 'r' to restart or 'q' to quit");

    saveHighScore();
    refresh();

    int ch;
    while ((ch = getch())) {
        if (ch == 'r' || ch == 'R') {
            reset();
            return;
        } else if (ch == 'q' || ch == 'Q') {
            quitting = 1;
            break;
        }
    }
}

void cleanup() {
    while (snake) {
        snakeSegment *tmp = snake;
        snake = snake->next;
        free(tmp);
    }
}

void reset() {
    cleanup();

    dirX = 1;
    dirY = 0;
    score = 0;
    delay = INITIAL_DELAY;
    gamePaused = 0;
    gameOver = 0;

    // Re-fetch size, but DO NOT subtract from termHeight
    getmaxyx(stdscr, termHeight, termWidth);
    termWidth -= 2;

    addSegment(termWidth / 2, (termHeight - STATUS_HEIGHT) / 2);
    placeFood();

    nodelay(stdscr, TRUE);
    clear();
    refresh();
}

void init() {
    srand(time(NULL));
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE);

    /* if (termHeight < 10 || termWidth < 20) {
        endwin();
        fprintf(stderr, "Terminal window too small.\n");
        exit(1);
    } */

    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);

    loadHighScore();

    getmaxyx(stdscr, termHeight, termWidth);
    termWidth -= 2;

    reset();
}

int main() {
    init();

    while (!quitting) {
        input();

        if (!gamePaused && !gameOver) {
            update();
            draw();
            usleep(delay);
        } else if (gameOver) {
            saveHighScore();
            draw();
            if (!quitting) {
                gameOverScreen();
            }
        }
    }

    endwin();
    cleanup();
    return 0;
}
