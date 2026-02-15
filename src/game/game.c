#include "game.h"

#include "../platform/platform.h"

#include <string.h>

#define PADDLE_MARGIN 24
#define WIN_SCORE 9

#define SERVE_SPEED 5
#define SPEED_STEP 1
#define MAX_SPEED 11

static int paddle_half_width(const struct GameState *g) {
    int half_w = g->width / 512;
    if (half_w < 1) {
        half_w = 1;
    }
    if (half_w > 2) {
        half_w = 2;
    }
    return half_w;
}

static int ball_radius(const struct GameState *g) {
    int r = g->width / 320;
    if (r < 3) {
        r = 3;
    }
    if (r > 5) {
        r = 5;
    }
    return r;
}

static int iabs(int v) {
    return (v < 0) ? -v : v;
}

static int clampi(int v, int min_v, int max_v) {
    if (v < min_v) {
        return min_v;
    }
    if (v > max_v) {
        return max_v;
    }
    return v;
}

static uint16_t irand_next16(uint16_t x) {
    x ^= (uint16_t) (x << 7);
    x ^= (uint16_t) (x >> 9);
    x ^= (uint16_t) (x << 8);
    return x;
}

static void set_ball_velocity(struct GameState *g, int direction, int dy) {
    int dx_sign = (direction >= 0) ? 1 : -1;
    int abs_dy = iabs(dy);

    if (abs_dy > g->ball_speed - 1) {
        abs_dy = g->ball_speed - 1;
    }

    g->ball_dx = (int16_t) (g->ball_speed - abs_dy);
    if (g->ball_dx < 1) {
        g->ball_dx = 1;
    }

    g->ball_dx = (int16_t) (g->ball_dx * dx_sign);
    g->ball_dy = (int16_t) ((dy < 0) ? -abs_dy : abs_dy);
}

static void reset_ball(struct GameState *g, int toward_p2) {
    int seed;
    int slice;

    g->rng = irand_next16((uint16_t) (g->rng + 0x4D2Bu));
    seed = (int) g->rng;
    slice = (seed % 7) - 3;

    g->ball_x = g->width / 2;
    g->ball_y = g->height / 2;
    g->ball_speed = (int16_t) (SERVE_SPEED + ((seed >> 3) % 2));

    set_ball_velocity(g, toward_p2 ? 1 : -1, slice);
}

static void start_match(struct GameState *g) {
    g->score1 = 0;
    g->score2 = 0;
    g->paddle1_y = g->height / 2;
    g->paddle2_y = g->height / 2;
    g->prev_paddle1_y = g->paddle1_y;
    g->prev_paddle2_y = g->paddle2_y;
    g->paddle1_vel = 0;
    g->paddle2_vel = 0;
    g->winner = 0;
    g->screen = 1;

    reset_ball(g, 1);
}

void game_init(struct GameState *g, int width, int height) {
    memset(g, 0, sizeof(*g));

    g->width = (int16_t) width;
    g->height = (int16_t) height;

    g->paddle_h = height / 8;
    if (g->paddle_h < 32) {
        g->paddle_h = 32;
    }

    g->paddle1_x = PADDLE_MARGIN;
    g->paddle2_x = width - PADDLE_MARGIN - 1;
    g->mode_pvc = 1;
    g->screen = 0;
    g->rng = 0xACE1u;
}

void game_handle_key(struct GameState *g, int key) {
    int paddle_step = g->height / 40;
    if (paddle_step < 6) {
        paddle_step = 6;
    }

    if (key == KEY_QUIT) {
        g->quit = 1;
        return;
    }

    if (g->screen == 0) {
        if (key == KEY_LEFT || key == KEY_RIGHT) {
            g->mode_pvc = !g->mode_pvc;
        }
        if (key == KEY_ENTER || key == KEY_SPACE) {
            start_match(g);
        }
        return;
    }

    if (g->screen == 2) {
        if (key == KEY_ENTER || key == KEY_SPACE) {
            g->screen = 0;
        }
        return;
    }

    if (key == KEY_W) {
        g->paddle1_y -= paddle_step;
    }
    if (key == KEY_S) {
        g->paddle1_y += paddle_step;
    }

    if (!g->mode_pvc) {
        if (key == KEY_UP) {
            g->paddle2_y -= paddle_step;
        }
        if (key == KEY_DOWN) {
            g->paddle2_y += paddle_step;
        }
    }
}

