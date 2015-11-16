/*
  TODO: Not the final platform layer.

  - saved game locations
  - getting a handle to our own exe file
  - asset loading path
  - threading
  - raw input (support for multiple keyboards)
  - sleep/timeBeginPeriod
  - ClipCursor() (for multimonitor support)
  - fullscreen support
  - WM_SETCURSOR (control cursor visibility)
  - QueryCancelAutoplay
  - WM_ACTIVATEAPP
  - blit speed improvements (BitBlt)
  - hardware acceleration (OpenGL or Direct3D or both??)
  - GetKeyboardLayout (for French keyboards, international WASD support)

  Just a partial list of stuff!
*/


//
// Global stuff that are platform specific but applies to game.
//

// printf macros that are not yet defined
#if __SIZEOF_SIZE_T__ == 8
#define __PRIS_PREFIX "z"
#elif __SIZEOF_SIZE_T__ == 4
#define __PRIS_PREFIX
#else
#error "Unsupported __SIZEOF_SIZE_T__ " #__SIZEOF_SIZE_T__
#endif // __SIZEOF_SIZE_T__

// #define PRIdS __PRIS_PREFIX "d"
// #define PRIxS __PRIS_PREFIX "x"
#define PRIuS __PRIS_PREFIX "u"
// #define PRIXS __PRIS_PREFIX "X"
// #define PRIoS __PRIS_PREFIX "o"


#include "handmade.h"
#include "handmade.cpp"

/*
  Platform specific stuff below
*/
#include <cstdio>
#include <cstring>

#include <chrono>
// #include <iostream>

typedef std::chrono::duration<real32, std::ratio<1, 1000> > chrono_duration_ms;


#ifdef WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <intrin.h>


internal void* platform_alloc_zeroed(void *base_addr, size_t length)
{
    // Guarantee to be allocation granularity (64KB) aligned
    // commited to page boundary (4KB), but the rest are wasted space
    // memory auto clears to 0
    // freed automatically when app terminates
    void *memory = VirtualAlloc(base_addr, length,
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    HANDMADE_ASSERT(memory);
    return memory;
}

internal void platform_free(void *memory, size_t)
{
    VirtualFree(memory, 0, MEM_RELEASE);
}

#elif __linux__

#include <sys/mman.h>
#include <x86intrin.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif  // MAP_ANONYMOUS

internal void* platform_alloc_zeroed(void *base_addr, size_t length)
{
    void *memory = mmap(base_addr,
                        length,
                        PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE,
                        -1, 0);
    HANDMADE_ASSERT(memory && memory != MAP_FAILED);
    return memory;
}

internal void platform_free(void *memory, size_t length)
{
    munmap(memory, length);
}

#if __clang__
internal __inline__ uint64_t __rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (uint64_t)lo)|( ((uint64_t)hi)<<32 );
}
#endif  // __clang__

#endif  // platform check

#include <SDL.h>

// constants
constexpr const char *kSdlControllerMappingFile = "./data/sdl_gamecontroller_db/gamecontrollerdb.txt";
constexpr real32 kSdlControllerMaxStickVal = 32767.0f;
// constexpr real32 kSdlControllerMinStickVal = -32768;

internal real32 sdl_get_controller_stick_normalized_deadzone(
    real32 unnormalized_deadzone)
{
    return unnormalized_deadzone / std::sqrt(
        kSdlControllerMaxStickVal * kSdlControllerMaxStickVal * 2.0f);
}

struct sdl_offscreen_buffer
{
    SDL_Texture *texture;
    int32_t width;
    int32_t height;
    ptrdiff_t pitch;
    void *memory;
};

struct sdl_sound_ring_buffer
{
    size_t size;
    volatile size_t play_cursor;
    void *memory;
};

struct sdl_sound_output
{
    sdl_sound_ring_buffer ring_buffer;
    uint32_t running_sample_index;
    uint32_t num_sound_ch;
    uint32_t samples_per_sec;
    uint32_t sec_to_buffer;
    SDL_AudioFormat sdl_audio_format;
    uint32_t sdl_audio_buffer_size_in_samples;

    // calculated values
    uint32_t latency_sample_count;  // to minimize delay when we want to change the sound
    uint32_t bytes_per_sample;
    uint32_t sdl_audio_buffer_size_in_bytes;
};

struct sdl_game_controllers
{
    SDL_GameController *controllers[game_input::max_controller_count];
    SDL_Haptic *haptics[game_input::max_controller_count];
    int32_t num_controllers_active;
};

