#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_image.h>
#include <lvgl.h>
#include <time.h>
#include "player.h"
#include "background.h"
#include <stdio.h>
#include "ui.h"

#define W 1024
#define H 600

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static int mouse_x = 0, mouse_y = 0;
static bool mouse_pressed = false;
static volatile bool need_render = false;

static void flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    int w = lv_area_get_width(area);
    int h = lv_area_get_height(area);
    SDL_Rect r = {area->x1, area->y1, w, h};
    SDL_UpdateTexture(tex, &r, px_map, w * 4);
    lv_display_flush_ready(disp);
    need_render = true;
}

static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

int main(void) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    int flags = IMG_INIT_JPG | IMG_INIT_PNG;
    if ((IMG_Init(flags) & flags) != flags) {
        printf("IMG_Init failed: %s\n", IMG_GetError());
    }

    player_init();

    win = SDL_CreateWindow("AuraTunes", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    srand((unsigned)time(NULL));

    bg_init(ren);
    bg_load_directory("C:\\Users\\User\\Desktop\\image");
    bg_load_initial_image();

    player_load_songs("C:\\Users\\User\\Desktop\\music");

    lv_init();
    static lv_color_t buf1[W * 40];
    static lv_color_t buf2[W * 40];
    lv_display_t *disp = lv_display_create(W, H);
    lv_display_set_default(disp);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpad_read);

    ui_init();
    lv_obj_update_layout(lv_screen_active());
    ui_update_text_colors();

    uint32_t last = SDL_GetTicks();
    while (1) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) goto cleanup;
            if (e.type == SDL_MOUSEMOTION) { mouse_x = e.motion.x; mouse_y = e.motion.y; }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) mouse_pressed = true;
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) mouse_pressed = false;
        }

        uint32_t now = SDL_GetTicks();
        lv_tick_inc(now - last);
        last = now;

        need_render = false;
        lv_timer_handler();

        if (need_render) {
            SDL_Texture *bg = bg_get_texture();
            if (bg) {
                SDL_RenderCopy(ren, bg, NULL, NULL);
            } else {
                bg_render_fallback(ren, W, H);
            }
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren);
        } else {
            SDL_Delay(5);
        }
    }

cleanup:
    player_cleanup();
    bg_cleanup();
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    IMG_Quit();
    return 0;
}
