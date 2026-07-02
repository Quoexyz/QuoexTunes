#include "ui.h"
#include "player.h"
#include "background.h"
#include "utils.h"
#include <stdio.h>
#include <time.h>

static lv_obj_t *clock_label   = NULL;
static lv_obj_t *title_label_g = NULL;
static lv_obj_t *artist_label_g = NULL;
static lv_obj_t *curr_time_g   = NULL;
static lv_obj_t *total_time_g  = NULL;
static lv_obj_t *weather_label_g = NULL;
static lv_obj_t *refresh_label = NULL;
static lv_obj_t *play_icon_g   = NULL;
static lv_obj_t *progress_bar_g = NULL;
static lv_obj_t *mode_icon_g   = NULL;
static lv_obj_t *vol_icon_g    = NULL;

static lv_obj_t *playlist_list = NULL;
static lv_obj_t *search_box_g  = NULL;
static lv_obj_t *clear_btn_g   = NULL;
static lv_obj_t *selected_btn  = NULL;
static lv_obj_t *kb            = NULL;
static lv_obj_t *vol_popup     = NULL;

static lv_obj_t *lyric_curr_g  = NULL;
static lv_obj_t *lyric_next_g  = NULL;


static bool user_seeking = false;
static int refresh_interval = 0;
static lv_timer_t *image_timer = NULL;

/* ---------- Forward declarations ---------- */
static void search_cb(lv_event_t *e);
static void update_mode_icon(void);
static void update_text_colors(void);

/* ---------- UI Callbacks ---------- */
static void play_pause_cb(lv_event_t *e) {
    player_play_pause();
    if (player_is_playing() && !player_is_paused()) {
        lv_label_set_text(play_icon_g, LV_SYMBOL_PAUSE);
    } else {
        lv_label_set_text(play_icon_g, LV_SYMBOL_PLAY);
    }

    if (player_get_current_index() != -1) {
        const SongInfo* songs = player_get_songs();
        int idx = player_get_current_index();
        lv_label_set_text(title_label_g, songs[idx].title);
        lv_label_set_text(artist_label_g, songs[idx].artist);
    }
}

static void update_player_ui(void) {
    if (player_is_playing() && !player_is_paused()) {
        lv_label_set_text(play_icon_g, LV_SYMBOL_PAUSE);
    } else {
        lv_label_set_text(play_icon_g, LV_SYMBOL_PLAY);
    }

    if (player_get_current_index() != -1) {
        const SongInfo* songs = player_get_songs();
        int idx = player_get_current_index();
        lv_label_set_text(title_label_g, songs[idx].title);
        lv_label_set_text(artist_label_g, songs[idx].artist);
    }
}

static void next_btn_cb(lv_event_t *e) {
    player_next();
    update_player_ui(); // <-- 改这里
}
static void prev_btn_cb(lv_event_t *e) {
    player_prev();
    update_player_ui(); // <-- 改这里
}


static void mode_btn_cb(lv_event_t *e) {
    player_set_next_mode();
    update_mode_icon();
}

static void progress_pressed_cb(lv_event_t *e) { user_seeking = true; }
static void progress_released_cb(lv_event_t *e) {
    if (player_get_current_index() == -1) { user_seeking = false; return; }
    int val = lv_slider_get_value(progress_bar_g);
    player_seek(val);
    char buf[16];
    format_time(val, buf, sizeof(buf));
    lv_label_set_text(curr_time_g, buf);
    user_seeking = false;
}
static void progress_value_changed_cb(lv_event_t *e) {
    if (!user_seeking) return;
    int val = lv_slider_get_value(progress_bar_g);
    char buf[16];
    format_time(val, buf, sizeof(buf));
    lv_label_set_text(curr_time_g, buf);
}

static void song_item_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    if (selected_btn && lv_obj_is_valid(selected_btn)) {
        lv_obj_set_style_bg_color(selected_btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(selected_btn, 56, 0);
        lv_obj_set_style_text_color(selected_btn, lv_color_hex(0xDBEAFE), 0);
    }
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_100, 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
    selected_btn = btn;

    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    player_play_index(idx);
    update_player_ui();
}