static void update_ai(struct GameState *g) {
    int ball_x = g->ball_x;
    int target = g->ball_y;
    int center = g->paddle2_y;
    int seed;
    int distance = g->paddle2_x - ball_x;
    int noise;
    int dead_zone = 1;
    int gap;
    int step = 2;

    g->rng = irand_next16((uint16_t) (g->rng + 0x3C6Du));
    seed = (int) g->rng;
    noise = (seed % 5) - 2;

    if (distance < 0) {
        distance = 0;
    }

    target += noise;
    target = clampi(target, 1, g->height - 2);

    if (g->ball_dx <= 0) {
        /* Re-center lazily while ball is moving away. */
        target = g->height / 2;
        if ((g->frame % 2) != 0) {
            return;
        }
    } else {
        /* Keep some imperfection, but react more often. */
        if ((g->frame % 8) == 0) {
            return;
        }
        if (distance < (g->width / 3) && iabs(target - center) > (g->paddle_h / 3)) {
            step = 3;
        }
    }

    gap = target - center;
    if (gap > dead_zone) {
        g->paddle2_y += step;
    } else if (gap < -dead_zone) {
        g->paddle2_y -= step;
    }
}

static void clamp_paddles(struct GameState *g) {
    int half = g->paddle_h / 2;

    g->paddle1_y = clampi(g->paddle1_y, half, g->height - 1 - half);
    g->paddle2_y = clampi(g->paddle2_y, half, g->height - 1 - half);
}

static void apply_paddle_bounce(struct GameState *g, int is_left) {
    int paddle_y = is_left ? g->paddle1_y : g->paddle2_y;
    int paddle_vel = is_left ? g->paddle1_vel : g->paddle2_vel;
    int ball_y = g->ball_y;
    int rel = ball_y - paddle_y;
    int half = g->paddle_h / 2;
    int max_dy = (g->ball_speed * 3) / 4;
    int influence = paddle_vel / 2;
    int dy;

    if (half <= 0) {
        half = 1;
    }

    dy = (rel * max_dy) / half;
    dy += influence;

    if (dy > max_dy) {
        dy = max_dy;
    }
    if (dy < -max_dy) {
        dy = -max_dy;
    }

    if (g->ball_speed < MAX_SPEED) {
        g->ball_speed += SPEED_STEP;
    }

    set_ball_velocity(g, is_left ? 1 : -1, dy);

    if (is_left) {
        g->ball_x = g->paddle1_x + 1;
    } else {
        g->ball_x = g->paddle2_x - 1;
    }
}

