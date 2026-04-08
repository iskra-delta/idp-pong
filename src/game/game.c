#include "game.h"

#include <ugpx.h>
#include <string.h>

static int16_t read_le_i16(const uint8_t *p) {
    uint16_t u = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    return (int16_t)u;
}

static void draw_title_vectors(const struct game_state *g) {
    static const uint8_t k_escape = 0x7f;
    const uint8_t *p = vector_data;
    const uint8_t *end = vector_data + vector_data_size;
    int16_t x = 0;
    int16_t y = 0;
    int preview_w = (int)vector_width;
    int preview_h = (int)vector_height;
    int x_off = (g->width - preview_w) / 2;
    int y_off = (g->height - preview_h) / 2;

    if (x_off < 0) x_off = 0;
    if (y_off < 0) y_off = 0;

    for (;;) {
        uint8_t b;

        if (p >= end)
            break;

        b = *p++;

        if (b != k_escape) {
            int8_t dx = (int8_t)b;
            int8_t dy;

            if (p >= end)
                break;

            dy = (int8_t)*p++;
            gdrawd((coord)dx, (coord)dy);
            x = (int16_t)(x + dx);
            y = (int16_t)(y + dy);
            continue;
        }

        if (p >= end)
            break;

        b = *p++;
        if (b == 0x01)
            break;
        if (b != 0x00)
            break;

        if ((end - p) < 4)
            break;

        x = read_le_i16(p);
        y = read_le_i16(p + 2);
        p += 4;
        gxy((coord)(x + x_off), (coord)(y + y_off));
    }
}

/* -------------------------------------------------------------------------
 * Game constants
 * ------------------------------------------------------------------------- */

#define PADDLE_MARGIN 24
#define PLAYFIELD_TOP 64
#define PLAYFIELD_BOTTOM_MARGIN 16
#define NET_DASH_LEN 5
#define NET_DASH_STEP 10
#define SCORE_THICK 2
#define SCORE_SEG_LEN 22
#define SCORE_DIGIT_W (SCORE_SEG_LEN + (2 * SCORE_THICK))
#define SCORE_DIGIT_H ((3 * SCORE_THICK) + (2 * SCORE_SEG_LEN))
#define SCORE_GAP (3 * SCORE_THICK)
#define SCORE_PAIR_W ((2 * SCORE_DIGIT_W) + SCORE_GAP)
#define SCORE_Y 10
#define SCORE_LEFT_X (PARTNER_SCORE_WIDTH / 8)
#define SCORE_RIGHT_X (PARTNER_SCORE_WIDTH - (PARTNER_SCORE_WIDTH / 8) - SCORE_PAIR_W)
#define PADDLE_SPEED_LOW 2
#define PADDLE_SPEED_HIGH 3
#define WIN_SCORE     9

#define SERVE_SPEED   4
#define SPEED_STEP    1
#define MAX_SPEED     8
#define PARTNER_SCORE_WIDTH 1024

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static int paddle_half_width(const struct game_state *g) {
    int half_w = 1;
    (void)g;
    return half_w;
}

static int ball_radius(const struct game_state *g) {
    (void)g;
    return 6;
}

static int playfield_top(const struct game_state *g) {
    (void)g;
    return PLAYFIELD_TOP;
}

static int playfield_bottom(const struct game_state *g) {
    int bottom = g->height - PLAYFIELD_BOTTOM_MARGIN;
    int top = playfield_top(g);
    if (bottom <= top + 32)
        bottom = top + 32;
    return bottom;
}

static int iabs(int v) { return (v < 0) ? -v : v; }

static int clampi(int v, int min_v, int max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static uint16_t irand_next16(uint16_t x) {
    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 8);
    return x;
}

static int signum(int v) {
    if (v < 0) return -1;
    if (v > 0) return 1;
    return 0;
}

static void set_paddle_motion(int16_t *vel, int dir) {
    int cur_dir = signum(*vel);
    int speed = iabs(*vel);

    if (cur_dir == 0) {
        *vel = (int16_t)(dir * PADDLE_SPEED_LOW);
        return;
    }

    if (cur_dir == dir) {
        if (speed < PADDLE_SPEED_HIGH)
            *vel = (int16_t)(dir * PADDLE_SPEED_HIGH);
        return;
    }

    *vel = 0;
}

