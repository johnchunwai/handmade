#include "handmade.h"


internal void render_weird_gradient(GameOffscreenBuffer *buffer, int32_t blue_offset, int32_t green_offset)
{
    // draw something
    uint8_t *row = static_cast<uint8_t*>(buffer->memory);
    for (int32_t y = 0; y < buffer->height; ++y)
    {
        uint32_t* pixel = reinterpret_cast<uint32_t*>(row);
        for (int32_t x = 0; x < buffer->width; ++x)
        {
            /*
              pixel in memory:
              BITMAPINFOHEADER's biBitCount mentions that order is BB GG RR XX.

              Since we're in little endian (least sig byte in lower addr),
              32bit int representation is 0xXXRRGGBB

              Memory:    BB GG RR xx
              Register:  xx RR GG BB
            */
            uint8_t red = 0;
            uint8_t green = static_cast<uint8_t>(y + green_offset);
            uint8_t blue = static_cast<uint8_t>(x + blue_offset);
            // little endian - least sig val on smallest addr
            *pixel++ = (red << 16) | (green << 8) | blue;
        }
        row += buffer->pitch;
    }
}

internal void game_update_and_render(GameOffscreenBuffer *buffer, int32_t blue_offset, int32_t green_offset)
{
    render_weird_gradient(buffer, blue_offset, green_offset);
}
