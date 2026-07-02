#ifndef PLAYER_H
#define PLAYER_H
#include <stdbool.h>

#define MAX_SONGS 200

typedef struct {
    char title[128];
    char artist[128];
    char filepath[512];
} SongInfo;

typedef enum {
    PLAY_MODE_LIST = 0,
    PLAY_MODE_SINGLE,
    PLAY_MODE_SHUFFLE
} PlayMode;

void player_init(void);
void player_cleanup(void);
void player_load_songs(const char *dir);

void player_play_index(int index);
void player_play_pause(void);
void player_next(void);
void player_prev(void);
void player_seek(int seconds);

#define MAX_LRC_LINES 300

typedef struct {
    double time_sec;
    char text[256];
} LyricLine;

void player_get_lyrics(double pos, const char **curr, const char **next);


int player_get_song_count(void);
int player_get_current_index(void);
const SongInfo* player_get_songs(void);
int player_get_position(void);
int player_get_duration(void);
int player_get_volume(void);
void player_set_volume(int vol);
bool player_is_playing(void);
bool player_is_paused(void);
bool player_music_ended(void);
void player_clear_end_flag(void);
PlayMode player_get_mode(void);
void player_set_next_mode(void);


int player_get_next_song_index(void);

#endif