/* -------------------------------------------------------------------------
 * Ball / match management
 * ------------------------------------------------------------------------- */

static void set_ball_velocity(struct game_state *g, int direction, int dy) {
    int dx_sign = (direction >= 0) ? 1 : -1;
    int abs_dy  = iabs(dy);

    if (abs_dy > g->ball_speed - 1)
        abs_dy = g->ball_speed - 1;

    g->ball_dx = (int16_t)(g->ball_speed - abs_dy);
    if (g->ball_dx < 1) g->ball_dx = 1;
    g->ball_dx = (int16_t)(g->ball_dx * dx_sign);
    g->ball_dy = (int16_t)((dy < 0) ? -abs_dy : abs_dy);
}

static void reset_ball(struct game_state *g, int toward_p2) {
    int seed, slice;
    g->rng  = irand_next16((uint16_t)(g->rng + 0x4D2Bu));
    seed    = (int)g->rng;
    slice   = (seed % 7) - 3;
    g->ball_x     = g->width  / 2;
    g->ball_y     = g->height / 2;
    g->ball_speed = (int16_t)(SERVE_SPEED + ((seed >> 3) % 2));
    set_ball_velocity(g, toward_p2 ? 1 : -1, slice);
}

static void start_match(struct game_state *g) {
    g->score1 = 0;
    g->score2 = 0;
    g->prev_score1 = -1;
    g->prev_score2 = -1;
    g->paddle1_y      = g->height / 2;
    g->paddle2_y      = g->height / 2;
    g->prev_paddle1_y = g->paddle1_y;
    g->prev_paddle2_y = g->paddle2_y;
    g->paddle1_vel    = 0;
    g->paddle2_vel    = 0;
    g->winner         = 0;
    g->screen         = 1;
    reset_ball(g, 1);
    g->prev_ball_x    = g->ball_x;
    g->prev_ball_y    = g->ball_y;
}

/* -------------------------------------------------------------------------
 * Public: init
 * ------------------------------------------------------------------------- */

void game_init(struct game_state *g, int width, int height) {
    memset(g, 0, sizeof(*g));
    g->width    = (int16_t)width;
    g->height   = (int16_t)height;
    g->paddle_h = height / 8;
    if (g->paddle_h < 32) g->paddle_h = 32;
    g->paddle1_x = PADDLE_MARGIN;
    g->paddle2_x = width - PADDLE_MARGIN - 1;
    g->mode_pvc  = 1;
    g->screen    = 0;
    g->rng       = 0xACE1u;
}

/* -------------------------------------------------------------------------
 * Public: key handling  (raw ASCII from kbd_poll_key)
 * ------------------------------------------------------------------------- */

void game_handle_key(struct game_state *g, int key) {
    /* quit: Q, ESC */
    if (key == 'q' || key == 'Q' || key == 27) {
        g->quit = 1;
        return;
    }

    if (g->screen == 0) {
        if (key == '\r' || key == '\n' || key == ' ') {
            start_match(g);
        }
        return;
    }

    if (g->screen == 2) {
        if (key == '\r' || key == '\n' || key == ' ') {
            g->screen = 0;
        }
        return;
    }

    /* gameplay controls */
    if (key == 'w' || key == 'W')
        set_paddle_motion(&g->paddle1_vel, -1);
    if (key == 's' || key == 'S')
        set_paddle_motion(&g->paddle1_vel, 1);

    if (!g->mode_pvc) {
        if (key == 'i' || key == 'I' || key == '8')
            set_paddle_motion(&g->paddle2_vel, -1);
        if (key == 'k' || key == 'K' || key == '2')
            set_paddle_motion(&g->paddle2_vel, 1);
    }
}

/* -------------------------------------------------------------------------
 * AI / physics helpers
 * ------------------------------------------------------------------------- */

