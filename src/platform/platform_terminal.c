#define _POSIX_C_SOURCE 200809L

#include "platform.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <time.h>

#define MIN_WIDTH  60
#define MIN_HEIGHT 20
#define MAX_WIDTH  220
#define MAX_HEIGHT 80

static struct termios g_old_termios;
static int g_width = 80;
static int g_height = 24;
static int g_draw_color = FORE;
static char g_framebuffer[MAX_HEIGHT][MAX_WIDTH];

static int clamp(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void put_pixel(int x, int y) {
    if (x < 0 || x >= g_width || y < 0 || y >= g_height) {
        return;
    }
    g_framebuffer[y][x] = (g_draw_color == FORE) ? '#' : ' ';
}

bool platform_init(void) {
    struct winsize ws;

    if (tcgetattr(STDIN_FILENO, &g_old_termios) != 0) {
        return false;
    }

    struct termios raw = g_old_termios;
    raw.c_lflag &= (unsigned int) ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return false;
    }

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) {
        (void) fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        g_width = clamp((int) ws.ws_col, MIN_WIDTH, MAX_WIDTH);
        g_height = clamp((int) ws.ws_row - 1, MIN_HEIGHT, MAX_HEIGHT);
    }

    platform_cls();
    printf("\x1b[2J\x1b[H\x1b[?25l");
    fflush(stdout);

    return true;
}

void platform_shutdown(void) {
    (void) tcsetattr(STDIN_FILENO, TCSANOW, &g_old_termios);
    printf("\x1b[?25h\x1b[0m\n");
    fflush(stdout);
}

void platform_cls(void) {
    int y;
    int x;

    for (y = 0; y < g_height; ++y) {
        for (x = 0; x < g_width; ++x) {
            g_framebuffer[y][x] = ' ';
        }
    }
}

void platform_set_color(int color) {
    g_draw_color = (color == BACK) ? BACK : FORE;
}

void platform_draw_line(int x0, int y0, int x1, int y1) {
    int dx = x1 - x0;
    int sx = (x0 < x1) ? 1 : -1;
    int dy = y1 - y0;
    int sy = (y0 < y1) ? 1 : -1;
    int abs_dx = (dx < 0) ? -dx : dx;
    int abs_dy = (dy < 0) ? -dy : dy;
    int err = ((abs_dx > abs_dy) ? abs_dx : -abs_dy) / 2;

    for (;;) {
        put_pixel(x0, y0);
        if (x0 == x1 && y0 == y1) {
            break;
        }

        int e2 = err;
        if (e2 > -abs_dx) {
            err -= abs_dy;
            x0 += sx;
        }
        if (e2 < abs_dy) {
            err += abs_dx;
            y0 += sy;
        }
    }
}

void platform_draw_text(int x, int y, const char *text) {
    int i;

    if (!text || y < 0 || y >= g_height) {
        return;
    }

    for (i = 0; text[i] != '\0'; ++i) {
        int px = x + i;
        if (px >= 0 && px < g_width) {
            g_framebuffer[y][px] = (g_draw_color == FORE) ? text[i] : ' ';
        }
    }
}

int platform_display_width(void) {
    return g_width;
}

int platform_display_height(void) {
    return g_height;
}

int platform_read_key(void) {
    unsigned char c;

    if (read(STDIN_FILENO, &c, 1) != 1) {
        return KEY_NONE;
    }

    if (c == 27) {
        unsigned char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
            if (seq[0] == '[') {
                if (seq[1] == 'A') {
                    return KEY_UP;
                }
                if (seq[1] == 'B') {
                    return KEY_DOWN;
                }
                if (seq[1] == 'C') {
                    return KEY_RIGHT;
                }
                if (seq[1] == 'D') {
                    return KEY_LEFT;
                }
            }
        }
        return KEY_NONE;
    }

    if (c == 'w' || c == 'W') {
        return KEY_W;
    }
    if (c == 's' || c == 'S') {
        return KEY_S;
    }
    if (c == 'q' || c == 'Q') {
        return KEY_QUIT;
    }
    if (c == ' ') {
        return KEY_SPACE;
    }
    if (c == '\r' || c == '\n') {
        return KEY_ENTER;
    }

    return KEY_NONE;
}

void platform_present(void) {
    int y;

    printf("\x1b[H");
    for (y = 0; y < g_height; ++y) {
        (void) fwrite(g_framebuffer[y], 1, (size_t) g_width, stdout);
        putchar('\n');
    }
    fflush(stdout);
}

void platform_delay(uint ms) {
    struct timespec ts;

    if (ms == 0U) {
        return;
    }

    ts.tv_sec = (time_t) (ms / 1000U);
    ts.tv_nsec = (long) (ms % 1000U) * 1000000L;
    (void) nanosleep(&ts, NULL);
}
