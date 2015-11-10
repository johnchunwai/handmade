#pragma once

/*
  HANDMADE_DIAGNOSTIC:
    0 - build with diagnostic code present
    1 - build with diagnostic code removed

  HANDMADE_DEV_BUILD:
    0 - build for internal development
    1 - build for public release
 */

/*
  Utilities
*/
#if HANDMADE_DIAGNOSTIC
#define assert(expr) { if (!(expr)) { *(int*)0 = 0; } }
#else
#define assert(expr)
#endif

template<typename T, size_t size>
constexpr size_t array_length(T (&arr)[size]) { return size; }

constexpr uint64_t kilobyte(uint64_t val) { return val * 1024ULL; }
constexpr uint64_t megabyte(uint64_t val) { return kilobyte(val) * 1024ULL; }
constexpr uint64_t gigabyte(uint64_t val) { return megabyte(val) * 1024ULL; }
constexpr uint64_t terabyte(uint64_t val) { return gigabyte(val) * 1024ULL; }

inline uint32_t safe_truncate_uint64(uint64_t val)
{
    assert(val <= UINT32_MAX);
    uint32_t result = static_cast<uint32_t>(val);
    return result;
}

/*
  TODO: Services that the platform layer provides to the game.
*/

#if HANDMADE_DEV_BUILD
struct debug_read_file_result
{
    void *content;
    uint32_t size;
};
// for debugging only, so just ansi filenames
internal debug_read_file_result debug_platform_read_entire_file(
    const char *filename);
internal void debug_platform_free_file_memory(void *memory);
internal bool32 debug_platform_write_entire_file(const char *filename,
                                                 uint32_t mem_size,
                                                 void *memory);
#endif // HANDMADE_DEV_BUILD


/*
  NOTE: Services that the game provides to the platform layer.
*/
// 4 THINGS: timing, controller/keyboard input, bitmap buffer to use, sound
//           buffer to use
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
    uint32_t latency_sample_count;  // to minimize delay when changing the sound
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
    real32 start_x;
    real32 start_y;

    real32 min_x;
    real32 min_y;

    real32 max_x;
    real32 max_y;

    real32 end_x;
    real32 end_y;
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
    class_scope constexpr int32_t max_controller_count = 4;
    game_controller_input controllers[max_controller_count];
};

struct game_memory
{
    bool32 is_initialized;

    void *permanent_storage;  // required to be cleared to 0 at startup
    uint64_t permanent_storage_size;
    void *transient_storage;  // required to be cleared to 0 at startup
    uint64_t transient_storage_size;
};

internal void game_update_and_render(game_memory *memory,
                                     game_offscreen_buffer *buffer,
                                     game_sound_buffer *sound_buffer,
                                     const game_input *input);

/*
 */

struct game_state
{
    int32_t blue_offset;
    int32_t green_offset;
    real32 tone_hz;
};
