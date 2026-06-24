#ifndef FT1210_DECK_H
#define FT1210_DECK_H

#include <stdbool.h>
#include <stdint.h>

#include <libopenmpt/libopenmpt.h>

#define FT1210_DECK_NAME_MAX 256
#define FT1210_EQ_BANDS 3

typedef enum FtLoopUnit {
    FT_LOOP_ROWS,
    FT_LOOP_PATTERNS
} FtLoopUnit;

typedef struct FtDeck {
    openmpt_module *module;
    char path[1024];
    char title[FT1210_DECK_NAME_MAX];
    char format[FT1210_DECK_NAME_MAX];
    bool loaded;
    bool playing;
    bool cue_set;
    bool loop_enabled;
    bool sync_enabled;
    bool selected;
    int loop_start_order;
    int loop_start_row;
    int loop_length_index;
    FtLoopUnit loop_unit;
    int cue_order;
    int cue_row;
    double cue_seconds;
    double duration_seconds;
    double pitch_percent;
    double nudge_percent;
    double tempo_bpm;
    double sync_target_bpm;
    float gain_db;
    float volume;
    float eq_db[FT1210_EQ_BANDS];
    float vu_l;
    float vu_r;
    float spectrum[16];
    float hp_l[FT1210_EQ_BANDS];
    float hp_r[FT1210_EQ_BANDS];
    float lp_l[FT1210_EQ_BANDS];
    float lp_r[FT1210_EQ_BANDS];
} FtDeck;

void deck_init(FtDeck *deck);
void deck_close(FtDeck *deck);
bool deck_load(FtDeck *deck, const char *path, int sample_rate, char *err, int err_len);
void deck_play_pause(FtDeck *deck);
void deck_stop(FtDeck *deck);
void deck_cue(FtDeck *deck);
void deck_set_cue_current(FtDeck *deck);
void deck_jump_cue(FtDeck *deck);
void deck_seek_relative(FtDeck *deck, double delta_seconds);
void deck_set_pitch(FtDeck *deck, double pitch_percent);
void deck_nudge(FtDeck *deck, double nudge_percent);
void deck_toggle_loop(FtDeck *deck);
void deck_loop_length_next(FtDeck *deck);
void deck_loop_length_prev(FtDeck *deck);
const char *deck_loop_length_label(const FtDeck *deck);
void deck_set_loop_in(FtDeck *deck);
void deck_set_loop_out(FtDeck *deck);
void deck_enforce_loop(FtDeck *deck);
double deck_position_seconds(const FtDeck *deck);
double deck_effective_rate(const FtDeck *deck);
int deck_current_order(const FtDeck *deck);
int deck_current_row(const FtDeck *deck);
int deck_current_pattern(const FtDeck *deck);
int deck_num_orders(const FtDeck *deck);
int deck_num_patterns(const FtDeck *deck);
int deck_num_channels(const FtDeck *deck);
int deck_current_playing_channels(const FtDeck *deck);
float deck_channel_vu(const FtDeck *deck, int channel);
int deck_current_pattern_rows(const FtDeck *deck);
void deck_format_pattern_cell(const FtDeck *deck, int row, int channel, char *dst, int dst_len);

#endif