void game_update(struct GameState *g) {
    int ball_x;
    int ball_y;
    int half;
    int p_half_w;
    int b_radius;

    if (g->screen != 1 || g->quit) {
        return;
    }

    g->frame++;

    g->prev_paddle1_y = g->paddle1_y;
    g->prev_paddle2_y = g->paddle2_y;

    if (g->mode_pvc) {
        update_ai(g);
    }

    clamp_paddles(g);

    g->paddle1_vel = g->paddle1_y - g->prev_paddle1_y;
    g->paddle2_vel = g->paddle2_y - g->prev_paddle2_y;

    g->ball_x += g->ball_dx;
    g->ball_y += g->ball_dy;

    ball_x = g->ball_x;
    ball_y = g->ball_y;
    p_half_w = paddle_half_width(g);
    b_radius = ball_radius(g);

    if (ball_y <= 1 + b_radius) {
        g->ball_y = (int16_t) (1 + b_radius);
        g->ball_dy = (int16_t) iabs(g->ball_dy);
    }
    if (ball_y >= (g->height - 2) - b_radius) {
        g->ball_y = (int16_t) ((g->height - 2) - b_radius);
        g->ball_dy = (int16_t) -iabs(g->ball_dy);
    }

    half = g->paddle_h / 2;

    if ((ball_x - b_radius) <= (g->paddle1_x + p_half_w) && g->ball_dx < 0) {
        if (ball_y >= g->paddle1_y - half - b_radius && ball_y <= g->paddle1_y + half + b_radius) {
            apply_paddle_bounce(g, 1);
            g->ball_x = (int16_t) (g->paddle1_x + p_half_w + b_radius + 1);
        }
    }

    if ((ball_x + b_radius) >= (g->paddle2_x - p_half_w) && g->ball_dx > 0) {
        if (ball_y >= g->paddle2_y - half - b_radius && ball_y <= g->paddle2_y + half + b_radius) {
            apply_paddle_bounce(g, 0);
            g->ball_x = (int16_t) (g->paddle2_x - p_half_w - b_radius - 1);
        }
    }

    if ((ball_x + b_radius) < 0) {
        g->score2++;
        if (g->score2 >= WIN_SCORE) {
            g->winner = 2;
            g->screen = 2;
        } else {
            reset_ball(g, 0);
        }
    } else if ((ball_x - b_radius) >= g->width) {
        g->score1++;
        if (g->score1 >= WIN_SCORE) {
            g->winner = 1;
            g->screen = 2;
        } else {
            reset_ball(g, 1);
        }
    }
}

static void draw_filled_rect(int x0, int y0, int x1, int y1) {
    int y;

    if (x1 < x0) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y1 < y0) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }

    for (y = y0; y <= y1; ++y) {
        platform_draw_line(x0, y, x1, y);
    }
}

static void draw_circle_filled(int cx, int cy, int r) {
    int x = 0;
    int y = r;
    int d = 3 - (2 * r);

    while (y >= x) {
        platform_draw_line(cx - x, cy - y, cx + x, cy - y);
        platform_draw_line(cx - y, cy - x, cx + y, cy - x);
        platform_draw_line(cx - y, cy + x, cx + y, cy + x);
        platform_draw_line(cx - x, cy + y, cx + x, cy + y);

        x++;
        if (d > 0) {
            y--;
            d += 4 * (x - y) + 10;
        } else {
            d += 4 * x + 6;
        }
    }
}

static void draw_paddle(const struct GameState *g, int x, int y, int paddle_h) {
    int half = paddle_h / 2;
    int half_w = paddle_half_width(g);

    draw_filled_rect(x - half_w, y - half, x + half_w, y + half);
}

static void draw_center_net(int width, int height) {
    int y;
    int cx = width / 2;

    for (y = 16; y < height - 16; y += 10) {
        platform_draw_line(cx, y, cx, y + 5);
    }
}

static int digit_segments(int digit) {
    static const int seg[10] = {
        0x3F, /*0*/
        0x06, /*1*/
        0x5B, /*2*/
        0x4F, /*3*/
        0x66, /*4*/
        0x6D, /*5*/
        0x7D, /*6*/
        0x07, /*7*/
        0x7F, /*8*/
        0x6F  /*9*/
    };

    if (digit < 0 || digit > 9) {
        return 0;
    }
    return seg[digit];
}

static void draw_big_digit(int x, int y, int digit, int seg_len, int thick) {
    int bits = digit_segments(digit);

    /* bit mapping: 0=A,1=B,2=C,3=D,4=E,5=F,6=G */
    if (bits & 0x01) {
        draw_filled_rect(x + thick, y, x + thick + seg_len, y + thick - 1);
    }
    if (bits & 0x02) {
        draw_filled_rect(x + thick + seg_len, y + thick, x + thick + seg_len + thick - 1, y + thick + seg_len - 1);
    }
    if (bits & 0x04) {
        draw_filled_rect(x + thick + seg_len, y + (2 * thick) + seg_len, x + thick + seg_len + thick - 1, y + (2 * thick) + (2 * seg_len) - 1);
    }
    if (bits & 0x08) {
        draw_filled_rect(x + thick, y + (2 * thick) + (2 * seg_len), x + thick + seg_len, y + (3 * thick) + (2 * seg_len) - 1);
    }
    if (bits & 0x10) {
        draw_filled_rect(x, y + (2 * thick) + seg_len, x + thick - 1, y + (2 * thick) + (2 * seg_len) - 1);
    }
    if (bits & 0x20) {
        draw_filled_rect(x, y + thick, x + thick - 1, y + thick + seg_len - 1);
    }
    if (bits & 0x40) {
        draw_filled_rect(x + thick, y + thick + seg_len, x + thick + seg_len, y + (2 * thick) + seg_len - 1);
    }
}