// globals
global_variable bool32 g_running = false;
global_variable sdl_offscreen_buffer g_backbuffer {};


internal void sdl_log_error(const char* func_name)
{
    printf("%s failed: %s\n", func_name, SDL_GetError());
}

#if HANDMADE_INTERNAL_BUILD

// for debugging only, so just ansi filenames
internal debug_read_file_result debug_platform_read_entire_file(
    const char *filename)
{
    // open file
    // get file size
    // alloc buffer
    // read entire file
    // close file
    debug_read_file_result result {};
    SDL_RWops *file = SDL_RWFromFile(filename, "rb");
    if (file)
    {
        int64_t filesize = SDL_RWsize(file);
        if (filesize >= 0)
        {
            result.size = safe_truncate_int64_uint32(filesize);
            result.content = platform_alloc_zeroed(nullptr, result.size);
            if (result.content)
            {
                if (1 == SDL_RWread(file, result.content, result.size, 1))
                {
                    // NOTE: file read successfully
                }
                else
                {
                    sdl_log_error("SDL_RWread");
                    debug_platform_free_file_memory(&result);
                }
            }
            else
            {
                // TODO: logging
            }
        }
        else
        {
            // TODO: logging
            sdl_log_error("SDL_RWops");
        }
        if (0 != SDL_RWclose(file))
        {
            sdl_log_error("SDL_RWclose");
        }
    }
    else
    {
        // TODO: logging
        sdl_log_error("SDL_RWFromFile");
    }
    return result;
}

internal void debug_platform_free_file_memory(debug_read_file_result *file_mem)
{
    platform_free(file_mem->content, file_mem->size);
    file_mem->content = nullptr;
    file_mem->size = 0;
}

internal bool32 debug_platform_write_entire_file(const char *filename,
                                                 void *memory,
                                                 uint32_t mem_size)
{
    // open file
    // write entire file
    // close file
    bool32 result = false;
    SDL_RWops *file = SDL_RWFromFile(filename, "wb");
    if (file)
    {
        if (SDL_RWwrite(file, memory, mem_size, 1) == 1)
        {
            result = true;
        }
        else
        {
            // TODO: logging
            sdl_log_error("SDL_RWwrite");
        }
        if (0 != SDL_RWclose(file))
        {
            sdl_log_error("SDL_RWclose");
        }
    }
    else
    {
        // TODO: logging
        sdl_log_error("SDL_RWFromFile");
    }

    return result;
}

#endif // HANDMADE_INTERNAL_BUILD

internal void sdl_cleanup(SDL_Window *window, SDL_Renderer *renderer,
                          SDL_Texture *texture, SDL_AudioDeviceID audio_dev_id,
                          sdl_game_controllers *controllers)
{
    if (controllers)
    {
        for (int32_t i = 0; i < game_input::max_controller_count; ++i)
        {
            auto *controller = controllers->controllers[i];
            if (controller)
            {
                auto *haptic = controllers->haptics[i];
                if (haptic)
                {
                    SDL_HapticClose(haptic);
                }
                SDL_GameControllerClose(controller);
            }
        }
    }

    if (audio_dev_id != 0)
    {
        SDL_CloseAudioDevice(audio_dev_id);
    }
    
    if (texture)
    {
        SDL_DestroyTexture(texture);
    }
    if (renderer)
    {
        SDL_DestroyRenderer(renderer);
    }
    if (window)
    {
        SDL_DestroyWindow (window);
    }
    SDL_Quit();
}

internal bool32 sdl_resize_backbuffer(sdl_offscreen_buffer *buffer,
                                      SDL_Renderer *renderer,
                                      int32_t width, int32_t height)
{
    bool32 succeeded = true;
    constexpr int32_t bytes_per_pixel = 4;
    // TODO: bulletproof this.
    // maybe don't free first, free after.
    if (buffer->memory)
    {
        int32_t mem_size = buffer->width * buffer->height * bytes_per_pixel;
        platform_free(buffer->memory, static_cast<size_t>(mem_size));
        buffer->memory = nullptr;
    }

    buffer->texture = SDL_CreateTexture(renderer,
                                        SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING,
                                        width, height);
    if (!buffer->texture)
    {
        printf("SDL_CreateTexture error: %s\n", SDL_GetError());
        succeeded = false;
        return succeeded;
    }
    
    buffer->width = width;
    buffer->height = height;
    buffer->pitch = buffer->width * bytes_per_pixel;

    int32_t mem_size = buffer->width * buffer->height * bytes_per_pixel;
    buffer->memory = platform_alloc_zeroed(nullptr,
                                           static_cast<size_t>(mem_size));

    return succeeded;
}