static void update_ai(struct game_state *g) {
    int ball_x   = g->ball_x;
    int target   = g->ball_y;
    int center   = g->paddle2_y;
    int top      = playfield_top(g);
    int bottom   = playfield_bottom(g);
    int seed;
    int distance = g->paddle2_x - ball_x;
    int noise;
    int dead_zone = 3;
    int gap;
    int step = 2;

    g->rng = irand_next16((uint16_t)(g->rng + 0x3C6Du));
    seed   = (int)g->rng;
    noise  = (seed % 7) - 3;

    if (distance < 0) distance = 0;

    target += noise;
    target = clampi(target, top + 1, bottom - 1);

    if (g->ball_dx <= 0) {
        target = (top + bottom) / 2;
        if ((g->frame % 2) == 0) return;
    } else {
        if ((g->frame % 6) == 0) return;
        if (distance < (g->width / 3) && iabs(target - center) > (g->paddle_h / 4))
            step = 3;
    }

    gap = target - center;
    if (gap >  dead_zone) g->paddle2_y += step;
    else if (gap < -dead_zone) g->paddle2_y -= step;
}

static void clamp_paddles(struct game_state *g) {
    int half = g->paddle_h / 2;
    int top = playfield_top(g);
    int bottom = playfield_bottom(g);
    g->paddle1_y = clampi(g->paddle1_y, top + half + 2, bottom - half - 2);
    g->paddle2_y = clampi(g->paddle2_y, top + half + 2, bottom - half - 2);
}

static void apply_paddle_bounce(struct game_state *g, int is_left) {
    int paddle_y   = is_left ? g->paddle1_y   : g->paddle2_y;
    int paddle_vel = is_left ? g->paddle1_vel  : g->paddle2_vel;
    int ball_y     = g->ball_y;
    int rel        = ball_y - paddle_y;
    int half       = g->paddle_h / 2;
    int max_dy     = (g->ball_speed * 3) / 4;
    int influence  = paddle_vel / 2;
    int dy;

    if (half <= 0) half = 1;

    dy = (rel * max_dy) / half;
    dy += influence;
    if (dy >  max_dy) dy =  max_dy;
    if (dy < -max_dy) dy = -max_dy;

    if (g->ball_speed < MAX_SPEED)
        g->ball_speed += SPEED_STEP;

    set_ball_velocity(g, is_left ? 1 : -1, dy);

    if (is_left)
        g->ball_x = g->paddle1_x + 1;
    else
        g->ball_x = g->paddle2_x - 1;
}

/* -------------------------------------------------------------------------
 * Public: update
 * ------------------------------------------------------------------------- */

void game_update(struct game_state *g) {
    int ball_x, ball_y, half, p_half_w, b_radius, top, bottom;

    if (g->screen != 1 || g->quit) return;

    g->frame++;

    g->paddle1_y += g->paddle1_vel;
    if (g->mode_pvc) update_ai(g);
    else g->paddle2_y += g->paddle2_vel;
    clamp_paddles(g);

    g->paddle1_vel = g->paddle1_y - g->prev_paddle1_y;
    g->paddle2_vel = g->paddle2_y - g->prev_paddle2_y;

    g->ball_x += g->ball_dx;
    g->ball_y += g->ball_dy;

    ball_x   = g->ball_x;
    ball_y   = g->ball_y;
    p_half_w = paddle_half_width(g);
    b_radius = ball_radius(g);
    top      = playfield_top(g);
    bottom   = playfield_bottom(g);

    if (ball_y <= top + 2 + b_radius) {
        g->ball_y  = (int16_t)(top + 2 + b_radius);
        g->ball_dy = (int16_t)iabs(g->ball_dy);
    }
    if (ball_y >= bottom - 2 - b_radius) {
        g->ball_y  = (int16_t)(bottom - 2 - b_radius);
        g->ball_dy = (int16_t)-iabs(g->ball_dy);
    }

    half = g->paddle_h / 2;

    if ((ball_x - b_radius) <= (g->paddle1_x + p_half_w) && g->ball_dx < 0) {
        if (ball_y >= g->paddle1_y - half - b_radius &&
            ball_y <= g->paddle1_y + half + b_radius) {
            apply_paddle_bounce(g, 1);
            g->ball_x = (int16_t)(g->paddle1_x + p_half_w + b_radius + 1);
        }
    }

    if ((ball_x + b_radius) >= (g->paddle2_x - p_half_w) && g->ball_dx > 0) {
        if (ball_y >= g->paddle2_y - half - b_radius &&
            ball_y <= g->paddle2_y + half + b_radius) {
            apply_paddle_bounce(g, 0);
            g->ball_x = (int16_t)(g->paddle2_x - p_half_w - b_radius - 1);
        }
    }

    if ((ball_x + b_radius) < 0) {
        g->score2++;
        if (g->score2 >= WIN_SCORE) { g->winner = 2; g->screen = 2; }
        else reset_ball(g, 0);
    } else if ((ball_x - b_radius) >= g->width) {
        g->score1++;
        if (g->score1 >= WIN_SCORE) { g->winner = 1; g->screen = 2; }
        else reset_ball(g, 1);
    }
}