static void draw_big_score(const struct GameState *g) {
    int scale = g->width / 512;
    int thick;
    int seg_len;
    int digit_w;
    int pair_w;
    int gap;
    int y;
    int left_x;
    int right_x;
    int left_score = g->score1 % 100;
    int right_score = g->score2 % 100;

    if (scale < 2) {
        scale = 2;
    }
    if (scale > 3) {
        scale = 3;
    }

    thick = scale + 1;
    seg_len = 9 * scale;
    digit_w = seg_len + (2 * thick);
    gap = 3 * thick;
    pair_w = (2 * digit_w) + gap;
    y = 12;
    left_x = g->width / 8;
    right_x = g->width - (g->width / 8) - pair_w;

    draw_big_digit(left_x, y, (left_score / 10) % 10, seg_len, thick);
    draw_big_digit(left_x + digit_w + gap, y, left_score % 10, seg_len, thick);
    draw_big_digit(right_x, y, (right_score / 10) % 10, seg_len, thick);
    draw_big_digit(right_x + digit_w + gap, y, right_score % 10, seg_len, thick);
}

static int text_pixel_width(const char *text) {
    int len = (int) strlen(text);
    return len * 6;
}

static int text2x_pixel_width(const char *text) {
    int len = (int) strlen(text);
    return len * 12;
}

static void draw_text_centered(const struct GameState *g, int y, const char *text) {
    int x = (g->width - text_pixel_width(text)) / 2;
    platform_draw_text(x, y, text);
}

static void draw_text2x_centered(const struct GameState *g, int y, const char *text) {
    int x = (g->width - text2x_pixel_width(text)) / 2;
    platform_draw_text2x(x, y, text);
}

void game_render(struct GameState *g) {
    platform_set_color(BACK);
    platform_cls();
    platform_set_color(FORE);

    if (g->screen == 0) {
        int block_h = 5 * 14 + 4 * 22;
        int top = (g->height - block_h) / 2;
        draw_text2x_centered(g, top, "ISKRA DELTA PONG");
        draw_text2x_centered(g, top + 36, "LEVO/DESNO: IZBIRA NACINA");
        draw_text2x_centered(g, top + 72, "ENTER/PRESLEDEK: ZACNI  Q: IZHOD");

        if (g->mode_pvc) {
            draw_text2x_centered(g, top + 108, "NACIN: IGRALEC PROTI RACUNALNIKU");
        } else {
            draw_text2x_centered(g, top + 108, "NACIN: IGRALEC PROTI IGRALCU");
        }

        draw_text2x_centered(g, top + 144, "P1 TIPKE: W/S   P2 TIPKE: GOR/DOL");
        return;
    }

    if (g->screen == 2) {
        int top = g->height / 3;
        if (g->winner == 1) {
            draw_text_centered(g, top, "ZMAGA IGRALEC 1");
        } else {
            draw_text_centered(g, top, "ZMAGA IGRALEC 2");
        }
        draw_text_centered(g, top + 28, "PRITISNI ENTER ALI PRESLEDEK");
        draw_text_centered(g, top + 48, "ZA VRNITEV V MENI");
        return;
    }

    draw_center_net(g->width, g->height);
    draw_paddle(g, g->paddle1_x, g->paddle1_y, g->paddle_h);
    draw_paddle(g, g->paddle2_x, g->paddle2_y, g->paddle_h);
    draw_circle_filled(g->ball_x, g->ball_y, ball_radius(g));
    draw_big_score(g);
}
