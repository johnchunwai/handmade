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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <utility>
#include <algorithm>
//#include <sstream>
#include <chrono>
// #include <iostream>

#define local_persist static
#define global_variable static
#define internal static
#define class_scope static

typedef int32_t bool32;
typedef float real32;
typedef double real64;
typedef std::chrono::duration<real32, std::ratio<1, 1000>> chrono_duration_ms;

// constants
constexpr real32 kPiReal32 = 3.14159265359f;
constexpr real32 kEpsilonReal32 = 0.00001f;

#include "handmade.cpp"


/*
  Platform specific stuff below
*/
#include <SDL2/SDL.h>
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

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
// following un-normalized deadzone comes from xinput
global_variable const real32 kSdlLeftThumbNormalizedDeadzone =
        sdl_get_controller_stick_normalized_deadzone(7849.0f);
global_variable const real32 kSdlRightThumbNormalizedDeadzone =
        sdl_get_controller_stick_normalized_deadzone(8689.0f);

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
    volatile ptrdiff_t play_cursor;
    void *memory;
};

struct sdl_sound_output
{
    sdl_sound_ring_buffer ring_buffer;
    int32_t running_sample_index;
    int32_t num_sound_ch;
    int32_t samples_per_sec;
    int32_t sec_to_buffer;
    SDL_AudioFormat sdl_audio_format;
    int32_t sdl_audio_buffer_size_in_samples;

    // calculated values
    int32_t latency_sample_count;  // to minimize delay when we want to change the sound
    int32_t bytes_per_sample;
    int32_t sdl_audio_buffer_size_in_bytes;
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

internal bool32 sdl_handle_event(SDL_Event *event)
{
    bool32 running = true;
    switch (event->type)
    {
    case SDL_QUIT:
        {
            printf("SDL_QUIT\n");
            running = false;
        }
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        {
            SDL_Keycode keycode = event->key.keysym.sym;
            bool32 is_down = false;
            bool32 was_down = false;
            if (event->key.state == SDL_RELEASED)
            {
                was_down = true;
            }
            else if (event->key.repeat != 0)
            {
                is_down = true;
                was_down = true;
            }
            bool32 alt_down = (event->key.keysym.mod & KMOD_ALT);
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
                    printf("UP\n");
                }
                break;
            case SDLK_DOWN:
                {
                    printf("DOWN\n");
                }
                break;
            case SDLK_LEFT:
                {
                    printf("LEFT\n");
                }
                break;
            case SDLK_RIGHT:
                {
                    printf("RIGHT\n");
                }
                break;
            }
        }
        break;
    case SDL_WINDOWEVENT:
        {
            switch (event->window.event)
            {
            case SDL_WINDOWEVENT_RESIZED:
                {
                    printf("SDL_WINDOWEVENT_RESIZED (%d, %d)\n",
                           event->window.data1, event->window.data2);
                }
                break;
                // TODO: just for now
            case SDL_WINDOWEVENT_EXPOSED:
                {
                    auto *window = SDL_GetWindowFromID(event->window.windowID);
                    auto *renderer = SDL_GetRenderer(window);
                    local_persist bool32 is_white = true;
                    is_white = !is_white;
                    if (is_white)
                    {
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                    }
                    else
                    {
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    }
                }
                break;
            }
        }
        break;
    }
    return running;
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
        // free(buffer->memory);
        munmap(buffer->memory,
               buffer->width * buffer->height * bytes_per_pixel);
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

    int32_t bitmap_mem_size = buffer->width * buffer->height * bytes_per_pixel;
    // buffer->memory = malloc(bitmap_mem_size);
    buffer->memory = mmap(nullptr,
                          bitmap_mem_size,
                          PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_PRIVATE,
                          -1, 0);
    assert(buffer->memory && buffer->memory != MAP_FAILED);

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
    desired.freq = sound_output->samples_per_sec;
    desired.format = sound_output->sdl_audio_format;
    desired.channels = sound_output->num_sound_ch;
    desired.samples = sound_output->sdl_audio_buffer_size_in_samples;
    desired.callback = sdl_audio_callback;
    desired.userdata = reinterpret_cast<void*>(&sound_output->ring_buffer);
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
        printf("Init audio: samples=%d, buffer size=%d, silience=%d\n",
               obtained.samples, obtained.size, obtained.silence);
        sound_output->sdl_audio_buffer_size_in_samples = obtained.samples;
        sound_output->sdl_audio_buffer_size_in_bytes = obtained.size;
        assert(obtained.silence == 0);
        // init ring buffer
        sound_output->ring_buffer.memory = mmap(nullptr,
                                   sound_output->ring_buffer.size,
                                   PROT_READ | PROT_WRITE,
                                   MAP_ANONYMOUS | MAP_PRIVATE,
                                   -1, 0);
        assert(sound_output->ring_buffer.memory &&
               sound_output->ring_buffer.memory != MAP_FAILED);
        std::memset(sound_output->ring_buffer.memory, 0,
                    sound_output->ring_buffer.size);
    }

    return audio_dev_id;
}