/* -------------------------------------------------------------------------
 * Rendering helpers
 * ------------------------------------------------------------------------- */

static void draw_ball(int cx, int cy) {
    gxy((coord)(cx - 3), (coord)(cy - 6));
    gdrawd(6, 0);
    gdrawd(3, 3);
    gdrawd(0, 6);
    gdrawd(-3, 3);
    gdrawd(-6, 0);
    gdrawd(-3, -3);
    gdrawd(0, -6);
    gdrawd(3, -3);
}

static void draw_paddle(const struct game_state *g, int x, int y, int paddle_h) {
    int half = paddle_h / 2;
    int top = y - half;
    int bottom = y + half;
    int span = bottom - top;
    int x0 = x - paddle_half_width(g);
    int x1 = x + paddle_half_width(g);

    gxy((coord)x0, (coord)top);
    gdrawd(0, (coord)span);
    gxy((coord)x, (coord)top);
    gdrawd(0, (coord)span);
    gxy((coord)x1, (coord)top);
    gdrawd(0, (coord)span);
}

static void erase_paddle(const struct game_state *g, int x, int y, int paddle_h) {
    int half = paddle_h / 2;
    int top = y - half;
    int bottom = y + half;
    int span = bottom - top;
    int x0 = x - paddle_half_width(g);
    int x1 = x + paddle_half_width(g);

    (void)g;
    gxy((coord)x0, (coord)top);
    gdrawd(0, (coord)span);
    gxy((coord)x, (coord)top);
    gdrawd(0, (coord)span);
    gxy((coord)x1, (coord)top);
    gdrawd(0, (coord)span);
}

static void erase_paddle_delta(const struct game_state *g, int x, int old_y, int new_y, int paddle_h) {
    int half = paddle_h / 2;
    int old_top = old_y - half;
    int old_bottom = old_y + half;
    int dy = new_y - old_y;
    int x0 = x - paddle_half_width(g);
    int x1 = x + paddle_half_width(g);

    if (dy == 0)
        return;

    if (iabs(dy) >= paddle_h) {
        erase_paddle(g, x, old_y, paddle_h);
        return;
    }

    if (dy > 0) {
        int span = dy;
        gxy((coord)x0, (coord)old_top);
        gdrawd(0, (coord)span);
        gxy((coord)x, (coord)old_top);
        gdrawd(0, (coord)span);
        gxy((coord)x1, (coord)old_top);
        gdrawd(0, (coord)span);
    } else {
        int span = -dy;
        int start = old_bottom + dy;
        gxy((coord)x0, (coord)start);
        gdrawd(0, (coord)span);
        gxy((coord)x, (coord)start);
        gdrawd(0, (coord)span);
        gxy((coord)x1, (coord)start);
        gdrawd(0, (coord)span);
    }
}

static void draw_paddle_delta(const struct game_state *g, int x, int old_y, int new_y, int paddle_h) {
    int half = paddle_h / 2;
    int half_w = paddle_half_width(g);
    int x0 = x - half_w;
    int x1 = x + half_w;
    int old_top = old_y - half;
    int old_bottom = old_y + half;
    int new_top = new_y - half;
    int new_bottom = new_y + half;
    int dy = new_y - old_y;

    if (dy == 0)
        return;

    if (iabs(dy) >= paddle_h) {
        draw_paddle(g, x, new_y, paddle_h);
        return;
    }

    if (dy > 0) {
        gxy((coord)x0, (coord)new_bottom);
        gdrawd((coord)(x1 - x0), 0);
        if ((old_bottom) <= (new_bottom - 1)) {
            gxy((coord)x0, (coord)old_bottom);
            gdrawd(0, (coord)(new_bottom - 1 - old_bottom));
            gxy((coord)x1, (coord)old_bottom);
            gdrawd(0, (coord)(new_bottom - 1 - old_bottom));
        }
    } else {
        gxy((coord)x0, (coord)new_top);
        gdrawd((coord)(x1 - x0), 0);
        if ((new_top + 1) <= (old_top)) {
            gxy((coord)x0, (coord)(new_top + 1));
            gdrawd(0, (coord)(old_top - (new_top + 1)));
            gxy((coord)x1, (coord)(new_top + 1));
            gdrawd(0, (coord)(old_top - (new_top + 1)));
        }
    }
}

