#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

struct game_state {
    int16_t width;
    int16_t height;

    int16_t paddle_h;
    int16_t paddle1_x;
    int16_t paddle2_x;
    int16_t paddle1_y;
    int16_t paddle2_y;

    int16_t prev_paddle1_y;
    int16_t prev_paddle2_y;
    int16_t paddle1_vel;
    int16_t paddle2_vel;

    int16_t ball_x;
    int16_t ball_y;
    int16_t prev_ball_x;
    int16_t prev_ball_y;
    int16_t ball_dx;
    int16_t ball_dy;
    int16_t ball_speed;

    int16_t score1;
    int16_t score2;
    int16_t prev_score1;
    int16_t prev_score2;

    uint16_t rng;
    int16_t frame;
    int16_t p1_last_input_frame;
    int16_t p2_last_input_frame;
    int16_t p1_last_input_dir;
    int16_t p2_last_input_dir;
    int16_t mode_pvc;
    int16_t screen;
    int16_t winner;
    int16_t quit;
};

extern uint8_t vector_data[];
extern uint16_t vector_data_size;
extern uint16_t vector_width;
extern uint16_t vector_height;

void game_init(struct game_state *g, int width, int height);
void game_handle_key(struct game_state *g, int key);
void game_update(struct game_state *g);
void game_render_full(struct game_state *g);   /* full clear + static content for the current page */
void game_render_draw_all(struct game_state *g); /* draw all moving elements for initial page setup */
void game_render_score(struct game_state *g);  /* repaint score on the current write page */
void game_render_draw(struct game_state *g);   /* draw moving elements at current positions */
void game_render_erase(struct game_state *g);  /* erase moving elements at previous positions */

#endif
