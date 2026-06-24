#include "deck.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int LOOP_ROW_LENGTHS[] = { 4, 8, 16, 32, 64, 128 };
static const int LOOP_PATTERN_LENGTHS[] = { 1, 2, 4, 8 };
static const int LOOP_ROW_COUNT = (int)(sizeof(LOOP_ROW_LENGTHS) / sizeof(LOOP_ROW_LENGTHS[0]));
static const int LOOP_PATTERN_COUNT = (int)(sizeof(LOOP_PATTERN_LENGTHS) / sizeof(LOOP_PATTERN_LENGTHS[0]));

static void set_error(char *err, int err_len, const char *msg) {
    if (err && err_len > 0) {
        snprintf(err, (size_t)err_len, "%s", msg);
    }
}

static void copy_metadata(char *dst, size_t dst_len, openmpt_module *module, const char *key, const char *fallback) {
    const char *value = openmpt_module_get_metadata(module, key);
    const char *src = (value && value[0]) ? value : fallback;
    snprintf(dst, dst_len, "%s", src ? src : "");
    if (value) {
        openmpt_free_string(value);
    }
}

void deck_init(FtDeck *deck) {
    memset(deck, 0, sizeof(*deck));
    deck->volume = 1.0f;
    deck->tempo_bpm = 125.0;
    deck->sync_target_bpm = 125.0;
    deck->loop_start_order = -1;
    deck->loop_start_row = 0;
    deck->loop_length_index = 2;
    deck->loop_unit = FT_LOOP_ROWS;
    deck->cue_order = 0;
    deck->cue_row = 0;
}

void deck_close(FtDeck *deck) {
    if (deck->module) {
        openmpt_module_destroy(deck->module);
    }
    deck_init(deck);
}

bool deck_load(FtDeck *deck, const char *path, int sample_rate, char *err, int err_len) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        set_error(err, err_len, "could not open module");
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        set_error(err, err_len, "could not seek module");
        return false;
    }
    long size = ftell(file);
    if (size <= 0) {
        fclose(file);
        set_error(err, err_len, "empty module");
        return false;
    }
    rewind(file);

    void *data = malloc((size_t)size);
    if (!data) {
        fclose(file);
        set_error(err, err_len, "out of memory loading module");
        return false;
    }
    if (fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        set_error(err, err_len, "could not read module");
        return false;
    }
    fclose(file);

    int openmpt_err = OPENMPT_ERROR_OK;
    const openmpt_module_initial_ctl ctls[] = {
        { "render.resampler.emulate_amiga", "1" },
        { "play.at_end", "stop" },
        { NULL, NULL }
    };
    openmpt_module *module = openmpt_module_create_from_memory2(data, (size_t)size, NULL, NULL, NULL, NULL, &openmpt_err, NULL, ctls);
    free(data);

    if (!module) {
        set_error(err, err_len, "libopenmpt rejected module");
        return false;
    }

    deck_close(deck);
    deck->module = module;
    deck->loaded = true;
    deck->playing = false;
    deck->volume = 1.0f;
    deck->tempo_bpm = 125.0;
    deck->sync_target_bpm = 125.0;
    deck->loop_start_order = -1;
    deck->loop_start_row = 0;
    deck->loop_length_index = 2;
    deck->loop_unit = FT_LOOP_ROWS;
    deck->cue_order = 0;
    deck->cue_row = 0;
    deck->duration_seconds = openmpt_module_get_duration_seconds(module);
    snprintf(deck->path, sizeof(deck->path), "%s", path);
    copy_metadata(deck->title, sizeof(deck->title), module, "title", path);
    copy_metadata(deck->format, sizeof(deck->format), module, "type_long", "module");
    openmpt_module_set_render_param(module, OPENMPT_MODULE_RENDER_MASTERGAIN_MILLIBEL, 0);
    openmpt_module_set_render_param(module, OPENMPT_MODULE_RENDER_STEREOSEPARATION_PERCENT, 100);
    openmpt_module_set_position_seconds(module, 0.0);
    (void)sample_rate;
    return true;
}