internal void sdl_audio_callback(void *userdata, uint8_t* stream, int32_t len);

internal SDL_AudioDeviceID sdl_init_sound(sdl_sound_output *sound_output)
{
    // for (int i = 0, count = SDL_GetNumAudioDevices(0); i < count; ++i)
    // {
    //     printf("Audio device %d: %s\n", i, SDL_GetAudioDeviceName(i, 0));
    // }
    
    SDL_AudioSpec desired {};
    SDL_AudioSpec obtained {};
    desired.freq = static_cast<int>(sound_output->samples_per_sec);
    desired.format = sound_output->sdl_audio_format;
    desired.channels = safe_truncate_uint32_uint8(sound_output->num_sound_ch);
    desired.samples = safe_truncate_uint32_uint16(
        sound_output->sdl_audio_buffer_size_in_samples);
    desired.callback = sdl_audio_callback;
    desired.userdata = static_cast<void*>(&sound_output->ring_buffer);
    SDL_AudioDeviceID audio_dev_id = SDL_OpenAudioDevice(
        nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    // SDL_AudioSpec &obtained = desired;
    if (audio_dev_id == 0)
    {
        printf("Failed to init audio: %s\n", SDL_GetError());
    }
    else if (desired.format != obtained.format ||
             desired.freq != obtained.freq ||
             desired.channels != obtained.channels)
    {
        printf("Failed to obtain desired audio format. No sound\n");
        SDL_CloseAudioDevice(audio_dev_id);
        audio_dev_id = 0;
    }
    else
    {
        printf("Init audio: samples=%d (want=%d), buffer size=%d, silience=%d\n",
               obtained.samples, desired.samples,
               obtained.size, obtained.silence);
        sound_output->sdl_audio_buffer_size_in_samples = obtained.samples;
        sound_output->sdl_audio_buffer_size_in_bytes = obtained.size;
        HANDMADE_ASSERT(obtained.silence == 0);
        // init ring buffer
        sound_output->ring_buffer.memory = platform_alloc_zeroed(
            nullptr, sound_output->ring_buffer.size);
    }

    return audio_dev_id;
}

internal void sdl_fill_sound_buffer(sdl_sound_output *sound_output,
                                    const game_sound_buffer *source_buffer,
                                    size_t byte_to_lock,
                                    size_t bytes_to_write)
{
    if (bytes_to_write == 0)
    {
        return;
    }
    // int16_t int16_t int16_t ...
    // [left   right]  [left   right] ...
    // since it's a circular buffer, the lock region might be consecutive, or
    // chopped into 2 regions:
    // one at the back of the buffer, the other at the front
    constexpr int32_t max_regions = 2;
    uint8_t *regions[max_regions] = {};
    size_t region_sizes[max_regions] = {};
    // int16_t* samples = source_buffer->samples;
    uint8_t *samples = reinterpret_cast<uint8_t*>(source_buffer->samples);
    uint8_t *ring_buffer_memory = static_cast<uint8_t*>(
        sound_output->ring_buffer.memory);
    regions[0] =  ring_buffer_memory + byte_to_lock;
    region_sizes[0] = bytes_to_write;
    if (byte_to_lock + bytes_to_write > sound_output->ring_buffer.size)
    {
        region_sizes[0] = sound_output->ring_buffer.size - byte_to_lock;
        regions[1] = ring_buffer_memory;
        region_sizes[1] = bytes_to_write - region_sizes[0];
    }
    for (int32_t region_index = 0; region_index < max_regions; ++region_index)
    {
        // region must be a multiple of full sample 
        HANDMADE_ASSERT(0 == (region_sizes[region_index] %
                              sound_output->bytes_per_sample));
            
        std::memcpy(regions[region_index], samples, region_sizes[region_index]);
        samples += region_sizes[region_index];
        sound_output->running_sample_index += safe_truncate_uint64_uint32(
            region_sizes[region_index] / sound_output->bytes_per_sample);
        // int16_t *sample_out = static_cast<int16_t*>(regions[region_index]);
        // for (DWORD sample_index = 0,
        //              sample_end = region_sizes[region_index] /
        //                           sound_output->bytes_per_sample;
        //      sample_index < sample_end;
        //      ++sample_index)
        // {
        //     *sample_out++ = *samples++;
        //     *sample_out++ = *samples++;
        //     ++sound_output->running_sample_index;
        // }
    }
}

internal void sdl_audio_callback(void *userdata, uint8_t* stream, int32_t len)
{
    printf("sdl_audio_callback: len=%d, ring_buffer=%p\n", len, userdata);
    // std::memset(stream, 0, len);
    sdl_sound_ring_buffer *ring_buffer =
            static_cast<sdl_sound_ring_buffer*>(userdata);
    size_t len_in_size = static_cast<size_t>(len);

    // grab data from ring buffer to fill the sdl audio buffer
    size_t region_1_size = len_in_size;
    size_t region_2_size = 0;
    if ((ring_buffer->play_cursor + len_in_size) > ring_buffer->size)
    {
        region_1_size = ring_buffer->size - ring_buffer->play_cursor;
        region_2_size = static_cast<size_t>(len) - region_1_size;
    }
    std::memcpy(stream,
                static_cast<uint8_t*>(ring_buffer->memory) +
                ring_buffer->play_cursor,
                region_1_size);
    if (region_2_size > 0)
    {
        std::memcpy(stream + region_1_size,
                    ring_buffer->memory,
                    region_2_size);
    }
    ring_buffer->play_cursor = (ring_buffer->play_cursor + len_in_size)
            % ring_buffer->size;
}

internal std::pair<real32, real32> sdl_thumb_stick_resolve_deadzone_normalize(
    real32 x, real32 y, real32 deadzone)
{
    // normalize the input first (-1.0f to 1.0f)
    // max with -1 because abs(min val) is 1 great then max val
    x = std::max(-1.0f, x / kSdlControllerMaxStickVal);
    y = std::max(-1.0f, y / kSdlControllerMaxStickVal);

    // adjust for deadzone
    if (x >= 0.0f)
    {
        x = x < deadzone ? 0 : x - deadzone;
    }
    else
    {
        x = x > -deadzone ? 0 : x + deadzone;
    }
    if (y >= 0.0f)
    {
        y = y < deadzone ? 0 : y - deadzone;
    }
    else
    {
        y = y > -deadzone ? 0 : y + deadzone;
    }

    // scale the val for smooth transition outside deadzone
    // sdl flips y-axis compared to xinput
    x *= (1.0f / (1.0f - deadzone));
    y *= (-1.0f / (1.0f - deadzone));

    return std::make_pair(x, y);
}

internal void sdl_init_controllers(sdl_game_controllers *controllers,
                          const char* mapping_file)
{
    // add mapping first
    // TODO: FixThis!!! always fail
    int32_t num_mappings = SDL_GameControllerAddMappingsFromFile(mapping_file);
    if (num_mappings < 0)
    {
        printf("SDL_GameControllerAddMappingsFromFile error: %s\n",
               SDL_GetError());
    }
    else
    {
        printf("SDL_GameControllerAddMappingsFromFile added %d mappings\n",
               num_mappings);
    }
    // get the number of joysticks detected by SDL
    int32_t sdl_num_joysticks = SDL_NumJoysticks();
    if (sdl_num_joysticks >= 0)
    {
        printf("sdl num controllers=%d\n", sdl_num_joysticks);
        int32_t controller_index = 0;
        for (int32_t joystick_index = 0;
             joystick_index < sdl_num_joysticks;
             ++joystick_index)
        {
            // check if the joystick has controller support
            // (with button mapping)
            if (SDL_IsGameController(joystick_index))
            {
                printf("Found supported joystick: %d, controller %d\n",
                       joystick_index, controller_index);
                controllers->controllers[controller_index++] =
                        SDL_GameControllerOpen(joystick_index);
                if (controller_index == game_input::max_controller_count)
                {
                    break;
                }
            }
        }
        controllers->num_controllers_active = controller_index;
    }
    else
    {
        printf("SDL_NumJoysticks error: %s. No controller support.\n",
               SDL_GetError());
    }

    // init haptic
    for (int32_t controller_index = 0;
         controller_index < controllers->num_controllers_active;
         ++controller_index)
    {
        SDL_Joystick *joystick = SDL_GameControllerGetJoystick(
            controllers->controllers[controller_index]);
        HANDMADE_ASSERT(joystick);
        SDL_Haptic *haptic = SDL_HapticOpenFromJoystick(joystick);
        if (haptic)
        {
            printf("Haptic supported by controller %d\n", controller_index);
            if (SDL_HapticRumbleInit(haptic) == 0)
            {
                printf("Rumbling supported\n");
                controllers->haptics[controller_index] = haptic;
            }
            else
            {
                printf("Rumbling not supported\n");
                SDL_HapticClose(haptic);
            }
        }
        else
        {
            printf("Haptic not supported by controller %d\n", controller_index);
        }
    }
}

internal void sdl_process_controller_digital_button(
    game_button_state *new_state,
    const game_button_state *old_state,
    SDL_GameController *controller, SDL_GameControllerButton button)
{
    new_state->ended_down = SDL_GameControllerGetButton(controller, button);
    int32_t transition_amount =
            (old_state->ended_down != new_state->ended_down) ? 1 : 0;
    new_state->num_half_transition =
            old_state->num_half_transition + transition_amount;
}

internal void sdl_process_kbd_msg(game_button_state *new_state, bool32 is_down)
{
    new_state->ended_down = is_down;
    ++new_state->num_half_transition;
}

internal void sdl_process_event(game_controller_input *kbd_controller)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            {
                printf("SDL_QUIT\n");
                g_running = false;
            }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            {
                SDL_Keycode keycode = event.key.keysym.sym;
                bool32 is_down = false;
                bool32 was_down = false;
                if (event.key.state == SDL_RELEASED)
                {
                    was_down = true;
                }
                else
                {
                    is_down = true;
                     if (event.key.repeat != 0)
                     {
                         was_down = true;
                     }
                }
                bool32 alt_down = (event.key.keysym.mod & KMOD_ALT) ? true : false;
                if (is_down != was_down)
                {
                    switch (keycode)
                    {
                    case SDLK_w:
                        {
                            printf("W\n");
                        }
                        break;
                    case SDLK_a:
                        {
                            printf("A\n");
                        }
                        break;
                    case SDLK_s:
                        {
                            printf("S\n");
                        }
                        break;
                    case SDLK_d:
                        {
                            printf("D\n");
                        }
                        break;
                    case SDLK_UP:
                        {
                            sdl_process_kbd_msg(&kbd_controller->y,
                                                  is_down);
                        }
                        break;
                    case SDLK_DOWN:
                        {
                            printf("SDLK_DOWN: isdown=%d, wasdown=%d\n",
                                   is_down, was_down);
                            sdl_process_kbd_msg(&kbd_controller->a,
                                                  is_down);
                        }
                        break;
                    case SDLK_LEFT:
                        {
                            sdl_process_kbd_msg(&kbd_controller->x,
                                                  is_down);
                        }
                        break;
                    case SDLK_RIGHT:
                        {
                            sdl_process_kbd_msg(&kbd_controller->b,
                                                  is_down);
                        }
                        break;
                    case SDLK_ESCAPE:
                        {
                            g_running = false;
                        }
                        break;
                    case SDLK_SPACE:
                        {
                            printf("SPACE\n");
                        }
                        break;
                    case SDLK_RETURN:
                        {
                            printf("ENTER\n");
                        }
                        break;
                    }
                }
            }
            break;
            // case SDL_WINDOWEVENT:
            //     {
            //         switch (event.window.event)
            //         {
            //         case SDL_WINDOWEVENT_RESIZED:
            //             {
            //                 printf("SDL_WINDOWEVENT_RESIZED (%d, %d)\n",
            //                        event.window.data1, event.window.data2);
            //             }
            //             break;
            //             // TODO: just for now
            //         case SDL_WINDOWEVENT_EXPOSED:
            //             {
            //                 auto *window = SDL_GetWindowFromID(event.window.windowID);
            //                 auto *renderer = SDL_GetRenderer(window);
            //                 local_persist bool32 is_white = true;
            //                 is_white = !is_white;
            //                 if (is_white)
            //                 {
            //                     SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            //                 }
            //                 else
            //                 {
            //                     SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            //                 }
            //             }
            //             break;
            //         }
            //     }
            //     break;
        }
    }
}

