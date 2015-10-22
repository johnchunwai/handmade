
#pragma once

/*
  NOTE: Services that the game provides to the platform layer.
*/
// 4 THINGS: timing, controller/keyboard input, bitmap buffer to use, sound buffer to use
struct GameOffscreenBuffer
{
    // pixels are 32-bit wide, memory order BB GG RR xx
    int32_t width;
    int32_t height;
    ptrdiff_t pitch;
    void *memory;
};

struct GameSoundBuffer
{
    int16_t *samples;
    uint32_t sample_count;
    uint32_t samples_per_sec;
};

internal void game_update_and_render(GameOffscreenBuffer * buffer,
                                     int32_t blue_offset, int32_t green_offset,
                                     GameSoundBuffer *sound_buffer, uint32_t tone_hz);

/*
  TODO: Services that the platform layer provides to the game.
*/
