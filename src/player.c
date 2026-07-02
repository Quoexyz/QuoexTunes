#include "player.h"
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static Mix_Music *cur_music = NULL;
static bool is_playing = false;
static bool is_paused = false;
static int g_volume = 100;
static volatile bool music_end_flag = false;

static SongInfo songs[MAX_SONGS];
static int song_count = 0;
static int current_song_index = -1;
static PlayMode current_play_mode = PLAY_MODE_LIST;


static LyricLine lrc_lines[MAX_LRC_LINES];
static int lrc_count = 0;

// qsort 比较函数：按时间升序
static int lrc_compare(const void *a, const void *b) {
    const LyricLine *la = (const LyricLine *)a;
    const LyricLine *lb = (const LyricLine *)b;
    if (la->time_sec < lb->time_sec) return -1;
    if (la->time_sec > lb->time_sec) return 1;
    return 0;
}

// 内部辅助函数：加载并解析LRC
static void load_lrc(const char *audio_filepath) {
    lrc_count = 0;
    char lrc_path[512];
    strncpy(lrc_path, audio_filepath, sizeof(lrc_path));
    lrc_path[sizeof(lrc_path)-1] = '\0';

    char *ext = strrchr(lrc_path, '.');
    if (ext) strcpy(ext, ".lrc");
    else strcat(lrc_path, ".lrc");

    FILE *f = fopen(lrc_path, "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f) && lrc_count < MAX_LRC_LINES) {
        double times[16];
        int time_count = 0;
        char *text_start = NULL;

        char *p = line;
        // 解析该行所有时间戳 [mm:ss.xx]（支持 2 位厘秒或 3 位毫秒）
        while (time_count < 16) {
            int min, sec, ms = 0;
            int n = 0;
            if (sscanf(p, "[%d:%d.%d%n", &min, &sec, &ms, &n) >= 2) {
                p += n;
                if (*p == ']') p++;
                double t = min * 60.0 + sec + (ms >= 100 ? ms / 1000.0 : ms / 100.0);
                times[time_count++] = t;
                text_start = p; // 记录最后一个时间戳后的文本位置
            } else {
                break;
            }
        }

        if (time_count == 0) continue; // 无时间戳，跳过

        // 提取文本
        if (!text_start) continue;
        char *nl = strpbrk(text_start, "\r\n");
        if (nl) *nl = '\0';
        while (*text_start == ' ' || *text_start == '\t') text_start++;
        if (strlen(text_start) == 0) continue; // 空文本跳过

        // 每个时间戳生成一条歌词
        for (int i = 0; i < time_count && lrc_count < MAX_LRC_LINES; i++) {
            lrc_lines[lrc_count].time_sec = times[i];
            strncpy(lrc_lines[lrc_count].text, text_start, 255);
            lrc_lines[lrc_count].text[255] = '\0';
            lrc_count++;
        }
    }
    fclose(f);

    // 按时间排序，保证查找正确
    if (lrc_count > 1) {
        qsort(lrc_lines, lrc_count, sizeof(LyricLine), lrc_compare);
    }
}



static void music_finished_hook(void) {
    music_end_flag = true;
}

static void stop_playback(void) {
    if (cur_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(cur_music);
        cur_music = NULL;
    }
    is_playing = false;
    is_paused = false;
    music_end_flag = false;
}

static void load_and_play(int index) {
    if (index < 0 || index >= song_count) return;
    stop_playback();

    cur_music = Mix_LoadMUS(songs[index].filepath);
    if (!cur_music) {
        printf("Failed to load music [%s]: %s\n", songs[index].filepath, Mix_GetError());
        return;
    }
    current_song_index = index;

    Mix_VolumeMusic(g_volume);
    if (Mix_PlayMusic(cur_music, 1) == -1) {
        printf("Failed to play music: %s\n", Mix_GetError());
        Mix_FreeMusic(cur_music);
        cur_music = NULL;
        return;
    }


    load_lrc(songs[index].filepath);

    is_playing = true;
    is_paused = false;
}

static int pick_next_song(void) {
    if (song_count == 0) return -1;
    switch (current_play_mode) {
        case PLAY_MODE_SINGLE:
            return current_song_index;
        case PLAY_MODE_SHUFFLE: {
            if (song_count == 1) return 0;
            int n;
            do { n = rand() % song_count; } while (n == current_song_index);
            return n;
        }
        case PLAY_MODE_LIST:
        default:
            return (current_song_index + 1) % song_count;
    }
}

