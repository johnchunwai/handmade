#pragma once


/*
  Utilities
*/
template<typename T, size_t size>
constexpr size_t array_length(T (&arr)[size]) { return size; }

/*
  NOTE: Services that the game provides to the platform layer.
*/
// 4 THINGS: timing, controller/keyboard input, bitmap buffer to use, sound buffer to use
struct game_offscreen_buffer
{
    // pixels are 32-bit wide, memory order BB GG RR xx
    int32_t width;
    int32_t height;
    ptrdiff_t pitch;
    void *memory;
};

struct game_sound_buffer
{
    int16_t *samples;
    uint32_t sample_count;
    uint32_t samples_per_sec;
};

struct win32_sound_output
{
    uint32_t running_sample_index;
    uint32_t num_sound_ch;
    uint32_t samples_per_sec;
    uint32_t sec_to_buffer;

    // calculated values
    uint32_t latency_sample_count;  // to minimize delay when we want to change the sound
    uint32_t bytes_per_sample;
    uint32_t sound_buffer_size;
};

struct game_button_state
{
    int32_t num_half_transition;
    bool32 ended_down;
};

struct game_analog_stick_state
{
    float start_x;
    float start_y;

    float min_x;
    float min_y;

    float max_x;
    float max_y;

    float end_x;
    float end_y;
};

struct game_controller_input
{
    bool32 is_analog;

    game_analog_stick_state left_stick;
    game_analog_stick_state right_stick;
    
    union
    {
        game_button_state buttons[6];
        struct
        {
            game_button_state a;
            game_button_state b;
            game_button_state x;
            game_button_state y;
            game_button_state left_shoulder;
            game_button_state right_shoulder;
        };
    };
};

struct game_input
{
    static constexpr uint32_t max_controller_count = 4;
    game_controller_input controllers[max_controller_count];
};

internal void game_update_and_render(game_offscreen_buffer *buffer,
                                     game_sound_buffer *sound_buffer,
                                     const game_input *input);

/*
  TODO: Services that the platform layer provides to the game.
*/
