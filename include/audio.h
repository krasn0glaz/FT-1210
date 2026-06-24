#ifndef FT1210_AUDIO_H
#define FT1210_AUDIO_H

#include <SDL.h>

#include "deck.h"

typedef struct FtAudio {
    SDL_AudioDeviceID device;
    SDL_AudioSpec spec;
    FtDeck *deck_a;
    FtDeck *deck_b;
    float crossfader;
    float master_volume;
} FtAudio;

bool audio_open(FtAudio *audio, FtDeck *deck_a, FtDeck *deck_b, char *err, int err_len);
void audio_close(FtAudio *audio);
void audio_lock(FtAudio *audio);
void audio_unlock(FtAudio *audio);

#endif
