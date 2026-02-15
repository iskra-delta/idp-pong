#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

struct GameState {
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
    int16_t ball_dx;
    int16_t ball_dy;
    int16_t ball_speed;

    int16_t score1;
    int16_t score2;

    uint16_t rng;
    int16_t frame;
    int16_t mode_pvc;
    int16_t screen;
    int16_t winner;
    int16_t quit;
};

void game_init(struct GameState *g, int width, int height);
void game_handle_key(struct GameState *g, int key);
void game_update(struct GameState *g);
void game_render(struct GameState *g);

#endif