static void draw_center_net(int width, int height) {
    int y;
    int top = PLAYFIELD_TOP + 8;
    int bottom = height - PLAYFIELD_BOTTOM_MARGIN - 8;
    int cx = width / 2;

    for (y = top; y <= bottom; y += NET_DASH_STEP) {
        int y1 = y + NET_DASH_LEN;
        if (y1 > bottom)
            y1 = bottom;
        gdrawline(cx, y, cx, y1);
    }
}

static void draw_playfield_bounds(const struct game_state *g) {
    int top = playfield_top(g);
    int bottom = playfield_bottom(g);
    gdrawline(0, top, g->width - 1, top);
    gdrawline(0, bottom, g->width - 1, bottom);
}

static void draw_playfield_static(const struct game_state *g) {
    draw_playfield_bounds(g);
    draw_center_net(g->width, g->height);
}

static void redraw_center_net_span(const struct game_state *g, int y0, int y1) {
    int y;
    int top = playfield_top(g) + 8;
    int bottom = playfield_bottom(g) - 8;
    int cx = g->width / 2;

    if (y1 < y0) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }

    if (y0 < top) y0 = top;
    if (y1 > bottom) y1 = bottom;
    if (y0 > y1) return;

    for (y = top; y <= bottom; y += NET_DASH_STEP) {
        int seg0 = y;
        int seg1 = y + NET_DASH_LEN;
        int draw0, draw1;

        if (seg1 > bottom)
            seg1 = bottom;
        if (seg1 < y0 || seg0 > y1)
            continue;

        draw0 = (seg0 > y0) ? seg0 : y0;
        draw1 = (seg1 < y1) ? seg1 : y1;
        gdrawline(cx, draw0, cx, draw1);
    }
}

static void redraw_static_after_ball(const struct game_state *g, int cx, int cy, int r) {
    gsetcolor(CO_FORE);

    if (cx - r <= (g->width / 2) && cx + r >= (g->width / 2))
        redraw_center_net_span(g, cy - r - 1, cy + r + 1);
}

static int digit_segments(int digit) {
    static const int seg[10] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66,
        0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };
    if (digit < 0 || digit > 9) return 0;
    return seg[digit];
}

static void draw_big_digit(int x, int y, int digit, int seg_len, int thick) {
    int bits = digit_segments(digit);
    rect_t r;
    if (bits & 0x01) { r.x0 = x+thick;         r.y0 = y;                          r.x1 = x+thick+seg_len;         r.y1 = y+thick-1;                 gfillrect(&r); }
    if (bits & 0x02) { r.x0 = x+thick+seg_len; r.y0 = y+thick;                    r.x1 = x+thick+seg_len+thick-1; r.y1 = y+thick+seg_len-1;         gfillrect(&r); }
    if (bits & 0x04) { r.x0 = x+thick+seg_len; r.y0 = y+(2*thick)+seg_len;        r.x1 = x+thick+seg_len+thick-1; r.y1 = y+(2*thick)+(2*seg_len)-1; gfillrect(&r); }
    if (bits & 0x08) { r.x0 = x+thick;         r.y0 = y+(2*thick)+(2*seg_len);    r.x1 = x+thick+seg_len;         r.y1 = y+(3*thick)+(2*seg_len)-1; gfillrect(&r); }
    if (bits & 0x10) { r.x0 = x;               r.y0 = y+(2*thick)+seg_len;        r.x1 = x+thick-1;               r.y1 = y+(2*thick)+(2*seg_len)-1; gfillrect(&r); }
    if (bits & 0x20) { r.x0 = x;               r.y0 = y+thick;                    r.x1 = x+thick-1;               r.y1 = y+thick+seg_len-1;         gfillrect(&r); }
    if (bits & 0x40) { r.x0 = x+thick;         r.y0 = y+thick+seg_len;            r.x1 = x+thick+seg_len;         r.y1 = y+(2*thick)+seg_len-1;     gfillrect(&r); }
}

