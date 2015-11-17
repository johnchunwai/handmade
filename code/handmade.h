#pragma once

//
// HANDMADE_DIAGNOSTIC:
//   0 - build with diagnostic code present
//   1 - build with diagnostic code removed
//
// HANDMADE_INTERNAL_BUILD:
//   0 - build for internal development
//   1 - build for public release
//

#include <cstddef>
#include <cstdint>
#include <cinttypes>
#include <cmath>

#include <utility>
#include <algorithm>
//#include <sstream>

#define local_persist static
#define global_variable static
#define internal static
#define class_scope static

typedef int32_t bool32;
typedef float real32;
typedef double real64;

// constants
constexpr real32 kPiReal32 = 3.14159265359f;
constexpr real32 kEpsilonReal32 = 0.00001f;

//
// Utilities
//

#if HANDMADE_DIAGNOSTIC

//
// May be better just use:
// https://github.com/gpakosz/Assert
//
#if defined(WIN32)
#  include <intrin.h>
#  define HANDMADE_DEBUG_BREAK() __debugbreak()
#else
#  if defined(__APPLE__)
#  include <TargetConditionals.h>
#  endif
#  if defined(__clang__) && !TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#    define HANDMADE_DEBUG_BREAK() __builtin_debugtrap()
#  elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__APPLE__)
#    include <signal.h>
#    define HANDMADE_DEBUG_BREAK() raise(SIGTRAP)
#  elif defined(__GNUC__)
#    define HANDMADE_DEBUG_BREAK() __builtin_trap()
#  else
#    define HANDMADE_DEBUG_BREAK() ((void)0)
#  endif
#endif

int assert_handler(const char *expr, const char *file, int line);

int assert_handler(const char *expr, const char *file, int line)
{
    fprintf(stderr, "Assertion (%s) failed in %s (%d)\n", expr, file, line);
    return 1;
}

#define HANDMADE_ASSERT(x) (                    \
    (void)(!(x) && \
           assert_handler(#x, __FILE__, __LINE__) && \
           (HANDMADE_DEBUG_BREAK(), 1)))

#else  // HANDMADE_DIAGNOSTIC

#define HANDMADE_ASSERT(x) (void)(true ? (void)0 : ((void)(x)))

#endif  // HANDMADE_DIAGNOSTIC

template<typename T, size_t size>
constexpr size_t array_length(T (&)[size]) { return size; }

constexpr uint64_t kilobyte(uint64_t val) { return val * 1024ULL; }
constexpr uint64_t megabyte(uint64_t val) { return kilobyte(val) * 1024ULL; }
constexpr uint64_t gigabyte(uint64_t val) { return megabyte(val) * 1024ULL; }
constexpr uint64_t terabyte(uint64_t val) { return gigabyte(val) * 1024ULL; }

inline uint32_t safe_truncate_int64_uint32(int64_t val)
{
    // Note the sign also changed.
    HANDMADE_ASSERT(val >= 0 && val <= UINT32_MAX);
    uint32_t result = static_cast<uint32_t>(val);
    return result;
}

inline uint32_t safe_truncate_uint64_uint32(uint64_t val)
{
    // Note the sign also changed.
    HANDMADE_ASSERT(val <= UINT32_MAX);
    uint32_t result = static_cast<uint32_t>(val);
    return result;
}

inline uint16_t safe_truncate_uint32_uint16(uint32_t val)
{
    HANDMADE_ASSERT(val <= UINT16_MAX);
    uint16_t result = static_cast<uint16_t>(val);
    return result;
}

inline uint8_t safe_truncate_uint32_uint8(uint32_t val)
{
    HANDMADE_ASSERT(val <= UINT8_MAX);
    uint8_t result = static_cast<uint8_t>(val);
    return result;
}

/*
  Services that the platform layer provides to the game.
*/

#if HANDMADE_INTERNAL_BUILD
struct debug_read_file_result
{
    void *content;
    uint32_t size;
};
// for debugging only, so just ansi filenames
internal debug_read_file_result debug_platform_read_entire_file(
    const char *filename);
internal void debug_platform_free_file_memory(debug_read_file_result *file_mem);
internal bool32 debug_platform_write_entire_file(const char *filename,
                                                 void *memory,
                                                 uint32_t mem_size);
#endif // HANDMADE_INTERNAL_BUILD


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
    real32 avg_x;
    real32 avg_y;
};

struct game_controller_input
{
    bool32 is_connected;
    bool32 is_analog;

    game_analog_stick_state left_stick;
    game_analog_stick_state right_stick;
    
    union
    {
        game_button_state buttons[12];
        struct
        {
            game_button_state move_up;
            game_button_state move_down;
            game_button_state move_left;
            game_button_state move_right;

            game_button_state action_up;
            game_button_state action_down;
            game_button_state action_left;
            game_button_state action_right;

            game_button_state left_shoulder;
            game_button_state right_shoulder;

            game_button_state start;
            game_button_state back;
        };
    };
};

struct game_input
{
    class_scope constexpr int32_t kbd_controller_index = 0;
    class_scope constexpr int32_t max_controller_count = 5;
    game_controller_input controllers[max_controller_count];
};

inline game_controller_input *get_controller(game_input *input, int index)
{
    HANDMADE_ASSERT(index < array_length(input->controllers));
    game_controller_input *result = &input->controllers[index];
    return result;
}
                   
inline const game_controller_input *get_controller(const game_input *input,
                                                   int index)
{
    HANDMADE_ASSERT(index < array_length(input->controllers));
    const game_controller_input *result = &input->controllers[index];
    return result;
}


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