internal void sdl_fill_sound_buffer(sdl_sound_output *sound_output,
                                    const game_sound_buffer *source_buffer,
                                    ptrdiff_t byte_to_lock,
                                    ptrdiff_t bytes_to_write)
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
    uint8_t *ring_buffer_memory = reinterpret_cast<uint8_t*>(
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
        assert(0 == (region_sizes[region_index] % sound_output->bytes_per_sample));
            
        std::memcpy(regions[region_index], samples, region_sizes[region_index]);
        samples += region_sizes[region_index];
        sound_output->running_sample_index += region_sizes[region_index]
                / sound_output->bytes_per_sample;
        // int16_t *sample_out = static_cast<int16_t*>(regions[region_index]);
        // for (DWORD sample_index = 0,
        //              sample_end = region_sizes[region_index] / sound_output->bytes_per_sample;
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
            reinterpret_cast<sdl_sound_ring_buffer*>(userdata);

    // grab data from ring buffer to fill the sdl audio buffer
    size_t region_1_size = len;
    size_t region_2_size = 0;
    if (ring_buffer->play_cursor + len > ring_buffer->size)
    {
        region_1_size = ring_buffer->size - ring_buffer->play_cursor;
        region_2_size = static_cast<size_t>(len) - region_1_size;
    }
    std::memcpy(stream,
                reinterpret_cast<uint8_t*>(ring_buffer->memory) +
                ring_buffer->play_cursor,
                region_1_size);
    if (region_2_size > 0)
    {
        std::memcpy(stream + region_1_size,
                    ring_buffer->memory,
                    region_2_size);
    }
    ring_buffer->play_cursor = (ring_buffer->play_cursor + len)
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
    x *= (1 / (1 - deadzone));
    y *= (1 / (1 - deadzone));

    return std::make_pair(x, y);
}

void sdl_init_controllers(sdl_game_controllers *controllers,
                          const char* mapping_file)
{
    // add mapping first
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
        assert(joystick || !"failed to get joystick from controller");
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

#if 0
internal __inline__ volatile uint64_t __rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (uint64_t)lo)|( ((uint64_t)hi)<<32 );
}
#endif

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

int main(int argc, char **argv)
{
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
    sound_output.sdl_audio_buffer_size_in_samples = 4096;
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
        samples = static_cast<int16_t*>(mmap(nullptr,
                                             sound_output.ring_buffer.size,
                                             PROT_READ | PROT_WRITE,
                                             MAP_ANONYMOUS | MAP_PRIVATE,
                                             -1, 0));
        assert(samples && samples != MAP_FAILED);
        std::memset(samples, 0, sound_output.ring_buffer.size);
    }

    // input
    game_input input[2] = {};
    game_input *new_input = &input[0];
    game_input *old_input = &input[1];

    // init game controller
    sdl_game_controllers sdl_controllers {};
    sdl_init_controllers(&sdl_controllers, kSdlControllerMappingFile);

    /*
      TODO: may be handle SDL_CONTROLLERDEVICE events for plugged in or out
      controllers.
    */

    // game memory
    game_memory memory {};
    memory.permanent_storage_size = megabyte(64ULL);
    // allocate game memory, this is free automatically when app
    // terminates
    memory.permanent_storage = mmap(nullptr,
                                    memory.permanent_storage_size,
                                    PROT_READ | PROT_WRITE,
                                    MAP_ANONYMOUS | MAP_PRIVATE,
                                    -1, 0);
    memory.transient_storage_size = gigabyte(1ULL);
    // allocate game memory, this is free automatically when app
    // terminates
    memory.transient_storage = mmap(nullptr,
                                    memory.transient_storage_size,
                                    PROT_READ | PROT_WRITE,
                                    MAP_ANONYMOUS | MAP_PRIVATE,
                                    -1, 0);

    if (g_backbuffer.memory && samples && memory.permanent_storage &&
        memory.transient_storage)
    {
        g_running = true;

        uint64_t last_cycle_count = __rdtsc();
        auto last_time_point = std::chrono::high_resolution_clock::now();
    
        while (g_running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (!sdl_handle_event(&event))
                {
                    g_running = false;
                    break;
                }
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
                    left_stick_x, left_stick_y, kSdlLeftThumbNormalizedDeadzone);
                left_stick_x = left_stick_xy.first;
                left_stick_y = left_stick_xy.second;
                auto right_stick_xy = sdl_thumb_stick_resolve_deadzone_normalize(
                    right_stick_x, right_stick_y, kSdlRightThumbNormalizedDeadzone);
                right_stick_x = right_stick_xy.first;
                right_stick_y = right_stick_xy.second;
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

            ptrdiff_t byte_to_lock = 0;
            ptrdiff_t bytes_to_write = 0;
            if (audio_dev_id != 0)
            {
                // fill the buffer till the play cursor + latency,
                // starting from the last write position
                byte_to_lock = (sound_output.running_sample_index *
                                sound_output.bytes_per_sample)
                        % sound_output.ring_buffer.size;

                // avoid play cursor to change during this
                SDL_LockAudioDevice(audio_dev_id);
                ptrdiff_t target_to_cursor = (sound_output.ring_buffer.play_cursor +
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
                printf("bytes_to_write=%" PRIu64"\n", bytes_to_write);
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
                game_sound_buffer.sample_count = bytes_to_write
                        / sound_output.bytes_per_sample;
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
                printf("bytes written=%" PRIu64"\n", bytes_to_write);
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
                              g_backbuffer.memory, g_backbuffer.pitch);
            SDL_RenderCopy(renderer, g_backbuffer.texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);

            // profiling
            uint64_t end_cycle_count = __rdtsc();
            auto end_time_point = std::chrono::high_resolution_clock::now();

            // use signed, as it may go backward
            int64_t cycles_elapsed = end_cycle_count - last_cycle_count;
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