void deck_play_pause(FtDeck *deck) {
    if (!deck->loaded) return;
    deck->playing = !deck->playing;
}

void deck_stop(FtDeck *deck) {
    if (!deck->loaded) return;
    deck->playing = false;
    if (deck->cue_set) {
        openmpt_module_set_position_order_row(deck->module, deck->cue_order, deck->cue_row);
    } else {
        openmpt_module_set_position_order_row(deck->module, 0, 0);
    }
}

void deck_cue(FtDeck *deck) {
    if (!deck->loaded) return;
    if (!deck->cue_set) deck_set_cue_current(deck);
    deck_jump_cue(deck);
}

void deck_set_cue_current(FtDeck *deck) {
    if (!deck->loaded) return;
    deck->cue_order = deck_current_order(deck);
    deck->cue_row = 0;
    deck->cue_seconds = openmpt_module_get_position_seconds(deck->module);
    deck->cue_set = true;
}

void deck_jump_cue(FtDeck *deck) {
    if (!deck->loaded || !deck->cue_set) return;
    openmpt_module_set_position_order_row(deck->module, deck->cue_order, deck->cue_row);
}

void deck_seek_relative(FtDeck *deck, double delta_seconds) {
    if (!deck->loaded) return;
    double pos = openmpt_module_get_position_seconds(deck->module) + delta_seconds;
    if (pos < 0.0) pos = 0.0;
    if (deck->duration_seconds > 0.0 && pos > deck->duration_seconds) pos = deck->duration_seconds;
    openmpt_module_set_position_seconds(deck->module, pos);
}

void deck_set_pitch(FtDeck *deck, double pitch_percent) {
    if (pitch_percent < -50.0) pitch_percent = -50.0;
    if (pitch_percent > 100.0) pitch_percent = 100.0;
    deck->pitch_percent = pitch_percent;
}

void deck_nudge(FtDeck *deck, double nudge_percent) {
    deck->nudge_percent = nudge_percent;
}

int deck_current_order(const FtDeck *deck) {
    if (!deck->loaded) return 0;
    return openmpt_module_get_current_order(deck->module);
}

int deck_current_row(const FtDeck *deck) {
    if (!deck->loaded) return 0;
    return openmpt_module_get_current_row(deck->module);
}

int deck_current_pattern(const FtDeck *deck) {
    if (!deck->loaded) return 0;
    return openmpt_module_get_current_pattern(deck->module);
}

int deck_num_orders(const FtDeck *deck) {
    if (!deck->loaded) return 0;
    return openmpt_module_get_num_orders(deck->module);
}

int deck_num_patterns(const FtDeck *deck) {
    if (!deck->loaded) return 0;
    return openmpt_module_get_num_patterns(deck->module);
}

int deck_num_channels(const FtDeck *deck) {
    if (!deck->loaded) return 0;
    return openmpt_module_get_num_channels(deck->module);
}

int deck_current_playing_channels(const FtDeck *deck) {
    if (!deck->loaded) return 0;
    return openmpt_module_get_current_playing_channels(deck->module);
}

float deck_channel_vu(const FtDeck *deck, int channel) {
    if (!deck->loaded || channel < 0 || channel >= deck_num_channels(deck)) return 0.0f;
    return openmpt_module_get_current_channel_vu_mono(deck->module, channel);
}

int deck_current_pattern_rows(const FtDeck *deck) {
    if (!deck->loaded) return 64;
    int rows = openmpt_module_get_pattern_num_rows(deck->module, deck_current_pattern(deck));
    return rows > 0 ? rows : 64;
}

void deck_format_pattern_cell(const FtDeck *deck, int row, int channel, char *dst, int dst_len) {
    if (!dst || dst_len <= 0) return;
    dst[0] = '\0';
    if (!deck->loaded || !deck->module || channel < 0 || channel >= deck_num_channels(deck)) {
        snprintf(dst, (size_t)dst_len, "---000");
        return;
    }
    const char *cell = openmpt_module_format_pattern_row_channel(deck->module, deck_current_pattern(deck), row, channel, 6, 1);
    snprintf(dst, (size_t)dst_len, "%s", cell ? cell : "---000");
    if (cell) openmpt_free_string(cell);
}