static void draw_big_score(const struct game_state *g, int s1, int s2) {
    int left_score  = s1 % 100;
    int right_score = s2 % 100;

    (void)g;

    draw_big_digit(SCORE_LEFT_X,                              SCORE_Y, (left_score  / 10) % 10, SCORE_SEG_LEN, SCORE_THICK);
    draw_big_digit(SCORE_LEFT_X + SCORE_DIGIT_W + SCORE_GAP, SCORE_Y,  left_score  % 10,       SCORE_SEG_LEN, SCORE_THICK);
    draw_big_digit(SCORE_RIGHT_X,                             SCORE_Y, (right_score / 10) % 10, SCORE_SEG_LEN, SCORE_THICK);
    draw_big_digit(SCORE_RIGHT_X + SCORE_DIGIT_W + SCORE_GAP,SCORE_Y,  right_score % 10,       SCORE_SEG_LEN, SCORE_THICK);
}

static void erase_big_score(const struct game_state *g) {
    rect_t r;

    (void)g;

    r.x0 = SCORE_LEFT_X - 1;
    r.y0 = SCORE_Y - 1;
    r.x1 = SCORE_LEFT_X + SCORE_PAIR_W;
    r.y1 = SCORE_Y + SCORE_DIGIT_H;
    gfillrect(&r);
    r.x0 = SCORE_RIGHT_X - 1;
    r.x1 = SCORE_RIGHT_X + SCORE_PAIR_W;
    gfillrect(&r);
}

static void repaint_score(const struct game_state *g) {
    gsetcolor(CO_BACK);
    erase_big_score(g);
    gsetcolor(CO_FORE);
    draw_big_score(g, g->score1, g->score2);
}

/* -------------------------------------------------------------------------
 * Public: render
 * ------------------------------------------------------------------------- */

void game_render_full(struct game_state *g) {

    gcls();
    gsetcolor(CO_FORE);

    if (g->screen == 0) {
        draw_title_vectors(g);
    } else if (g->screen == 1) {
        draw_playfield_static(g);
    }
}

void game_render_draw_all(struct game_state *g) {
    if (g->screen != 1) return;

    gsetcolor(CO_FORE);
    draw_paddle(g, g->paddle1_x, g->paddle1_y, g->paddle_h);
    draw_paddle(g, g->paddle2_x, g->paddle2_y, g->paddle_h);
    draw_ball(g->ball_x, g->ball_y);
    draw_big_score(g, g->score1, g->score2);
}

void game_render_score(struct game_state *g) {
    if (g->screen != 1) return;
    repaint_score(g);
}

void game_render_draw(struct game_state *g) {
    if (g->screen != 1) return;

    gsetcolor(CO_FORE);

    if (g->paddle1_y != g->prev_paddle1_y)
        draw_paddle_delta(g, g->paddle1_x, g->prev_paddle1_y, g->paddle1_y, g->paddle_h);
    if (g->paddle2_y != g->prev_paddle2_y)
        draw_paddle_delta(g, g->paddle2_x, g->prev_paddle2_y, g->paddle2_y, g->paddle_h);
    if (g->ball_x != g->prev_ball_x || g->ball_y != g->prev_ball_y)
        draw_ball(g->ball_x, g->ball_y);
}

void game_render_erase(struct game_state *g) {
    if (g->screen != 1) return;

    gsetcolor(CO_BACK);
    if (g->paddle1_y != g->prev_paddle1_y)
        erase_paddle_delta(g, g->paddle1_x, g->prev_paddle1_y, g->paddle1_y, g->paddle_h);
    if (g->paddle2_y != g->prev_paddle2_y)
        erase_paddle_delta(g, g->paddle2_x, g->prev_paddle2_y, g->paddle2_y, g->paddle_h);
    if (g->ball_x != g->prev_ball_x || g->ball_y != g->prev_ball_y)
        draw_ball(g->prev_ball_x, g->prev_ball_y);
    if (g->ball_x != g->prev_ball_x || g->ball_y != g->prev_ball_y)
        redraw_static_after_ball(g, g->prev_ball_x, g->prev_ball_y, ball_radius(g));
}
