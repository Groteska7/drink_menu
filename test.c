#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>

#define STR 25
#define COL 80

void InputMatrix(int matr[STR][COL]);
void UpdateMatrix(int matr1[STR][COL], int matr2[STR][COL]);
void ReplaceMatrix(int matr1[STR][COL], int matr2[STR][COL]);
int CountNeighbors(int matr1[STR][COL], int i, int j);
int Decision(int neighbors, int condition);
int Count(int matr[STR][COL]);
int ChangeSpeed(char control_button, int *flag, int time_mili_sec);

int main() {
    int matr1[STR][COL];
    int matr2[STR][COL];
    int time_mili_sec = 50;
    int stop = 0;

    InputMatrix(matr1);
    if (freopen("/dev/tty", "r", stdin)) initscr();
    timeout(time_mili_sec);
    noecho();
    curs_set(0);

    while (stop != 1) {
        char control_button = getch();

        if (Count(matr1) == 0) {
            stop = 1;
        }

        time_mili_sec = ChangeSpeed(control_button, &stop, time_mili_sec);
        napms(time_mili_sec);

        clear();
        UpdateMatrix(matr1, matr2);
        refresh();

        ReplaceMatrix(matr1, matr2);
    }
    endwin();
    return 0;
}

void InputMatrix(int matr[STR][COL]) {
    char ch;
    for (int i = 0; i < STR; i++) {
        for (int j = 0; j < COL; j++) {
            if (scanf(" %c", &ch) == 1) {
                matr[i][j] = ch - '0';
            } else {
                matr[i][j] = 0;
            }
        }
    }
}

void UpdateMatrix(int matr1[STR][COL], int matr2[STR][COL]) {
    for (int i = 0; i < STR; i++) {
        for (int j = 0; j < COL; j++) {
            matr2[i][j] = Decision(CountNeighbors(matr1, i, j), matr1[i][j]);
            if (matr2[i][j] == 1)
                printw("0");
            else
                printw(".");
        }
        printw("\n");
    }
    printw("(A) - faster | (Z) - slower | (Space bar) - exit");
}

void ReplaceMatrix(int matr1[STR][COL], int matr2[STR][COL]) {
    for (int i = 0; i < STR; i++) {
        for (int j = 0; j < COL; j++) {
            matr1[i][j] = matr2[i][j];
        }
    }
}

int CountNeighbors(int matr1[STR][COL], int i, int j) {
    int sum = 0;

    int up = i - 1;
    int down = i + 1;
    int left = j - 1;
    int right = j + 1;

    if (up < 0) up = STR - 1;
    if (down > STR - 1) down = down % STR;

    if (left < 0) left = COL - 1;
    if (right > COL - 1) right = right % COL;

    sum += matr1[up][left];
    sum += matr1[up][j];
    sum += matr1[up][right];
    sum += matr1[i][right];
    sum += matr1[down][right];
    sum += matr1[down][j];
    sum += matr1[down][left];
    sum += matr1[i][left];

    return sum;
}

int Decision(int neighbors, int condition) {
    int code = condition;

    if (neighbors < 2 && condition == 1) {
        code = 0;
    } else if (neighbors > 3 && condition == 1) {
        code = 0;
    } else if (neighbors == 3 && condition == 0) {
        code = 1;
    }
    return code;
}

int ChangeSpeed(char control_button, int *flag, int time_mili_sec) {
    if ((control_button == 'a' || control_button == 'A') && time_mili_sec - 10 >= 10) {
        time_mili_sec -= 10;
    } else if ((control_button == 'Z' || control_button == 'z') && time_mili_sec + 10 <= 500) {
        time_mili_sec += 10;
    } else if (control_button == ' ') {
        *flag = 1;
    }
    return time_mili_sec;
}

int Count(int matr[STR][COL]) {
    int sum = 0;
    for (int i = 0; i < STR; i++) {
        for (int j = 0; j < COL; j++) {
            sum += matr[i][j];
        }
    }
    return sum;
}