static void clear_search_cb(lv_event_t *e) {
    lv_textarea_set_text(search_box_g, "");
    if (clear_btn_g) lv_obj_add_flag(clear_btn_g, LV_OBJ_FLAG_HIDDEN);
    search_cb(NULL);
}

static void search_cb(lv_event_t *e) {
    const char *txt = lv_textarea_get_text(search_box_g);
    if (!txt) txt = "";

    if (strlen(txt) > 0) {
        lv_obj_set_style_shadow_width(search_box_g, 20, 0);
        lv_obj_set_style_shadow_color(search_box_g, lv_color_hex(0x3B82F6), 0);
        lv_obj_set_style_shadow_opa(search_box_g, LV_OPA_80, 0);
        if (clear_btn_g) lv_obj_clear_flag(clear_btn_g, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_style_shadow_width(search_box_g, 0, 0);
        if (clear_btn_g) lv_obj_add_flag(clear_btn_g, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_clean(playlist_list);
    selected_btn = NULL;

    const SongInfo* songs = player_get_songs();
    int song_count = player_get_song_count();
    for (int i = 0; i < song_count; i++) {
        if (strlen(txt) > 0 && stristr(songs[i].title, txt) == NULL && stristr(songs[i].artist, txt) == NULL) continue;

        char label[256];
        snprintf(label, sizeof(label), "%s - %s", songs[i].title, songs[i].artist);
        lv_obj_t *btn = lv_list_add_btn(playlist_list, LV_SYMBOL_AUDIO, label);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(btn, 56, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xDBEAFE), 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, song_item_cb, LV_EVENT_CLICKED, NULL);
    }
}

static void search_box_focus_cb(lv_event_t *e) {
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(kb, search_box_g);
}

static void kb_event_cb(lv_event_t *e) {
    lv_obj_t *kb_obj = lv_event_get_target(e);
    uint32_t id = lv_buttonmatrix_get_selected_button(kb_obj);
    if (id != LV_BUTTONMATRIX_BUTTON_NONE) {
        const char *txt = lv_buttonmatrix_get_button_text(kb_obj, id);
        if (strcmp(txt, LV_SYMBOL_CLOSE) == 0 || strcmp(txt, LV_SYMBOL_OK) == 0) {
            lv_obj_add_flag(kb_obj, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_textarea(kb_obj, NULL);
        }
    }
}

static void toggle_sidebar(lv_event_t *e) {
    lv_obj_t *sidebar = (lv_obj_t *)lv_event_get_user_data(e);
    if (lv_obj_has_flag(sidebar, LV_OBJ_FLAG_HIDDEN)) lv_obj_clear_flag(sidebar, LV_OBJ_FLAG_HIDDEN);
    else {
        lv_obj_add_flag(sidebar, LV_OBJ_FLAG_HIDDEN);
        if (kb && !lv_obj_has_flag(kb, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_textarea(kb, NULL);
        }
    }
}

static void vol_slider_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int vol = lv_slider_get_value(slider);
    player_set_volume(vol);

    const char *sym;
    if (vol == 0)            sym = LV_SYMBOL_MUTE;
    else if (vol < 34)       sym = LV_SYMBOL_MUTE;
    else if (vol < 67)       sym = LV_SYMBOL_VOLUME_MID;
    else                     sym = LV_SYMBOL_VOLUME_MAX;
    lv_label_set_text(vol_icon_g, sym);
}

static void vol_btn_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    if (vol_popup && !lv_obj_has_flag(vol_popup, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(vol_popup, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_align_to(vol_popup, btn, LV_ALIGN_OUT_TOP_MID, 0, -8);
    lv_obj_move_foreground(vol_popup);
    lv_obj_clear_flag(vol_popup, LV_OBJ_FLAG_HIDDEN);
}

static void screen_click_cb(lv_event_t *e) {
    if (!vol_popup || lv_obj_has_flag(vol_popup, LV_OBJ_FLAG_HIDDEN)) return;
    lv_indev_t *indev = lv_indev_active();
    lv_point_t p;
    if (indev) {
        lv_indev_get_point(indev, &p);
        lv_area_t area;
        lv_obj_get_coords(vol_popup, &area);
        if (p.x >= area.x1 && p.x <= area.x2 && p.y >= area.y1 && p.y <= area.y2) return;
    }
    lv_obj_add_flag(vol_popup, LV_OBJ_FLAG_HIDDEN);
}

static void image_timer_cb(lv_timer_t *timer) {
    bg_load_next_image();
    update_text_colors();
}

static void refresh_btn_cb(lv_event_t *e) {
    const char *labels[] = {"Refresh: Off", "Refresh: 1min", "Refresh: 5min", "Refresh: 10min"};
    int intervals[] = {0, 60, 300, 600};
    int state_idx = 0;
    if (refresh_interval == 60) state_idx = 1;
    else if (refresh_interval == 300) state_idx = 2;
    else if (refresh_interval == 600) state_idx = 3;
    state_idx = (state_idx + 1) % 4;
    refresh_interval = intervals[state_idx];
    lv_label_set_text(refresh_label, labels[state_idx]);

    if (image_timer) { lv_timer_del(image_timer); image_timer = NULL; }
    if (refresh_interval > 0) {
        if (bg_get_image_count() == 0) {
            refresh_interval = 0; lv_label_set_text(refresh_label, "Refresh: Off"); return;
        }
        image_timer = lv_timer_create(image_timer_cb, refresh_interval * 1000, NULL);
    }
    update_text_colors();
}

/* ---------- Helpers ---------- */
static void update_mode_icon(void) {
    const char *sym;
    switch (player_get_mode()) {
        case PLAY_MODE_SINGLE:  sym = LV_SYMBOL_REFRESH; break;
        case PLAY_MODE_SHUFFLE: sym = LV_SYMBOL_SHUFFLE; break;
        case PLAY_MODE_LIST:
        default:                sym = LV_SYMBOL_LOOP;    break;
    }
    lv_label_set_text(mode_icon_g, sym);
}

static void update_text_colors(void) {
    lv_obj_t *labels[] = { clock_label, title_label_g, artist_label_g,
                       curr_time_g, total_time_g, refresh_label, weather_label_g,
                       lyric_curr_g, lyric_next_g };

    int n = sizeof(labels) / sizeof(labels[0]);
    for (int i = 0; i < n; i++) {
        lv_obj_t *lbl = labels[i];
        if (!lbl || !lv_obj_is_valid(lbl)) continue;
        lv_area_t coords;
        lv_obj_get_coords(lbl, &coords);
        int cx = (coords.x1 + coords.x2) / 2;
        int cy = (coords.y1 + coords.y2) / 2;
        int w  = (coords.x2 - coords.x1);
        int h  = (coords.y2 - coords.y1);
        if (w < 4) w = 4;
        if (h < 4) h = 4;
        int lum = bg_get_luminance_at(cx - w/4, cy - h/4, w/2, h/2);
        lv_color_t color = (lum < 128) ? lv_color_hex(0xDBEAFE) : lv_color_hex(0x1E40AF);
        lv_obj_set_style_text_color(lbl, color, 0);
    }
}
void ui_update_text_colors(void) { update_text_colors(); }

/* ---------- Styles ---------- */
static void apply_frost_light(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(obj, 38, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_opa(obj, 51, 0);
    lv_obj_set_style_shadow_width(obj, 16, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(obj, 76, 0);
}

static void apply_frost_dark(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x00001E), 0);
    lv_obj_set_style_bg_opa(obj, 178, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_opa(obj, 51, 0);
    lv_obj_set_style_shadow_width(obj, 16, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(obj, 76, 0);
}

/* ---------- LVGL Timers ---------- */
static void clock_timer_cb(lv_timer_t *timer) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
    lv_label_set_text(clock_label, buf);
}

static void progress_timer_cb(lv_timer_t *timer) {
    if (player_get_current_index() == -1) return;

    if (player_music_ended()) {
        player_clear_end_flag();
        int next_idx = player_get_next_song_index(); // <-- 根据播放模式获取下一首
        if (next_idx >= 0) {
            player_play_index(next_idx);
            update_player_ui(); // <-- 只更新UI，不要调用 play_pause_cb
        }
        return;
    }

    if (user_seeking) return;
    if (!player_is_playing() || player_is_paused()) return;

    int cur_sec = player_get_position();
    lv_slider_set_value(progress_bar_g, cur_sec, LV_ANIM_OFF);
    char buf[16];
    format_time(cur_sec, buf, sizeof(buf));
    lv_label_set_text(curr_time_g, buf);



    // 更新歌词
    const char *curr_text = NULL;
    const char *next_text = NULL;
    player_get_lyrics((double)cur_sec, &curr_text, &next_text);

    if (curr_text) lv_label_set_text(lyric_curr_g, curr_text);
    else lv_label_set_text(lyric_curr_g, "");

    if (next_text) lv_label_set_text(lyric_next_g, next_text);
    else lv_label_set_text(lyric_next_g, "");



    // Update duration in case it wasn't available initially
    int total_sec = player_get_duration();
    if (total_sec > 0 && lv_slider_get_max_value(progress_bar_g) != total_sec) {
        lv_slider_set_range(progress_bar_g, 0, total_sec);
        format_time(total_sec, buf, sizeof(buf));
        lv_label_set_text(total_time_g, buf);
    }
}

/* ---------- UI Builder ---------- */
void ui_init(void) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_0, 0);

    clock_label = lv_label_create(scr);
    lv_label_set_text(clock_label, "--:--:--");
    lv_obj_set_style_text_color(clock_label, lv_color_hex(0x1E40AF), 0);
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_32, 0);
    lv_obj_align(clock_label, LV_ALIGN_TOP_LEFT, 32, 32);

    weather_label_g = lv_label_create(scr);
    lv_label_set_text(weather_label_g, "25°C   61%");
    lv_obj_set_style_text_color(weather_label_g, lv_color_hex(0x1E40AF), 0);
    lv_obj_set_style_text_font(weather_label_g, &lv_font_montserrat_18, 0);
    lv_obj_align_to(weather_label_g, clock_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 12);

    lv_timer_create(clock_timer_cb, 1000, NULL);
    lv_timer_create(progress_timer_cb, 1000, NULL);

    lv_obj_t *player_card = lv_obj_create(scr);
    lv_obj_set_size(player_card, 384, LV_SIZE_CONTENT);
    lv_obj_align(player_card, LV_ALIGN_BOTTOM_RIGHT, -16, -16);
    apply_frost_light(player_card);
    lv_obj_set_style_radius(player_card, 16, 0);
    lv_obj_set_flex_flow(player_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(player_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(player_card, 16, 0);
    lv_obj_set_style_pad_row(player_card, 8, 0);

    lv_obj_t *header = lv_obj_create(player_card);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header, 0, 0);

    title_label_g = lv_label_create(header);
    lv_label_set_text(title_label_g, "No song selected");
    lv_obj_set_style_text_color(title_label_g, lv_color_hex(0xDBEAFE), 0);
    lv_obj_set_style_text_font(title_label_g, &lv_font_montserrat_18, 0);

    lv_obj_t *menu_btn = lv_btn_create(header);
    lv_obj_set_size(menu_btn, 36, 36);
    apply_frost_light(menu_btn);
    lv_obj_set_style_radius(menu_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(menu_btn, 0, 0);
    lv_obj_t *menu_icon = lv_label_create(menu_btn);
    lv_label_set_text(menu_icon, LV_SYMBOL_LIST);
    lv_obj_set_style_text_color(menu_icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(menu_icon);

    artist_label_g = lv_label_create(player_card);
    lv_label_set_text(artist_label_g, "Unknown artist");
    lv_obj_set_style_text_color(artist_label_g, lv_color_hex(0xBFDBFE), 0);
    lv_obj_set_style_text_font(artist_label_g, &lv_font_montserrat_14, 0);

    progress_bar_g = lv_slider_create(player_card);
    lv_obj_set_size(progress_bar_g, LV_PCT(100), 6);
    lv_slider_set_range(progress_bar_g, 0, 1);
    lv_slider_set_value(progress_bar_g, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress_bar_g, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(progress_bar_g, 51, 0);
    lv_obj_set_style_radius(progress_bar_g, 3, 0);
    lv_obj_set_style_bg_color(progress_bar_g, lv_color_hex(0x3B82F6), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(progress_bar_g, LV_OPA_100, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(progress_bar_g, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(progress_bar_g, 0, LV_PART_KNOB);
    lv_obj_set_ext_click_area(progress_bar_g, 8);
    lv_obj_set_style_pad_all(progress_bar_g, 0, 0);
    lv_obj_set_style_pad_all(progress_bar_g, 0, LV_PART_INDICATOR);

    lv_obj_add_event_cb(progress_bar_g, progress_pressed_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(progress_bar_g, progress_released_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(progress_bar_g, progress_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *time_row = lv_obj_create(player_card);
    lv_obj_set_size(time_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(time_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(time_row, 0, 0);
    lv_obj_set_style_pad_all(time_row, 0, 0);
    lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    curr_time_g = lv_label_create(time_row);
    lv_label_set_text(curr_time_g, "0:00");
    lv_obj_set_style_text_color(curr_time_g, lv_color_hex(0xBFDBFE), 0);
    lv_obj_set_style_text_font(curr_time_g, &lv_font_montserrat_12, 0);

    total_time_g = lv_label_create(time_row);
    lv_label_set_text(total_time_g, "0:00");
    lv_obj_set_style_text_color(total_time_g, lv_color_hex(0xBFDBFE), 0);
    lv_obj_set_style_text_font(total_time_g, &lv_font_montserrat_12, 0);

    lv_obj_t *controls = lv_obj_create(player_card);
    lv_obj_set_size(controls, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(controls, LV_OPA_0, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_pad_all(controls, 0, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(controls, 12, 0);

    lv_obj_t *mode_btn = lv_btn_create(controls);
    lv_obj_set_size(mode_btn, 36, 36);
    lv_obj_set_style_radius(mode_btn, LV_RADIUS_CIRCLE, 0);
    apply_frost_light(mode_btn);
    lv_obj_set_style_shadow_width(mode_btn, 0, 0);
    mode_icon_g = lv_label_create(mode_btn);
    lv_label_set_text(mode_icon_g, LV_SYMBOL_LOOP);
    lv_obj_set_style_text_color(mode_icon_g, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(mode_icon_g);
    lv_obj_add_event_cb(mode_btn, mode_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *prev_btn = lv_btn_create(controls);
    lv_obj_set_size(prev_btn, 36, 36);
    lv_obj_set_style_radius(prev_btn, LV_RADIUS_CIRCLE, 0);
    apply_frost_light(prev_btn);
    lv_obj_set_style_shadow_width(prev_btn, 0, 0);
    lv_obj_t *prev_icon = lv_label_create(prev_btn);
    lv_label_set_text(prev_icon, LV_SYMBOL_PREV);
    lv_obj_set_style_text_color(prev_icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(prev_icon);
    lv_obj_add_event_cb(prev_btn, prev_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *play_btn = lv_btn_create(controls);
    lv_obj_set_size(play_btn, 48, 48);
    lv_obj_set_style_radius(play_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_bg_opa(play_btn, LV_OPA_100, 0);
    lv_obj_set_style_border_width(play_btn, 0, 0);
    lv_obj_set_style_shadow_width(play_btn, 8, 0);
    lv_obj_set_style_shadow_color(play_btn, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_shadow_opa(play_btn, 102, 0);
    play_icon_g = lv_label_create(play_btn);
    lv_label_set_text(play_icon_g, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(play_icon_g, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(play_icon_g);
    lv_obj_add_event_cb(play_btn, play_pause_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *next_btn = lv_btn_create(controls);
    lv_obj_set_size(next_btn, 36, 36);
    lv_obj_set_style_radius(next_btn, LV_RADIUS_CIRCLE, 0);
    apply_frost_light(next_btn);
    lv_obj_set_style_shadow_width(next_btn, 0, 0);
    lv_obj_t *next_icon = lv_label_create(next_btn);
    lv_label_set_text(next_icon, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_color(next_icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(next_icon);
    lv_obj_add_event_cb(next_btn, next_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *vol_btn = lv_btn_create(controls);
    lv_obj_set_size(vol_btn, 36, 36);
    lv_obj_set_style_radius(vol_btn, LV_RADIUS_CIRCLE, 0);
    apply_frost_light(vol_btn);
    lv_obj_set_style_shadow_width(vol_btn, 0, 0);
    vol_icon_g = lv_label_create(vol_btn);
    lv_label_set_text(vol_icon_g, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_color(vol_icon_g, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(vol_icon_g);
    lv_obj_add_event_cb(vol_btn, vol_btn_cb, LV_EVENT_CLICKED, NULL);

    vol_popup = lv_obj_create(lv_layer_top());
    lv_obj_set_size(vol_popup, 56, 160);
    apply_frost_dark(vol_popup);
    lv_obj_set_style_radius(vol_popup, 12, 0);
    lv_obj_set_style_pad_all(vol_popup, 6, 0);
    lv_obj_set_style_border_width(vol_popup, 1, 0);
    lv_obj_set_style_border_color(vol_popup, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(vol_popup, 51, 0);
    lv_obj_clear_flag(vol_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(vol_popup, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *vol_slider = lv_slider_create(vol_popup);
    lv_obj_set_size(vol_slider, 8, 120);
    lv_obj_align(vol_slider, LV_ALIGN_CENTER, 0, 0);
    lv_slider_set_range(vol_slider, 0, 128);
    lv_slider_set_value(vol_slider, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(vol_slider, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(vol_slider, 51, 0);
    lv_obj_set_style_bg_color(vol_slider, lv_color_hex(0x3B82F6), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(vol_slider, LV_OPA_100, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(vol_slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_pad_all(vol_slider, 3, 0);
    lv_obj_add_event_cb(vol_slider, vol_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_add_event_cb(scr, screen_click_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scr, screen_click_cb, LV_EVENT_CLICKED, NULL);

    static lv_obj_t *sidebar = NULL;
    sidebar = lv_obj_create(scr);
    lv_obj_set_size(sidebar, 340, 600);
    lv_obj_align(sidebar, LV_ALIGN_RIGHT_MID, 0, 0);
    apply_frost_dark(sidebar);
    lv_obj_set_style_radius(sidebar, 0, 0);
    lv_obj_remove_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(sidebar, 16, 0);
    lv_obj_set_style_pad_row(sidebar, 12, 0);
    lv_obj_set_style_pad_column(sidebar, 0, 0);
    lv_obj_set_style_border_width(sidebar, 0, 0);
    lv_obj_add_flag(sidebar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_flex_flow(sidebar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sidebar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *sidebar_header = lv_obj_create(sidebar);
    lv_obj_set_size(sidebar_header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(sidebar_header, LV_OPA_0, 0);
    lv_obj_set_style_border_width(sidebar_header, 0, 0);
    lv_obj_set_style_pad_all(sidebar_header, 0, 0);
    lv_obj_remove_flag(sidebar_header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(sidebar_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sidebar_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *list_title = lv_label_create(sidebar_header);
    lv_label_set_text(list_title, "Music Playlist");
    lv_obj_set_style_text_color(list_title, lv_color_hex(0xF0F9FF), 0);
    lv_obj_set_style_text_font(list_title, &lv_font_montserrat_24, 0);

    lv_obj_t *close_btn = lv_btn_create(sidebar_header);
    lv_obj_set_size(close_btn, 36, 36);
    apply_frost_light(close_btn);
    lv_obj_set_style_radius(close_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(close_btn, 0, 0);
    lv_obj_t *close_icon = lv_label_create(close_btn);
    lv_label_set_text(close_icon, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(close_icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(close_icon);
    lv_obj_add_event_cb(close_btn, toggle_sidebar, LV_EVENT_CLICKED, sidebar);

    lv_obj_t *search_container = lv_obj_create(sidebar);
    lv_obj_set_width(search_container, LV_PCT(100));
    lv_obj_set_height(search_container, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(search_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(search_container, 0, 0);
    lv_obj_set_style_pad_all(search_container, 0, 0);
    lv_obj_remove_flag(search_container, LV_OBJ_FLAG_SCROLLABLE);

    search_box_g = lv_textarea_create(search_container);
    lv_obj_set_width(search_box_g, LV_PCT(100));
    lv_textarea_set_one_line(search_box_g, true);
    lv_textarea_set_placeholder_text(search_box_g, "Search songs...");
    lv_obj_set_style_bg_color(search_box_g, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(search_box_g, 25, 0);
    lv_obj_set_style_text_color(search_box_g, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(search_box_g, 8, 0);
    lv_obj_set_style_border_color(search_box_g, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(search_box_g, 51, 0);
    lv_obj_set_style_border_width(search_box_g, 1, 0);
    lv_obj_set_style_pad_right(search_box_g, 36, 0);

    lv_obj_add_event_cb(search_box_g, search_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(search_box_g, search_box_focus_cb, LV_EVENT_CLICKED, NULL);

    clear_btn_g = lv_label_create(search_container);
    lv_label_set_text(clear_btn_g, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(clear_btn_g, lv_color_hex(0xBFDBFE), 0);
    lv_obj_align(clear_btn_g, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_add_flag(clear_btn_g, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(clear_btn_g, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(clear_btn_g, clear_search_cb, LV_EVENT_CLICKED, NULL);

    playlist_list = lv_list_create(sidebar);
    lv_obj_set_width(playlist_list, LV_PCT(100));
    lv_obj_set_flex_grow(playlist_list, 1);
    lv_obj_set_style_bg_opa(playlist_list, LV_OPA_0, 0);
    lv_obj_set_style_border_width(playlist_list, 0, 0);
    lv_obj_set_style_pad_all(playlist_list, 0, 0);
    lv_obj_set_style_pad_row(playlist_list, 6, 0);

    search_cb(NULL);
    lv_obj_add_event_cb(menu_btn, toggle_sidebar, LV_EVENT_CLICKED, sidebar);



    /* 歌词显示区域 */
    lv_obj_t *lyric_cont = lv_obj_create(scr);
    lv_obj_set_size(lyric_cont, 400, 60);
    lv_obj_align(lyric_cont, LV_ALIGN_BOTTOM_MID, -100, -16); // 位于左下角刷新按钮和右下角控制框中间
    lv_obj_set_style_bg_opa(lyric_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(lyric_cont, 0, 0);
    lv_obj_set_flex_flow(lyric_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(lyric_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(lyric_cont, LV_OBJ_FLAG_SCROLLABLE);

    lyric_curr_g = lv_label_create(lyric_cont);
    lv_obj_set_width(lyric_curr_g, 380);
    lv_label_set_text(lyric_curr_g, "");
    lv_obj_set_style_text_align(lyric_curr_g, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lyric_curr_g, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lyric_curr_g, &lv_font_montserrat_16, 0);

    lyric_next_g = lv_label_create(lyric_cont);
    lv_obj_set_width(lyric_next_g, 380);
    lv_label_set_text(lyric_next_g, "");
    lv_obj_set_style_text_align(lyric_next_g, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lyric_next_g, lv_color_hex(0xBFDBFE), 0);
    lv_obj_set_style_text_font(lyric_next_g, &lv_font_montserrat_14, 0);











    lv_obj_t *refresh_btn = lv_btn_create(scr);
    lv_obj_set_size(refresh_btn, 140, 48);
    lv_obj_align(refresh_btn, LV_ALIGN_BOTTOM_LEFT, 32, -16);
    apply_frost_light(refresh_btn);
    lv_obj_set_style_radius(refresh_btn, 24, 0);
    lv_obj_set_style_shadow_width(refresh_btn, 0, 0);
    refresh_label = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_label, "Refresh: Off");
    lv_obj_set_style_text_color(refresh_label, lv_color_hex(0xDBEAFE), 0);
    lv_obj_set_style_text_font(refresh_label, &lv_font_montserrat_14, 0);
    lv_obj_center(refresh_label);
    lv_obj_add_event_cb(refresh_btn, refresh_btn_cb, LV_EVENT_CLICKED, NULL);

    kb = lv_keyboard_create(lv_layer_top());
    lv_obj_set_width(kb, 1024);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    update_mode_icon();
}
