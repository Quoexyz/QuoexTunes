#include "background.h"
#include <SDL_image.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#define W 1024
#define H 600
#define MAX_IMAGES 100

static SDL_Renderer *ren = NULL;
static SDL_Texture *bg_tex = NULL;
static SDL_Surface *bg_surf = NULL;

static char image_paths[MAX_IMAGES][256];
static int image_count = 0;
static int current_image_index = 0;

static int load_images_from_directory(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return 0;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < MAX_IMAGES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char *ext = strrchr(entry->d_name, '.');
        if (ext && (strcasecmp(ext, ".bmp") == 0 || strcasecmp(ext, ".jpg") == 0 ||
                    strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".png") == 0)) {
            snprintf(image_paths[count], sizeof(image_paths[count]), "%s\\%s", dir_path, entry->d_name);
            count++;
        }
    }
    closedir(dir);
    for (int i = 0; i < count-1; i++)
        for (int j = i+1; j < count; j++)
            if (strcmp(image_paths[i], image_paths[j]) > 0) {
                char tmp[256];
                strcpy(tmp, image_paths[i]);
                strcpy(image_paths[i], image_paths[j]);
                strcpy(image_paths[j], tmp);
            }
    return count;
}

static void update_background_image(const char *image_path) {
    if (bg_tex) { SDL_DestroyTexture(bg_tex); bg_tex = NULL; }
    if (bg_surf) { SDL_FreeSurface(bg_surf); bg_surf = NULL; }
    SDL_Surface *surf = IMG_Load(image_path);
    if (!surf) return;
    bg_surf = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ARGB8888, 0);
    bg_tex = SDL_CreateTextureFromSurface(ren, bg_surf);
    SDL_FreeSurface(surf);
}

void bg_init(SDL_Renderer *renderer) {
    ren = renderer;
}

void bg_cleanup(void) {
    if (bg_tex) SDL_DestroyTexture(bg_tex);
    if (bg_surf) SDL_FreeSurface(bg_surf);
}

void bg_load_directory(const char *dir) {
    image_count = load_images_from_directory(dir);
}

SDL_Texture* bg_get_texture(void) {
    return bg_tex;
}

void bg_render_fallback(SDL_Renderer *r, int w, int h) {
    SDL_SetRenderDrawColor(r, 0xE0, 0xF2, 0xF7, 0xFF);
    SDL_Rect fb = {0, 0, w, h};
    SDL_RenderFillRect(r, &fb);
}

int bg_get_luminance_at(int sx, int sy, int sw, int sh) {
    if (!bg_surf) return 128;
    int bx = sx * bg_surf->w / W;
    int by = sy * bg_surf->h / H;
    int bw = sw * bg_surf->w / W;
    int bh = sh * bg_surf->h / H;

    if (bx < 0) bx = 0;
    if (by < 0) by = 0;
    if (bx >= bg_surf->w) bx = bg_surf->w - 1;
    if (by >= bg_surf->h) by = bg_surf->h - 1;
    if (bx + bw > bg_surf->w) bw = bg_surf->w - bx;
    if (by + bh > bg_surf->h) bh = bg_surf->h - by;
    if (bw <= 0 || bh <= 0) return 128;

    Uint32 total_lum = 0;
    int count = 0;
    Uint8 r, g, b;
    int step_x = bw > 30 ? 3 : 1;
    int step_y = bh > 30 ? 3 : 1;

    SDL_LockSurface(bg_surf);
    for (int py = by; py < by + bh; py += step_y) {
        for (int px = bx; px < bx + bw; px += step_x) {
            Uint32 pixel = *((Uint32 *)((Uint8 *)bg_surf->pixels + py * bg_surf->pitch + px * 4));
            SDL_GetRGB(pixel, bg_surf->format, &r, &g, &b);
            total_lum += (r * 299 + g * 587 + b * 114) / 1000;
            count++;
        }
    }
    SDL_UnlockSurface(bg_surf);
    if (count == 0) return 128;
    return (int)(total_lum / count);
}

void bg_set_refresh_interval(int seconds) {
    // Timer logic handled in ui.c to interact with LVGL timers properly
}

int bg_get_image_count(void) { return image_count; }

void bg_load_next_image(void) {
    if (image_count == 0) return;
    current_image_index = (current_image_index + 1) % image_count;
    update_background_image(image_paths[current_image_index]);
}

void bg_load_initial_image(void) {
    if (image_count > 0) {
        current_image_index = 0;
        update_background_image(image_paths[current_image_index]);
    }
}
