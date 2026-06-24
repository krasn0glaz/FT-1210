#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "deck.h"
#include "ui.h"

static void load_into_deck(FtAudio *audio, FtDeck *deck, const char *path) {
    char err[256];
    audio_lock(audio);
    bool ok = deck_load(deck, path, audio->spec.freq ? audio->spec.freq : 48000, err, sizeof(err));
    audio_unlock(audio);
    if (!ok) {
        fprintf(stderr, "load failed: %s: %s\n", path, err);
    }
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void decode_uri_path(char *dst, size_t dst_len, const char *src) {
    if (strncmp(src, "file://", 7) == 0) src += 7;
    if (strncmp(src, "localhost/", 10) == 0) src += 9;
    size_t w = 0;
    for (size_t r = 0; src[r] && src[r] != '\n' && src[r] != '\r' && w + 1 < dst_len; r++) {
        if (src[r] == '%' && src[r + 1] && src[r + 2]) {
            int hi = hex_value(src[r + 1]);
            int lo = hex_value(src[r + 2]);
            if (hi >= 0 && lo >= 0) {
                dst[w++] = (char)((hi << 4) | lo);
                r += 2;
                continue;
            }
        }
        dst[w++] = src[r];
    }
    dst[w] = '\0';
}

static FtDeck *drop_target_deck(FtUi *ui, FtDeck decks[2]) {
    int x = 0, y = 0;
    SDL_GetMouseState(&x, &y);
    int deck_index = ui_deck_at(ui, x, y);
    if (deck_index == 0 || deck_index == 1) {
        decks[0].selected = deck_index == 0;
        decks[1].selected = deck_index == 1;
        return &decks[deck_index];
    }
    return decks[0].selected ? &decks[0] : &decks[1];
}

static void load_drop_payload(FtAudio *audio, FtUi *ui, FtDeck decks[2], const char *payload) {
    char path[2048];
    const char *p = payload;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (!*p) return;
    decode_uri_path(path, sizeof(path), p);
    if (path[0]) {
        load_into_deck(audio, drop_target_deck(ui, decks), path);
    }
}

int main(int argc, char **argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    FtDeck decks[2];
    deck_init(&decks[0]);
    deck_init(&decks[1]);
    decks[0].selected = true;

    FtAudio audio;
    char err[256];
    if (!audio_open(&audio, &decks[0], &decks[1], err, sizeof(err))) {
        fprintf(stderr, "audio failed: %s\n", err);
        SDL_Quit();
        return 1;
    }

    FtUi ui;
    if (!ui_open(&ui, err, sizeof(err))) {
        fprintf(stderr, "ui failed: %s\n", err);
        audio_close(&audio);
        SDL_Quit();
        return 1;
    }

    for (int i = 1; i < argc && i <= 2; i++) {
        load_into_deck(&audio, &decks[i - 1], argv[i]);
    }

    while (ui.running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_DROPFILE || event.type == SDL_DROPTEXT) {
                load_drop_payload(&audio, &ui, decks, event.drop.file);
                SDL_free(event.drop.file);
            } else {
                ui_handle_event(&ui, &audio, decks, &event);
            }
        }
        ui_render(&ui, &audio, decks);
        SDL_Delay(16);
    }

    ui_close(&ui);
    audio_close(&audio);
    deck_close(&decks[0]);
    deck_close(&decks[1]);
    SDL_Quit();
    return 0;
}
