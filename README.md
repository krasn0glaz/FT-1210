# FT-1210

Native module DJ deck inspired by PT-1210, aimed at Linux first and macOS second.

The intent is not to clone PT-1210's Amiga code. This project keeps the same ideology: tracker modules as DJ material, with turntable-style pitch, nudging, cueing, pattern loops, and a simple two-deck mixer.

## Current Status

Early scaffold with:

- Two decks backed by `libopenmpt`.
- SDL2 audio callback and direct SDL UI.
- Drag/drop module loading into the selected deck.
- Built-in bitmap labels; no external font runtime dependency.
- Clickable play/cue/sync/loop buttons, pitch/volume/EQ faders, and crossfader.
- In-app module browser for loading files without drag/drop.
- Traktor-style split: decks on the sides, EQ/volume/gain/crossfader in the central mixer.
- Tracker-specific deck view: pattern/order/row, pattern blocks, channel activity background.
- Vinyl-style repitch: changing pitch changes playback speed and musical pitch together. There is intentionally no master-tempo/time-stretch mode.
- Cue, seek, pitch, nudge, sync target, pattern-order loop state.
- Per-deck volume, gain slot, 3-band DJ EQ, VU meters, crossfader.

## Formats

`libopenmpt` handles `.mod`, `.xm`, `.it`, `.s3m`, `.mptm`, many Amiga tracker variants, and imports OctaMED `.med` according to OpenMPT's format documentation. Exact MED behavior should be tested against real files.

## Build

With Nix:

```sh
nix develop
cmake -S . -B build -G Ninja
cmake --build build
./build/ft1210
```

Without Nix, install:

- C compiler
- CMake
- pkg-config
- SDL2 development package
- libopenmpt development package

Then run the same CMake commands.

## Controls

- `drag/drop`: load module into selected deck
- `B`: open in-app module browser for selected deck
- browser click: enter directory or load module
- `Esc`: close browser, or quit if browser is closed
- mouse click deck: select deck
- mouse drag pitch fader, mixer volume fader, gain knob, EQ knobs, or crossfader
- mouse wheel over gain/EQ knobs: fine adjust
- double click pitch/gain/EQ/volume/crossfader: reset to neutral
- `1`, `2`: select deck A/B
- `Tab`: toggle selected deck
- `Space`: play/pause selected deck
- `C`: jump to cue; creates cue at current pattern if none is set
- `F3`: set cue to current pattern start
- `F1`: jump to cue
- right click `CUE` or `SET`: clear cue
- `Left`, `Right`: temporary nudge selected deck
- `N`: clear nudge immediately
- `Up`, `Down`: pitch selected deck by 1 percent
- `Shift+Up`, `Shift+Down`: pitch by 0.1 percent
- `Home`: reset pitch
- `PageUp`, `PageDown`: seek +/- 5 seconds
- click the bottom order bar: seek to that pattern/order
- `L`: pattern/order loop cycle/toggle
- `F5`: toggle loop
- `F6` / `F7`: decrease/increase loop length (`4/8/16/32/64/128` rows, then `1/2/4/8` patterns)
- `I`: set loop in at current order
- `O`: set loop out at current order and activate loop
- `S`: sync selected deck to the other deck's effective BPM target
- `Q/A`: hi EQ up/down
- `W/Z`: mid EQ up/down
- `E/D`: low EQ up/down
- `=`, `-`: selected deck volume up/down
- `[`, `]`: crossfader left/right
- `Esc`: quit
- click channel scope/header in the pattern table: toggle channel mute state

## Design Notes

- Audio renders each module from OpenMPT into the SDL callback, then mixes both decks.
- Pitch is implemented by changing the render sample rate relative to the hardware output rate. This gives vinyl/CDJ-style repitching: faster playback is higher pitch, slower playback is lower pitch.
- Cue points are stored as tracker order/row positions. Current cue defaults to the start row of the active pattern.
- Loops support row lengths and multi-pattern lengths. Slip mode is not implemented yet.
- Channel scopes are oscilloscope-style traces driven by libopenmpt per-channel VU history. Channel muting uses `libopenmpt_ext`'s interactive channel mute API.
- Displayed BPM is derived from tracker tempo and speed as `tempo * 6 / speed`, then adjusted by vinyl pitch. This keeps ProTracker speed-4 modules from incorrectly appearing as 125 BPM.
- Sync is BPM-target based. Because arbitrary modules do not always expose a trustworthy BPM, this needs a proper BPM model next: parse tracker tempo/speed where possible, allow manual BPM entry, then sync pitch from deck A to B or B to A.

## Next Steps

- Real text rendering and clickable controls.
- Native file dialog/menu loading.
- Row-accurate pattern loops and slip loop mode.
- Manual BPM editing and tap tempo.
- Pattern/order visualizer from OpenMPT metadata.
- Optional libxmp fallback if specific MED files fail in OpenMPT.
