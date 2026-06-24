#ifndef FT1210_UI_H
#define FT1210_UI_H

#include <SDL.h>

#include "audio.h"
#include "deck.h"

typedef struct FtUi {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int width;
    int height;
    int drag_kind;
    int drag_deck;
    int drag_band;
    bool browser_open;
    int browser_deck;
    bool running;
    bool file_dialog_requested;
} FtUi;

bool ui_open(FtUi *ui, char *err, int err_len);
void ui_close(FtUi *ui);
void ui_handle_event(FtUi *ui, FtAudio *audio, FtDeck decks[2], const SDL_Event *event);
void ui_render(FtUi *ui, const FtAudio *audio, const FtDeck decks[2]);
int ui_deck_at(const FtUi *ui, int x, int y);

#endif
