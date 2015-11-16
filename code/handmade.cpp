#include "handmade.h"

internal void game_output_sound(game_sound_buffer *sound_buffer, real32 tone_hz)
{
    // Just do a sine wave
    local_persist real32 sine_t = 0.0f;
    int16_t tone_volume = 1000;
    real32 wave_period_sample_count =
            static_cast<real32>(sound_buffer->samples_per_sec) / tone_hz;
    
    int16_t *sample_out = sound_buffer->samples;
    for (uint32_t sample_index = 0;
         sample_index < sound_buffer->sample_count;
         ++sample_index)
    {
        real32 sine_val = std::sin(sine_t);
        int16_t sample_val = static_cast<int16_t>(sine_val * tone_volume);
        *sample_out++ = sample_val;
        *sample_out++ = sample_val;
        // advance sine t by 1 sample
        sine_t += 1.0f * 2.0f * kPiReal32 / wave_period_sample_count;
        // cap the sine t within a 2*PI to avoid losing floating point precision
        // when sine t is large
        if (sine_t > 2.0f * kPiReal32)
        {
            sine_t -= 2.0f * kPiReal32;
        }
    }
}

internal void render_weird_gradient(game_offscreen_buffer *buffer,
                                    int32_t blue_offset, int32_t green_offset)
{
    // draw something
    uint8_t *row = static_cast<uint8_t*>(buffer->memory);
    for (int32_t y = 0; y < buffer->height; ++y)
    {
        uint32_t *pixel = reinterpret_cast<uint32_t*>(row);
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
            *pixel++ = ((uint32_t)red << 16) | ((uint32_t)green << 8) | blue;
        }
        row += buffer->pitch;
    }
}

internal void game_update_and_render(game_memory *memory,
                                     game_offscreen_buffer *buffer,
                                     game_sound_buffer *sound_buffer,
                                     const game_input *input)
{
    HANDMADE_ASSERT(sizeof(game_state) <= memory->permanent_storage_size);
        
    game_state *state =
            static_cast<game_state*>(memory->permanent_storage);
    if (!memory->is_initialized)
    {
        const char *filename = __FILE__;
        debug_read_file_result read_result =
                debug_platform_read_entire_file(filename);
        if (read_result.content)
        {
            debug_platform_write_entire_file("test.out", read_result.content,
                                             read_result.size);
            debug_platform_free_file_memory(&read_result);
        }
        
        // memory is already zeroed
        // state->blue_offset = 0;
        // state->green_offset = 0;
        state->tone_hz = 256.0f;
        memory->is_initialized = true;
    }
    
    const game_controller_input *controller0 = &input->controllers[0];
    const game_controller_input *kbd_controller = &input->kbd_controller;
    if (controller0->is_analog)
    {
        state->tone_hz = 256.0f + 128.0f * controller0->right_stick.end_y;
        state->blue_offset -= static_cast<int>(
            controller0->left_stick.end_x * 10.0f);
        // green_offset += static_cast<int>(controller0->left_stick.end_y * 5.0f);
    }
    else
    {
    }

    if (controller0->a.ended_down || kbd_controller->a.ended_down)
    {
        state->green_offset += 1;
    }
    // if (left_stick_x > kEpsilonReal32 || left_stick_x < -kEpsilonReal32)
    // {
    // }
    // if (left_stick_y > kEpsilonReal32 || left_stick_y < -kEpsilonReal32)
    // {
    // }
    // if (x_button)
    // {
    //     blue_offset += 5;

    //     // Add some vibration left motor
    //     XINPUT_VIBRATION vibration {};
    //     vibration.wLeftMotorSpeed = 65535;
    //     XInputSetState(controller_index, &vibration);
    // }
    // if (y_button)
    // {
    //     green_offset += 2;

    //     // Add some vibration right motor
    //     XINPUT_VIBRATION vibration {};
    //     vibration.wRightMotorSpeed = 10000;
    //     XInputSetState(controller_index, &vibration);
    // }
    // if (a_button)
    // {
    //     XINPUT_VIBRATION vibration {};
    //     XInputSetState(controller_index, &vibration);
    // }

    // TODO: allow sample offsets here for more robust platform options
    game_output_sound(sound_buffer, state->tone_hz);
    render_weird_gradient(buffer, state->blue_offset,
                          state->green_offset);
}
