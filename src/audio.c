#include "audio.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MIX_RATE 48000
#define MIX_CHANNELS 2
#define RENDER_BLOCK 256

static float db_to_gain(float db) {
    return powf(10.0f, db / 20.0f);
}

static float one_pole_low(float input, float *state, float alpha) {
    *state += alpha * (input - *state);
    return *state;
}

static void apply_deck_eq(FtDeck *deck, float *l, float *r) {
    float lo_l = one_pole_low(*l, &deck->lp_l[0], 0.035f);
    float lo_r = one_pole_low(*r, &deck->lp_r[0], 0.035f);
    float hi_lp_l = one_pole_low(*l, &deck->lp_l[1], 0.32f);
    float hi_lp_r = one_pole_low(*r, &deck->lp_r[1], 0.32f);
    float hi_l = *l - hi_lp_l;
    float hi_r = *r - hi_lp_r;
    float mid_l = *l - lo_l - hi_l;
    float mid_r = *r - lo_r - hi_r;

    float g_hi = db_to_gain(deck->eq_db[0]);
    float g_mid = db_to_gain(deck->eq_db[1]);
    float g_lo = db_to_gain(deck->eq_db[2]);
    *l = lo_l * g_lo + mid_l * g_mid + hi_l * g_hi;
    *r = lo_r * g_lo + mid_r * g_mid + hi_r * g_hi;
}

static void render_deck(FtDeck *deck, int sample_rate, float *out, int frames, float pan_gain) {
    if (!deck->loaded || !deck->playing || !deck->module) return;

    float tmp[RENDER_BLOCK * 2];
    int done = 0;
    double rate = deck_effective_rate(deck);
    int render_rate = (int)lrint((double)sample_rate / rate);
    if (render_rate < 8000) render_rate = 8000;
    if (render_rate > 192000) render_rate = 192000;

    while (done < frames) {
        int todo = frames - done;
        if (todo > RENDER_BLOCK) todo = RENDER_BLOCK;
        memset(tmp, 0, sizeof(float) * (size_t)todo * 2);
        size_t got = openmpt_module_read_interleaved_float_stereo(deck->module, render_rate, (size_t)todo, tmp);
        if (got == 0) {
            deck->playing = false;
            break;
        }
        deck_enforce_loop(deck);

        float vu_l = 0.0f;
        float vu_r = 0.0f;
        float deck_gain = pan_gain * deck->volume * db_to_gain(deck->gain_db);
        for (size_t i = 0; i < got; i++) {
            float l = tmp[i * 2];
            float r = tmp[i * 2 + 1];
            apply_deck_eq(deck, &l, &r);
            l *= deck_gain;
            r *= deck_gain;
            out[(done + (int)i) * 2] += l;
            out[(done + (int)i) * 2 + 1] += r;
            if (fabsf(l) > vu_l) vu_l = fabsf(l);
            if (fabsf(r) > vu_r) vu_r = fabsf(r);
        }
        deck->vu_l = deck->vu_l * 0.86f + vu_l * 0.14f;
        deck->vu_r = deck->vu_r * 0.86f + vu_r * 0.14f;
        done += (int)got;
        if ((int)got < todo) break;
    }
}

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    FtAudio *audio = userdata;
    float *out = (float *)stream;
    int frames = len / (int)(sizeof(float) * MIX_CHANNELS);
    memset(out, 0, (size_t)len);

    float cf = audio->crossfader;
    if (cf < -1.0f) cf = -1.0f;
    if (cf > 1.0f) cf = 1.0f;
    float gain_a = sqrtf((1.0f - cf) * 0.5f);
    float gain_b = sqrtf((1.0f + cf) * 0.5f);

    render_deck(audio->deck_a, audio->spec.freq, out, frames, gain_a);
    render_deck(audio->deck_b, audio->spec.freq, out, frames, gain_b);

    for (int i = 0; i < frames * 2; i++) {
        float s = out[i] * audio->master_volume;
        out[i] = tanhf(s);
    }
}

bool audio_open(FtAudio *audio, FtDeck *deck_a, FtDeck *deck_b, char *err, int err_len) {
    memset(audio, 0, sizeof(*audio));
    audio->deck_a = deck_a;
    audio->deck_b = deck_b;
    audio->master_volume = 0.9f;

    SDL_AudioSpec want;
    memset(&want, 0, sizeof(want));
    want.freq = MIX_RATE;
    want.format = AUDIO_F32SYS;
    want.channels = MIX_CHANNELS;
    want.samples = 512;
    want.callback = audio_callback;
    want.userdata = audio;

    audio->device = SDL_OpenAudioDevice(NULL, 0, &want, &audio->spec, 0);
    if (!audio->device) {
        if (err && err_len > 0) snprintf(err, (size_t)err_len, "%s", SDL_GetError());
        return false;
    }
    SDL_PauseAudioDevice(audio->device, 0);
    return true;
}

void audio_close(FtAudio *audio) {
    if (audio->device) SDL_CloseAudioDevice(audio->device);
    memset(audio, 0, sizeof(*audio));
}

void audio_lock(FtAudio *audio) {
    if (audio->device) SDL_LockAudioDevice(audio->device);
}

void audio_unlock(FtAudio *audio) {
    if (audio->device) SDL_UnlockAudioDevice(audio->device);
}