// int if_rdtscp() {
//     unsigned int edx = 0;

//     asm volatile (
//      "CPUID\n\t"
//     :
//      "=d" (edx)
//     :
//     "a" (0x80000001)
//     :
//      "%rax", "%rcx", "%rdx");  

//     return (edx >> 27) & 0x1;
// }

int main(int, char **)
{
    int a[13];
    int b = array_length(a);
    printf("b is %d\n", b);
    // if (if_rdtscp())
    // {
    //     printf("rdtscp\n");
    // }
    // else
    //     printf("no rdtscp\n");
    // printf("page size=%d\n", sysconf(_SC_PAGESIZE));
    
    constexpr int32_t backbuffer_width = 1280;
    constexpr int32_t backbuffer_height = 720;
    
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER |
                 SDL_INIT_AUDIO | SDL_INIT_HAPTIC) != 0)
    {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("Handmade Hero",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              backbuffer_width, backbuffer_height,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    // | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window)
    {
        printf("SDL_CreateWindow error: %s\n", SDL_GetError());
        sdl_cleanup(window, renderer, g_backbuffer.texture, 0, nullptr);
        return 1;
    }

    // create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        printf("SDL_CreateRenderer error: %s\n", SDL_GetError());
        sdl_cleanup(window, renderer, g_backbuffer.texture, 0, nullptr);
        return 1;
    }

    // create back buffer
    if (!sdl_resize_backbuffer(&g_backbuffer, renderer,
                               backbuffer_width, backbuffer_height))
    {
        sdl_cleanup(window, renderer, g_backbuffer.texture, 0, nullptr);
        return 1;
    }

    // init audio
    sdl_sound_output sound_output {};
    sound_output.running_sample_index = 0;
    sound_output.num_sound_ch = 2;
    sound_output.samples_per_sec = 48000;
    sound_output.sec_to_buffer = 2;
    // signed 16 bit little endian order
    sound_output.sdl_audio_format = AUDIO_S16LSB;
    // TODO: may need to adjust this
    // must be a power of 2, 2048 samples seem to be a popular setting balancing
    // latency and skips (~23.5 fps, 42.67 ms between writes)
    sound_output.sdl_audio_buffer_size_in_samples = 2048;
    // aim for 1/15th sec latency
    sound_output.latency_sample_count = sound_output.samples_per_sec / 10;
    sound_output.bytes_per_sample = sizeof(int16_t) * sound_output.num_sound_ch;
    sound_output.ring_buffer.size = sound_output.samples_per_sec *
            sound_output.bytes_per_sample * sound_output.sec_to_buffer;

    SDL_AudioDeviceID audio_dev_id = sdl_init_sound(&sound_output);
    int16_t *samples = nullptr;
    bool32 sound_playing = false;
    if (audio_dev_id > 0)
    {
        // allocate sound buffer sample, this is free automatically when app
        // terminates make it as large as the total ring buffer size for safety
        samples = static_cast<int16_t*>(platform_alloc_zeroed(
            nullptr, sound_output.ring_buffer.size));
    }

    // input
    game_input input[2] = {};
    game_input *new_input = &input[0];
    game_input *old_input = &input[1];

    // init game controller
    sdl_game_controllers sdl_controllers {};
    sdl_init_controllers(&sdl_controllers, kSdlControllerMappingFile);
    // following un-normalized deadzone comes from xinput
    const real32 left_thumb_norm_deadzone =
            sdl_get_controller_stick_normalized_deadzone(7849.0f);
    const real32 right_thumb_norm_deadzone =
            sdl_get_controller_stick_normalized_deadzone(8689.0f);

    /*
      TODO: may be handle SDL_CONTROLLERDEVICE events for plugged in or out
      controllers.
    */

    // game memory
#if HANDMADE_INTERNAL_BUILD
    void *base_memory_ptr = reinterpret_cast<void*>(terabyte(2ULL));
#else
    void *base_memory_ptr = nullptr;
#endif
    game_memory memory {};
    memory.permanent_storage_size = megabyte(64ULL);
    memory.transient_storage_size = gigabyte(1ULL);
    uint64_t total_size = memory.permanent_storage_size +
            memory.transient_storage_size;
    // Guarantee to be allocation granularity (64KB) aligned
    // commited to page boundary (4KB), but the rest are wasted space
    // memory auto clears to 0
    // freed automatically when app terminates
    memory.permanent_storage = platform_alloc_zeroed(base_memory_ptr, total_size);
    memory.transient_storage = static_cast<int8_t*>(memory.permanent_storage) +
            memory.permanent_storage_size;
    if (g_backbuffer.memory && samples && memory.permanent_storage &&
        memory.transient_storage)
    {
        g_running = true;

        uint64_t last_cycle_count = __rdtsc();
        auto last_time_point = std::chrono::high_resolution_clock::now();
    
        while (g_running)
        {
            // We don't really need old input for keyboard as all keyboard
            // events are processed by wm msgs. So, just copy the old state.
            game_controller_input *kbd_controller = &new_input->kbd_controller;
            *kbd_controller = old_input->kbd_controller;
            
            sdl_process_event(kbd_controller);

            if (!g_running)
            {
                break;
            }
            // game frame

            // poll game controller input
            for (int32_t controller_index = 0;
                 controller_index < sdl_controllers.num_controllers_active;
                 ++controller_index)
            {
                // Controller is connected
                game_controller_input *new_controller
                        = &new_input->controllers[controller_index];
                const game_controller_input *old_controller
                        = &old_input->controllers[controller_index];
                SDL_GameController *sdl_controller =
                        sdl_controllers.controllers[controller_index];
                // these are what we care about
                bool32 up = SDL_GameControllerGetButton(
                    sdl_controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
                bool32 down = SDL_GameControllerGetButton(
                    sdl_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                bool32 left = SDL_GameControllerGetButton(
                    sdl_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                bool32 right = SDL_GameControllerGetButton(
                    sdl_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
                // bool32 start = SDL_GameControllerGetButton(
                // sdl_controller, SDL_CONTROLLER_BUTTON_START);
                // bool32 back = SDL_GameControllerGetButton(
                // sdl_controller, SDL_CONTROLLER_BUTTON_BACK);
                
                sdl_process_controller_digital_button(
                    &new_controller->a,
                    &old_controller->a,
                    sdl_controller,
                    SDL_CONTROLLER_BUTTON_A);
                sdl_process_controller_digital_button(
                    &new_controller->b,
                    &old_controller->b,
                    sdl_controller,
                    SDL_CONTROLLER_BUTTON_B);
                sdl_process_controller_digital_button(
                    &new_controller->x,
                    &old_controller->x,
                    sdl_controller,
                    SDL_CONTROLLER_BUTTON_X);
                sdl_process_controller_digital_button(
                    &new_controller->y,
                    &old_controller->a,
                    sdl_controller,
                    SDL_CONTROLLER_BUTTON_Y);
                sdl_process_controller_digital_button(
                    &new_controller->left_shoulder,
                    &old_controller->left_shoulder,
                    sdl_controller,
                    SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
                sdl_process_controller_digital_button(
                    &new_controller->right_shoulder,
                    &old_controller->right_shoulder,
                    sdl_controller,
                    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

                real32 left_stick_x = static_cast<real32>(SDL_GameControllerGetAxis(
                    sdl_controller, SDL_CONTROLLER_AXIS_LEFTX));
                real32 left_stick_y = static_cast<real32>(SDL_GameControllerGetAxis(
                    sdl_controller, SDL_CONTROLLER_AXIS_LEFTY));
                real32 right_stick_x = static_cast<real32>(SDL_GameControllerGetAxis(
                    sdl_controller, SDL_CONTROLLER_AXIS_RIGHTX));
                real32 right_stick_y = static_cast<real32>(SDL_GameControllerGetAxis(
                    sdl_controller, SDL_CONTROLLER_AXIS_RIGHTY));
                auto left_stick_xy = sdl_thumb_stick_resolve_deadzone_normalize(
                    left_stick_x, left_stick_y, left_thumb_norm_deadzone);
                left_stick_x = left_stick_xy.first;
                left_stick_y = left_stick_xy.second;
                auto right_stick_xy = sdl_thumb_stick_resolve_deadzone_normalize(
                    right_stick_x, right_stick_y, right_thumb_norm_deadzone);
                right_stick_x = right_stick_xy.first;
                right_stick_y = right_stick_xy.second;
                printf("left: x=%.2f, y=%.2f; right: x=%.2f, y=%.2f\n",
                       left_stick_x, left_stick_y, right_stick_x, right_stick_y);
                // std::stringstream ss;
                // ss << "stickx = " << left_stick_x << " sticky=" << left_stick_y << std::endl;
                // OutputDebugStringA(ss.str().c_str());
                new_controller->is_analog = true;

                new_controller->left_stick.start_x = old_controller->left_stick.end_x;
                new_controller->left_stick.end_x = left_stick_x;
                new_controller->left_stick.min_x = new_controller->left_stick.max_x
                        = new_controller->left_stick.end_x;
                new_controller->left_stick.start_y = old_controller->left_stick.end_y;
                new_controller->left_stick.end_y = left_stick_y;
                new_controller->left_stick.min_y = new_controller->left_stick.max_y
                        = new_controller->left_stick.end_y;

                new_controller->right_stick.start_x = old_controller->right_stick.end_x;
                new_controller->right_stick.end_x = right_stick_x;
                new_controller->right_stick.min_x = new_controller->right_stick.max_x
                        = new_controller->right_stick.end_x;
                new_controller->right_stick.start_y = old_controller->right_stick.end_y;
                new_controller->right_stick.end_y = right_stick_y;
                new_controller->right_stick.min_y = new_controller->right_stick.max_y
                        = new_controller->right_stick.end_y;
            }

            // const game_controller_input *controller0 = &new_input->controllers[0];
            // auto *haptic = sdl_controllers.haptics[0];
            // if (controller0->a.ended_down)
            // {
            //     // rumble for 2 sec with a strength of 30%
            //     SDL_HapticRumblePlay(haptic, 0.3f, 2000);
            //     // if we play for infinite, we can use SDL_HapticRumbleStop()
            // }

            size_t byte_to_lock = 0;
            size_t bytes_to_write = 0;
            if (audio_dev_id != 0)
            {
                // fill the buffer till the play cursor + latency,
                // starting from the last write position
                byte_to_lock = (sound_output.running_sample_index *
                                sound_output.bytes_per_sample)
                        % sound_output.ring_buffer.size;

                // avoid play cursor to change during this
                SDL_LockAudioDevice(audio_dev_id);
                size_t target_to_cursor = (sound_output.ring_buffer.play_cursor +
                                           sound_output.latency_sample_count *
                                           sound_output.bytes_per_sample) %
                        sound_output.ring_buffer.size;
                if (byte_to_lock > target_to_cursor)
                {
                    bytes_to_write = sound_output.ring_buffer.size - byte_to_lock
                            + target_to_cursor;
                }
                else
                {
                    bytes_to_write = target_to_cursor - byte_to_lock;
                }
                SDL_UnlockAudioDevice(audio_dev_id);
                printf("bytes_to_write=%" PRIuS "\n", bytes_to_write);
                if (!sound_playing)
                {
                    // start playing audio
                    SDL_PauseAudioDevice(audio_dev_id, 0);
                    sound_playing = true;
                }
            }
            
            game_sound_buffer game_sound_buffer {};
            if (bytes_to_write > 0)
            {
                game_sound_buffer.samples = samples; 
                game_sound_buffer.sample_count = safe_truncate_uint64_uint32(
                    bytes_to_write / sound_output.bytes_per_sample);
                game_sound_buffer.samples_per_sec = sound_output.samples_per_sec;
            }

            game_offscreen_buffer buffer {};
            buffer.width = g_backbuffer.width;
            buffer.height = g_backbuffer.height;
            buffer.pitch = g_backbuffer.pitch;
            buffer.memory = g_backbuffer.memory;

            game_update_and_render(&memory, &buffer, &game_sound_buffer,
                                   new_input);

            if (bytes_to_write > 0)
            {
                sdl_fill_sound_buffer(&sound_output, &game_sound_buffer,
                                      byte_to_lock, bytes_to_write);
                printf("bytes written=%" PRIuS "\n", bytes_to_write);
            }
        
            // uint8_t *row = (uint8_t*)g_backbuffer.memory;
            // for (int y = 0; y < g_backbuffer.height; ++y)
            // {
            //     uint32_t *pixel = (uint32_t*)row;
            //     for (int x = 0; x < g_backbuffer.width; ++x)
            //     {
            //         uint8_t red = 0;
            //         uint8_t green = uint8_t(y + green_offset);
            //         uint8_t blue = uint8_t(x + blue_offset);
            //         *pixel++ = (red << 16) | (green << 8) | blue;
            //     }
            //     row += g_backbuffer.pitch;
            // }
        
            // SDL_RenderClear(renderer);
            SDL_UpdateTexture(g_backbuffer.texture, nullptr,
                              g_backbuffer.memory,
                              static_cast<int32_t>(g_backbuffer.pitch));
            SDL_RenderCopy(renderer, g_backbuffer.texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);

            // swap game input
            game_input *tmp_input = new_input;
            new_input = old_input;
            old_input = tmp_input;

            // profiling
            uint64_t end_cycle_count = __rdtsc();
            auto end_time_point = std::chrono::high_resolution_clock::now();

            // use signed, as it may go backward
            int64_t cycles_elapsed =
                    static_cast<int64_t>(end_cycle_count - last_cycle_count);
            real32 mega_cycles_per_frame = static_cast<real32>(cycles_elapsed)
                    / 1000000.0f;

            real32 ms_per_frame = std::chrono::duration_cast<chrono_duration_ms>(
                end_time_point - last_time_point).count();
            real32 fps = 1000.0f / ms_per_frame;

            printf("%.2f Mc/f, %.2f ms/f, %.2f fps\n",
                   mega_cycles_per_frame, ms_per_frame, fps);

            last_cycle_count = end_cycle_count;
            last_time_point = end_time_point;
        }
    }
    else
    {
        // fail to allocate memory, no game.
        printf("Fail to alloc memory to backbuffer, sound buffer, or game memory.\n");
    }
    sdl_cleanup(window, renderer, g_backbuffer.texture, audio_dev_id,
                &sdl_controllers);
    return 0;
}