void deck_set_loop_in(FtDeck *deck) {
    if (!deck->loaded) return;
    deck->loop_start_order = deck_current_order(deck);
    deck->loop_start_row = deck_current_row(deck);
    if (deck->loop_unit == FT_LOOP_ROWS) deck->loop_start_row = (deck->loop_start_row / 4) * 4;
}

void deck_set_loop_out(FtDeck *deck) {
    if (!deck->loaded) return;
    deck_set_loop_in(deck);
    deck->loop_enabled = true;
}

void deck_toggle_loop(FtDeck *deck) {
    if (!deck->loaded) return;
    if (deck->loop_start_order < 0) {
        deck_set_loop_in(deck);
    }
    deck->loop_enabled = !deck->loop_enabled;
}

void deck_loop_length_next(FtDeck *deck) {
    if (deck->loop_unit == FT_LOOP_ROWS) {
        if (deck->loop_length_index + 1 < LOOP_ROW_COUNT) deck->loop_length_index++;
        else { deck->loop_unit = FT_LOOP_PATTERNS; deck->loop_length_index = 0; }
    } else if (deck->loop_length_index + 1 < LOOP_PATTERN_COUNT) {
        deck->loop_length_index++;
    }
}

void deck_loop_length_prev(FtDeck *deck) {
    if (deck->loop_unit == FT_LOOP_PATTERNS) {
        if (deck->loop_length_index > 0) deck->loop_length_index--;
        else { deck->loop_unit = FT_LOOP_ROWS; deck->loop_length_index = LOOP_ROW_COUNT - 1; }
    } else if (deck->loop_length_index > 0) {
        deck->loop_length_index--;
    }
}

const char *deck_loop_length_label(const FtDeck *deck) {
    static char label[16];
    if (deck->loop_unit == FT_LOOP_ROWS) {
        snprintf(label, sizeof(label), "%dROW", LOOP_ROW_LENGTHS[deck->loop_length_index]);
    } else {
        int pats = LOOP_PATTERN_LENGTHS[deck->loop_length_index];
        snprintf(label, sizeof(label), pats == 1 ? "1PAT" : "%dPAT", pats);
    }
    return label;
}

void deck_enforce_loop(FtDeck *deck) {
    if (!deck->loaded || !deck->loop_enabled || deck->loop_start_order < 0) return;
    int order = deck_current_order(deck);
    int row = deck_current_row(deck);
    if (deck->loop_unit == FT_LOOP_PATTERNS) {
        int len = LOOP_PATTERN_LENGTHS[deck->loop_length_index];
        if (order < deck->loop_start_order || order >= deck->loop_start_order + len) {
            openmpt_module_set_position_order_row(deck->module, deck->loop_start_order, 0);
        }
    } else {
        int len = LOOP_ROW_LENGTHS[deck->loop_length_index];
        if (order != deck->loop_start_order || row < deck->loop_start_row || row >= deck->loop_start_row + len) {
            openmpt_module_set_position_order_row(deck->module, deck->loop_start_order, deck->loop_start_row);
        }
    }
}

double deck_position_seconds(const FtDeck *deck) {
    if (!deck->loaded) return 0.0;
    return openmpt_module_get_position_seconds(deck->module);
}

double deck_effective_rate(const FtDeck *deck) {
    double pitch = deck->pitch_percent + deck->nudge_percent;
    if (deck->sync_enabled && deck->tempo_bpm > 1.0 && deck->sync_target_bpm > 1.0) {
        pitch += ((deck->sync_target_bpm / deck->tempo_bpm) - 1.0) * 100.0;
    }
    double rate = 1.0 + pitch / 100.0;
    if (rate < 0.25) rate = 0.25;
    if (rate > 4.0) rate = 4.0;
    return rate;
}
