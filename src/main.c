#include <ugpx.h>
#include <partner/conio.h>
#include <sys/bdos.h>
#include "engine/game.h"

#define PARTNER_WIDTH  1024
#define PARTNER_HEIGHT 512

struct game_state game;

struct page_state {
    int16_t paddle1_y;
    int16_t paddle2_y;
    int16_t ball_x;
    int16_t ball_y;
    int16_t score1;
    int16_t score2;
};

static int read_key(void) {
    return (int)(unsigned char)bdos(C_RAWIO, 0xFF);
}

static void capture_page_state(struct page_state *page, const struct game_state *g) {
    page->paddle1_y = g->paddle1_y;
    page->paddle2_y = g->paddle2_y;
    page->ball_x = g->ball_x;
    page->ball_y = g->ball_y;
    page->score1 = g->score1;
    page->score2 = g->score2;
}

static void apply_page_state(struct game_state *g, const struct page_state *page) {
    g->prev_paddle1_y = page->paddle1_y;
    g->prev_paddle2_y = page->paddle2_y;
    g->prev_ball_x = page->ball_x;
    g->prev_ball_y = page->ball_y;
    g->prev_score1 = page->score1;
    g->prev_score2 = page->score2;
}

static void clear_all_pages(void) {
    gsetpage(PG_DISPLAY, 1);
    gcls();
    gsetpage(PG_DISPLAY, 0);
    gcls();
    gsetpage(PG_WRITE, 0);
}

static void draw_intro_page(struct game_state *g) {
    gsetpage(PG_WRITE, 1);
    game_render_full(g);
    gsetpage(PG_DISPLAY, 1);
    gsetpage(PG_WRITE, 1);
}

static void prepare_game_pages(struct game_state *g) {
    gsetpage(PG_WRITE, 1);
    game_render_full(g);
    game_render_draw_all(g);

    gsetpage(PG_WRITE, 0);
    gcls();
    game_render_full(g);
    game_render_draw_all(g);
}

/* Intro: draw only on page 0, then wait for ENTER/Q. */
static void run_intro(struct game_state *g) {
    draw_intro_page(g);

    while (g->screen == 0 && !g->quit) {
        int key = read_key();
        if (key)
            game_handle_key(g, key);
    }
}

static void run_game_over(struct game_state *g) {
    gsetpage(PG_DISPLAY, 0);
    gsetpage(PG_WRITE, 0);
    gcls();
    game_render_full(g);

    while (g->screen == 2 && !g->quit) {
        int key = read_key();
        if (key)
            game_handle_key(g, key);
    }
}

static void run_game(struct game_state *g) {
    struct page_state pages[2];
    int display_page = 0;

    clear_all_pages();
    prepare_game_pages(g);
    capture_page_state(&pages[0], g);
    capture_page_state(&pages[1], g);

    while (g->screen == 1 && !g->quit) {
        int key = read_key();
        int hidden_page;
        int old_display_page;
        int score_changed;
        g->prev_paddle1_y = g->paddle1_y;
        g->prev_paddle2_y = g->paddle2_y;
        g->prev_ball_x    = g->ball_x;
        g->prev_ball_y    = g->ball_y;
        g->prev_score1    = g->score1;
        g->prev_score2    = g->score2;

        if (key)
            game_handle_key(g, key);
        game_update(g);
        if (g->screen != 1 || g->quit)
            break;
        score_changed = (g->score1 != g->prev_score1 || g->score2 != g->prev_score2);

        hidden_page = 1 - display_page;
        gsetpage(PG_WRITE, hidden_page);
        apply_page_state(g, &pages[hidden_page]);
        game_render_erase(g);
        game_render_draw(g);
        if (score_changed)
            game_render_score(g);

        capture_page_state(&pages[hidden_page], g);
        old_display_page = display_page;
        gsetpage(PG_DISPLAY, hidden_page);
        display_page = hidden_page;

        if (score_changed) {
            gsetpage(PG_WRITE, old_display_page);
            game_render_score(g);
            pages[old_display_page].score1 = g->score1;
            pages[old_display_page].score2 = g->score2;
        }
    }
}

int main(void) {

    /* Hide text cursor and clear text screen. */
    setcursortype(NOCURSOR);
    clrscr();
    
    /* Init HIRES mode. */
    ginit(RES_1024x512);

    clear_all_pages();
    
    game_init(&game, PARTNER_WIDTH, PARTNER_HEIGHT);

    while (!game.quit) {
        if (game.screen == 0)
            run_intro(&game);
        else if (game.screen == 1)
            run_game(&game);
        else if (game.screen == 2)
            run_game_over(&game);
    }
    
    clear_all_pages();

    /* Exit HIRES mode.*/
    gexit();

    /* Restore text cursor. */
    setcursortype(NORMALCURSOR);

    /* Exit to CP/M. */
    return 0;
}
