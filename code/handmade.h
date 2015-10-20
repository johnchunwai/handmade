
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

internal void game_update_and_render(GameOffscreenBuffer * buffer, int32_t blue_offset, int32_t green_offset);

/*
  TODO: Services that the platform layer provides to the game.
*/