void player_init(void) {
    int mix_flags = MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_FLAC | MIX_INIT_MOD;
    int mix_inited = Mix_Init(mix_flags);
    if ((mix_inited & mix_flags) != mix_flags) {
        printf("SDL_mixer partial init: inited=0x%x, err: %s\n", mix_inited, Mix_GetError());
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
        printf("Mix_OpenAudio failed: %s\n", Mix_GetError());
    }
    Mix_VolumeMusic(g_volume);
    Mix_HookMusicFinished(music_finished_hook);
}

void player_cleanup(void) {
    stop_playback();
    Mix_HookMusicFinished(NULL);
    Mix_CloseAudio();
    Mix_Quit();
}

void player_load_songs(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return;
    song_count = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && song_count < MAX_SONGS) {
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext) continue;
        if (strcasecmp(ext, ".wav")  != 0 && strcasecmp(ext, ".mp3")  != 0 &&
            strcasecmp(ext, ".ogg")  != 0 && strcasecmp(ext, ".flac") != 0 &&
            strcasecmp(ext, ".mod")  != 0 && strcasecmp(ext, ".mid")  != 0) continue;

        snprintf(songs[song_count].filepath, sizeof(songs[song_count].filepath), "%s\\%s", dir, entry->d_name);

        char name[256];
        strncpy(name, entry->d_name, sizeof(name));
        name[sizeof(name)-1] = '\0';
        char *ext_pos = strrchr(name, '.');
        if (ext_pos) *ext_pos = '\0';

        char *sep = strstr(name, "==");
        if (sep) {
            *sep = '\0';
            strncpy(songs[song_count].title, name, sizeof(songs[song_count].title));
            strncpy(songs[song_count].artist, sep + 2, sizeof(songs[song_count].artist));
        } else {
            strncpy(songs[song_count].title, name, sizeof(songs[song_count].title));
            songs[song_count].artist[0] = '\0';
        }
        song_count++;
    }
    closedir(d);
}

void player_play_index(int index) {
    load_and_play(index);
}

void player_play_pause(void) {
    if (current_song_index == -1) {
        if (song_count > 0) load_and_play(0);
        return;
    }
    if (is_playing && !is_paused) {
        is_paused = true;
        Mix_PauseMusic();
    } else {
        if (!is_playing) {
            if (cur_music) {
                Mix_SetMusicPosition(0.0);
                Mix_PlayMusic(cur_music, 1);
            }
            is_playing = true;
        } else {
            Mix_ResumeMusic();
        }
        is_paused = false;
    }
}

void player_next(void) {
    if (song_count == 0) return;
    load_and_play((current_song_index + 1) % song_count);
}

void player_prev(void) {
    if (song_count == 0) return;
    load_and_play((current_song_index - 1 + song_count) % song_count);
}

void player_seek(int seconds) {
    if (!cur_music) return;
    Mix_SetMusicPosition((double)seconds);
}

int player_get_song_count(void) { return song_count; }
int player_get_current_index(void) { return current_song_index; }
const SongInfo* player_get_songs(void) { return songs; }

int player_get_position(void) {
    if (!cur_music) return 0;
    double pos = Mix_GetMusicPosition(cur_music);
    return (pos < 0.0) ? 0 : (int)pos;
}

int player_get_duration(void) {
    if (!cur_music) return 0;
    double dur = Mix_MusicDuration(cur_music);
    return (dur < 0.0) ? 0 : (int)dur;
}

int player_get_volume(void) { return g_volume; }
void player_set_volume(int vol) {
    g_volume = vol;
    Mix_VolumeMusic(g_volume);
}

bool player_is_playing(void) { return is_playing; }
bool player_is_paused(void) { return is_paused; }
bool player_music_ended(void) { return music_end_flag; }
void player_clear_end_flag(void) { music_end_flag = false; }

PlayMode player_get_mode(void) { return current_play_mode; }
void player_set_next_mode(void) {
    current_play_mode = (PlayMode)((current_play_mode + 1) % 3);
}

int player_get_next_song_index(void) {
    return pick_next_song();
}
void player_get_lyrics(double pos, const char **curr, const char **next) {
    *curr = NULL;
    *next = NULL;
    if (lrc_count == 0) return;

    int found_idx = -1;
    for (int i = 0; i < lrc_count; i++) {
        if (lrc_lines[i].time_sec <= pos) {
            found_idx = i;
        } else {
            break;
        }
    }

    if (found_idx != -1) {
        *curr = lrc_lines[found_idx].text;
        if (found_idx + 1 < lrc_count) {
            *next = lrc_lines[found_idx + 1].text;
        }
    }
}
