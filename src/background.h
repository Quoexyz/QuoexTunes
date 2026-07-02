#ifndef BACKGROUND_H
#define BACKGROUND_H
#include <SDL.h>

void bg_init(SDL_Renderer *ren);
void bg_cleanup(void);
void bg_load_directory(const char *dir);
SDL_Texture* bg_get_texture(void);
void bg_render_fallback(SDL_Renderer *ren, int W, int H);
int bg_get_luminance_at(int sx, int sy, int sw, int sh);
void bg_set_refresh_interval(int seconds);
int bg_get_image_count(void);
void bg_load_next_image(void);
void bg_load_initial_image(void);

#endif
