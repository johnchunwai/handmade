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

// printf macros that are not yet defined on MSVC
#if _WIN64
#define __PRIS_PREFIX "z"
#else  // _WIN32
#define __PRIS_PREFIX
#endif // _WIN64

#define PRIdS __PRIS_PREFIX "d"
#define PRIxS __PRIS_PREFIX "x"
#define PRIuS __PRIS_PREFIX "u"
#define PRIXS __PRIS_PREFIX "X"
#define PRIoS __PRIS_PREFIX "o"



#include "handmade.h"
#include "handmade.cpp"


/*
  Platform specific stuff below
*/
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <xinput.h>
#include <mmsystem.h>
#include <dsound.h>
#include <intrin.h>
#include "handmade.h"

struct win32_offscreen_buffer
{
    BITMAPINFO info;
    int32_t width;
    int32_t height;
    ptrdiff_t pitch;
    void *memory;
};

struct win32_window_dimension
{
    int32_t width;
    int32_t height;
};

// constants
constexpr real32 kXInputMaxStickVal = 32767.0f;
// constexpr real32 kXInputMinStickVal = -32768;

// TODO: global for now
global_variable bool32 g_running = false;
global_variable win32_offscreen_buffer g_backbuffer {};
global_variable IDirectSoundBuffer *g_sound_buffer = nullptr;

internal real32 win32_get_xinput_stick_normalized_deadzone(
    real32 unnormalized_deadzone)
{
    return unnormalized_deadzone /
            std::sqrt(kXInputMaxStickVal * kXInputMaxStickVal * 2.0f);
}

// XInputGetState
#define XINPUT_GET_STATE(name) DWORD WINAPI name(DWORD, XINPUT_STATE*)
typedef XINPUT_GET_STATE(XInputGetStateType);
XINPUT_GET_STATE(xinput_get_state_stub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable XInputGetStateType *xinput_get_state_ = xinput_get_state_stub;
#define XInputGetState xinput_get_state_

// XInputSetState
#define XINPUT_SET_STATE(name) DWORD WINAPI name(DWORD, XINPUT_VIBRATION*)
typedef XINPUT_SET_STATE(XInputSetStateType);
XINPUT_SET_STATE(xinput_set_state_stub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable XInputSetStateType *xinput_set_state_ = xinput_set_state_stub;
#define XInputSetState xinput_set_state_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID, \
                                                      LPDIRECTSOUND *, \
                                                      LPUNKNOWN)
typedef DIRECT_SOUND_CREATE(DirectSoundCreateType);

internal void* win32_alloc_zeroed(void *base_addr, size_t length)
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

internal void win32_free(void *memory)
{
    VirtualFree(memory, 0, MEM_RELEASE);
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
    HANDLE file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, 0, nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER file_size {};
        if (GetFileSizeEx(file, &file_size))
        {
            result.size = safe_truncate_int64_uint32(file_size.QuadPart);
            // Guarantee to be allocation granularity (64KB) aligned
            // commited to page boundary (4KB), but the rest are wasted space
            // memory auto clears to 0
            // freed automatically when app terminates
            result.content = win32_alloc_zeroed(nullptr, result.size);
            if (result.content)
            {
                DWORD bytes_read;
                if (ReadFile(file, result.content, result.size, &bytes_read, 0)
                    && (result.size == bytes_read))
                {
                    // NOTE: file read successfully
                }
                else
                {
                    debug_platform_free_file_memory(&result);
                    result.content = nullptr;
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
        }
        CloseHandle(file);
    }
    else
    {
        // TODO: logging
    }
    return result;
}

internal void debug_platform_free_file_memory(debug_read_file_result *file_mem)
{
    win32_free(file_mem->content);
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
    HANDLE file = CreateFileA(filename, GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, 0, nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        DWORD bytes_written;
        if (WriteFile(file, memory, mem_size, &bytes_written, nullptr) )
        {
            result = mem_size == bytes_written;
        }
        else
        {
            // TODO: logging
        }
        CloseHandle(file);
    }
    else
    {
        // TODO: logging
    }

    return result;
}

#endif // HANDMADE_INTERNAL_BUILD


internal void win32_load_xinput()
{
    constexpr char *xinput_dlls[] = {
        "xinput1_4.dll",
        "xinput1_3.dll",
        "xinput9_1_0.dll"
    };
    for (auto xinput_dll : xinput_dlls)
    {
        HMODULE xinput_lib = LoadLibraryA(xinput_dll);
        if (xinput_lib)
        {
            XInputGetState = reinterpret_cast<XInputGetStateType*>(
                GetProcAddress(xinput_lib, "XInputGetState"));
            XInputSetState = reinterpret_cast<XInputSetStateType*>(
                GetProcAddress(xinput_lib, "XInputSetState"));
            HANDMADE_ASSERT(XInputGetState && XInputSetState);
            break;
        }
    }
}

internal std::pair<real32, real32> win32_xinput_thumb_resolve_deadzone_normalize(
    real32 x, real32 y, real32 deadzone)
{
    // normalize the input first (-1.0f to 1.0f)
    // max with -1 because abs(min val) is 1 great then max val
    x = std::max(-1.0f, x / kXInputMaxStickVal);
    y = std::max(-1.0f, y / kXInputMaxStickVal);

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

internal win32_window_dimension win32_get_window_dimension(HWND hwnd)
{
    RECT window_rect;
    // rc.top and rc.left are always 0
    GetClientRect(hwnd, &window_rect);
    win32_window_dimension result {window_rect.right - window_rect.left,
                window_rect.bottom - window_rect.top};
    return result;
}

internal void win32_init_direct_sound(HWND hwnd,
                                      uint32_t num_sound_ch,
                                      uint32_t samples_per_sec,
                                      uint32_t buffer_size)
{
    // load library
    HMODULE direct_sound_lib = LoadLibraryA("dsound.dll");
    if (direct_sound_lib)
    {
        // get DirectSound object - cooperative mode
        auto *DirectSoundCreate = reinterpret_cast<DirectSoundCreateType*>(
            GetProcAddress(direct_sound_lib, "DirectSoundCreate"));
        IDirectSound *direct_sound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &direct_sound, 0)))
        {
            // just simple 2 ch stereo sound
            WAVEFORMATEX wave_fmt {};
            wave_fmt.wFormatTag = WAVE_FORMAT_PCM;
            wave_fmt.nChannels = static_cast<WORD>(num_sound_ch);
            wave_fmt.nSamplesPerSec = samples_per_sec;
            wave_fmt.wBitsPerSample = 16;
            wave_fmt.nBlockAlign = (wave_fmt.nChannels * wave_fmt.wBitsPerSample) / 8;
            wave_fmt.nAvgBytesPerSec = wave_fmt.nSamplesPerSec * wave_fmt.nBlockAlign;
            // wave_fmt.cbSize = 0;
            
            if (SUCCEEDED(direct_sound->SetCooperativeLevel(hwnd, DSSCL_PRIORITY)))
            {
                // create a primary buffer (object that mixes all sounds)
                DSBUFFERDESC buffer_desc {};
                buffer_desc.dwSize = sizeof(buffer_desc);
                buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;  // DSBCAPS_GLOBALFOCUS?
                // buffer_desc.dwBufferBytes = 0;
                // buffer_desc.lpwfxFormat = nullptr;
                // buffer_desc.guid3DAlgorithm = DS3DALG_DEFAULT;  // same as 0
                
                IDirectSoundBuffer *primary_buffer;
                if (SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_desc,
                                                              &primary_buffer,
                                                              nullptr)))
                {
                    if (SUCCEEDED(primary_buffer->SetFormat(&wave_fmt)))
                    {
                        // We have finally set the format.
                        OutputDebugStringA("Primary buffer format was set.\n");
                    }
                    else
                    {
                        // TODO: diagnostic
                    }
                }
                else
                {
                    // TODO: diagnostic
                }
            }
            else
            {
                // TODO: diagnostic
            }

            // create a 2ndary buffer (that holds the sound data)
            // this can be done independent of the above failures
            DSBUFFERDESC buffer_desc {};
            buffer_desc.dwSize = sizeof(buffer_desc);
            buffer_desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
            buffer_desc.dwBufferBytes = buffer_size;
            buffer_desc.lpwfxFormat = &wave_fmt;
            // buffer_desc.guid3DAlgorithm = DS3DALG_DEFAULT;  // same as 0
            if (SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_desc,
                                                          &g_sound_buffer,
                                                          nullptr)))
            {
                OutputDebugStringA("Secondary buffer created successfully\n");
            }
            else
            {
                // TODO: diagnostics
            }
        }
        else
        {
            // TODO: diagnostic
        }
    }
    else
    {
        // TODO: diagnostic
    }
}

internal void win32_debug_print_last_error()
{
    wchar_t *msg = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                   FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr,
                   GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&msg,
                   0,
                   nullptr);
    OutputDebugStringW(msg);
    LocalFree(msg);
}

internal void win32_resize_backbuffer(win32_offscreen_buffer *buffer,
                                      int32_t width, int32_t height)
{
    // TODO: bulletproof this.
    // maybe don't free first, free after.
    if (buffer->memory)
    {
        win32_free(buffer->memory);
        buffer->memory = nullptr;
    }

    int32_t bytes_per_pixel = 4;
    buffer->width = width;
    buffer->height = height;

    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;  // no alpha, 8-bit padding for align
    buffer->info.bmiHeader.biCompression = BI_RGB;
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = -buffer->height;  // -ve means top down
    
    buffer->pitch = buffer->width * bytes_per_pixel;

    // StretchDIBits doesn't need dvcCtx or DibSection (compared to BitBlt)
    // just need the memory to be aligned on DWORD boundary
    int32_t bitmap_mem_size = buffer->width * buffer->height * bytes_per_pixel;
    // Guarantee to be allocation granularity (64KB) aligned
    // commited to page boundary (4KB), but the rest are wasted space
    // memory auto clears to 0
    // freed automatically when app terminates
    buffer->memory = win32_alloc_zeroed(nullptr, bitmap_mem_size);
}

internal void win32_display_offscreen_buffer(const win32_offscreen_buffer *buffer,
                                             HDC device_context,
                                             int32_t width, int32_t height)
{
    // Setting to stretch blt mode to prevent artifacts when shrink blt.
    SetStretchBltMode(device_context, STRETCH_DELETESCANS);
    int32_t blt_result = StretchDIBits(device_context,
                                       0, 0, width, height,
                                       0, 0, buffer->width, buffer->height,
                                       buffer->memory, &buffer->info,
                                       DIB_RGB_COLORS, SRCCOPY);
    HANDMADE_ASSERT(blt_result);
}

internal LRESULT CALLBACK win32_wnd_proc(
    _In_ HWND   hwnd,
    _In_ UINT   msg,
    _In_ WPARAM wparam,
    _In_ LPARAM lparam)
{
    LRESULT lresult = 0;
            
    switch (msg)
    {
    case WM_SIZE:
        {
            // int width = LOWORD(lparam);
            // int height = HIWORD(lparam);
        }
        break;

    case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        }
        break;

    case WM_CLOSE:
        {
            // TODO: handle this with message for user to confirm quitting?
            g_running = false;
        }
        break;

    case WM_DESTROY:
        {
            // TODO: handle this as error - recreate window?
            g_running = false;
        }
        break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
        {
            HANDMADE_ASSERT(false);
            WPARAM vk_code = wparam;
            bool32 was_down = (lparam & (1 << 30));
            bool32 is_down = (lparam & (1 << 31));
            if (is_down != was_down)
            {
                switch (vk_code)
                {
                case 'W':
                    {
                        OutputDebugStringA("W\n");
                    }
                    break;
                case 'A':
                    {
                        OutputDebugStringA("A\n");
                    }
                    break;
                case 'S':
                    {
                        OutputDebugStringA("S\n");
                    }
                    break;
                case 'D':
                    {
                        OutputDebugStringA("D\n");
                    }
                    break;
                case 'Q':
                    {
                        OutputDebugStringA("Q\n");
                    }
                    break;
                case 'E':
                    {
                        OutputDebugStringA("E\n");
                    }
                    break;
                case VK_UP:
                    {
                        OutputDebugStringA("UP\n");
                    }
                    break;
                case VK_DOWN:
                    {
                        OutputDebugStringA("DOWN\n");
                    }
                    break;
                case VK_LEFT:
                    {
                        OutputDebugStringA("LEFT\n");
                    }
                    break;
                case VK_RIGHT:
                    {
                        OutputDebugStringA("RIGHT\n");
                    }
                    break;
                case VK_ESCAPE:
                    {
                        OutputDebugStringA("ESCAPE: ");
                        if (is_down)
                        {
                            OutputDebugStringA("is_down ");
                        }
                        if (was_down)
                        {
                            OutputDebugStringA("was_down ");
                        }
                        OutputDebugStringA("\n");
                    }
                    break;
                case VK_SPACE:
                    {
                        OutputDebugStringA("SPACE\n");
                    }
                    break;
                case VK_RETURN:
                    {
                        OutputDebugStringA("ENTER\n");
                    }
                    break;
                case VK_F4:
                    {
                        bool32 alt_down = (lparam & (1 << 29));
                        if (alt_down)
                        {
                            g_running = false;
                        }
                    }
                    break;
                }
            }
        }
        break;

    case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(hwnd, &paint);
            win32_window_dimension dimension = win32_get_window_dimension(hwnd);
            win32_display_offscreen_buffer(&g_backbuffer, device_context,
                                           dimension.width, dimension.height);
            EndPaint(hwnd, &paint);
        }
        break;

    default:
        {
            lresult = DefWindowProcW(hwnd, msg, wparam, lparam);
        }
        break;
    }
    return lresult;
}

internal void win32_clear_sound_buffer(IDirectSoundBuffer *sound_buffer)
{
    void *region = nullptr;
    DWORD region_size = 0;
    if (SUCCEEDED(sound_buffer->Lock(0, 0, &region, &region_size, nullptr,
                                     nullptr, DSBLOCK_ENTIREBUFFER)))
    {
        memset(region, 0, region_size);
        sound_buffer->Unlock(region, region_size, nullptr, 0);
    }
}

internal void win32_fill_sound_buffer(win32_sound_output *sound_output,
                                      const game_sound_buffer *source_buffer,
                                      DWORD byte_to_lock, DWORD bytes_to_write)
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
    void *regions[max_regions] = {};
    DWORD region_sizes[max_regions] = {};
    // int16_t* samples = source_buffer->samples;
    uint8_t *samples = reinterpret_cast<uint8_t*>(source_buffer->samples);
    
    if (SUCCEEDED(g_sound_buffer->Lock(
            byte_to_lock,
            bytes_to_write,
            &regions[0], &region_sizes[0],
            &regions[1], &region_sizes[1],
            0)))
    {
        for (int32_t region_index = 0; region_index < max_regions; ++region_index)
        {
            // region must be a multiple of full sample 
            HANDMADE_ASSERT(0 == (region_sizes[region_index] % sound_output->bytes_per_sample));
            
            std::memcpy(regions[region_index], samples, region_sizes[region_index]);
            samples += region_sizes[region_index];
            sound_output->running_sample_index += (region_sizes[region_index] /
                                                   sound_output->bytes_per_sample);
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
        g_sound_buffer->Unlock(regions[0], region_sizes[0],
                               regions[1], region_sizes[1]);
    }
}

internal void win32_process_xinput_digital_button(game_button_state *new_state,
                                                  const game_button_state *old_state,
                                                  const XINPUT_GAMEPAD *pad,
                                                  uint32_t xinput_button_bit)
{
    new_state->ended_down = pad->wButtons & xinput_button_bit;
    int32_t transition_amount =
            (old_state->ended_down != new_state->ended_down) ? 1 : 0;
    new_state->num_half_transition = old_state->num_half_transition +
            transition_amount;
}

internal void win32_process_kbd_msg(game_button_state *new_state, bool32 is_down)
{
    new_state->ended_down = is_down;
    ++new_state->num_half_transition;
}

internal void win32_process_wm_msg_synchonously(
    game_controller_input *kbd_controller)
{
    MSG msg;
    // BOOL msgResult = GetMessageW(&msg, nullptr, 0, 0);
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        // if (msgResult > 0)
    {
        switch (msg.message)
        {
        case WM_QUIT:
            {
                g_running = false;
            }
            break;
                    
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
            {
                WPARAM vk_code = msg.wParam;
                bool32 was_down = (msg.lParam & (1 << 30)) ? true : false;
                bool32 is_down = !(msg.lParam & (1 << 31));
                if (is_down != was_down)
                {
                    switch (vk_code)
                    {
                    case 'W':
                        {
                            OutputDebugStringA("W\n");
                        }
                        break;
                    case 'A':
                        {
                            OutputDebugStringA("A\n");
                        }
                        break;
                    case 'S':
                        {
                            OutputDebugStringA("S\n");
                        }
                        break;
                    case 'D':
                        {
                            OutputDebugStringA("D\n");
                        }
                        break;
                    case 'Q':
                        {
                            win32_process_kbd_msg(
                                &kbd_controller->left_shoulder, is_down);
                        }
                        break;
                    case 'E':
                        {
                            win32_process_kbd_msg(
                                &kbd_controller->right_shoulder, is_down);
                        }
                        break;
                    case VK_UP:
                        {
                            win32_process_kbd_msg(&kbd_controller->y,
                                                  is_down);
                        }
                        break;
                    case VK_DOWN:
                        {
                            char buf[256];
                            sprintf(buf, "VK_DOWN: isdown=%d, wasdown=%d\n",
                                    is_down, was_down);
                            OutputDebugStringA(buf);
                            win32_process_kbd_msg(&kbd_controller->a,
                                                  is_down);
                        }
                        break;
                    case VK_LEFT:
                        {
                            win32_process_kbd_msg(&kbd_controller->x,
                                                  is_down);
                        }
                        break;
                    case VK_RIGHT:
                        {
                            win32_process_kbd_msg(&kbd_controller->b,
                                                  is_down);
                        }
                        break;
                    case VK_ESCAPE:
                        {
                            g_running = false;
                        }
                        break;
                    case VK_SPACE:
                        {
                            OutputDebugStringA("SPACE\n");
                        }
                        break;
                    case VK_RETURN:
                        {
                            OutputDebugStringA("ENTER\n");
                        }
                        break;
                    case VK_F4:
                        {
                            bool32 alt_down = (msg.lParam & (1 << 29));
                            if (alt_down)
                            {
                                g_running = false;
                            }
                        }
                        break;
                    }
                }
            }
            break;
        default:
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            break;
        }
    }
}


#if HANDMADE_DIAGNOSTIC

int32_t main(int, const char**)
{
    return wWinMain(0, 0, 0, 0);
}

#endif  // HANDMADE_DIAGNOSTIC

int32_t CALLBACK wWinMain(
    HINSTANCE window_instance,
    HINSTANCE,  // hPrevInst is useless
    LPWSTR,     // not using lpCmdLine
    int)        // not using nCmdShow
{
    LARGE_INTEGER perf_count_freq_result;
    QueryPerformanceFrequency(&perf_count_freq_result);
    int64_t perf_count_freq = perf_count_freq_result.QuadPart;
    
    win32_load_xinput();
    
    wchar_t *wnd_class_name = L"Handmade Hero Window Class";
    WNDCLASSEXW wnd_class = {};  // c++11 aggregate init to zero the struct
    wnd_class.cbSize = sizeof(wnd_class);
    wnd_class.style = CS_HREDRAW | CS_VREDRAW;
    wnd_class.lpfnWndProc = win32_wnd_proc;
    wnd_class.hInstance = window_instance;
    wnd_class.lpszClassName = wnd_class_name;

    ATOM wnd_class_atom = RegisterClassExW(&wnd_class);
    if (0 == wnd_class_atom)
    {
        win32_debug_print_last_error();
        HANDMADE_ASSERT(0 != wnd_class_atom);
        return 0;
    }

    HWND hwnd = CreateWindowExW(0, wnd_class_name, L"Handmade Hero",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                nullptr, nullptr, window_instance, nullptr);
    if (hwnd == nullptr)
    {
        win32_debug_print_last_error();
        HANDMADE_ASSERT(nullptr != hwnd);
        return 0;
    }

    win32_resize_backbuffer(&g_backbuffer, 1280, 720);
    
    // test sound
    win32_sound_output sound_output {};
    sound_output.running_sample_index = 0;
    sound_output.num_sound_ch = 2;
    sound_output.samples_per_sec = 48000;
    sound_output.sec_to_buffer = 2;
    sound_output.latency_sample_count = sound_output.samples_per_sec / 15;
    sound_output.bytes_per_sample = sizeof(int16_t) * sound_output.num_sound_ch;
    sound_output.sound_buffer_size = sound_output.samples_per_sec *
            sound_output.bytes_per_sample * sound_output.sec_to_buffer;

    // create buffer for 2 sec
    win32_init_direct_sound(hwnd, sound_output.num_sound_ch,
                            sound_output.samples_per_sec,
                            sound_output.sound_buffer_size);

    int16_t *samples = nullptr;
    if (g_sound_buffer)
    {
        // fill the whole buffer and started playing sound.
        win32_clear_sound_buffer(g_sound_buffer);
        g_sound_buffer->Play(0, 0, DSBPLAY_LOOPING);

        // allocate sound buffer sample
        // Guarantee to be allocation granularity (64KB) aligned
        // commited to page boundary (4KB), but the rest are wasted space
        // memory auto clears to 0
        // freed automatically when app terminates
        samples = static_cast<int16_t*>(win32_alloc_zeroed(
            nullptr, sound_output.sound_buffer_size));
    }

    // input
    const real32 left_thumb_norm_deadzone =
            win32_get_xinput_stick_normalized_deadzone(
                XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    const real32 right_thumb_norm_deadzone =
            win32_get_xinput_stick_normalized_deadzone(
                XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
    game_input input[2] = {};
    game_input *new_input = &input[0];
    game_input *old_input = &input[1];

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
    memory.permanent_storage = win32_alloc_zeroed(base_memory_ptr, total_size);
    memory.transient_storage = static_cast<int8_t*>(memory.permanent_storage) +
            memory.permanent_storage_size;
    if (g_backbuffer.memory && samples && memory.permanent_storage &&
        memory.transient_storage)
    {
        g_running = true;
    
        uint64_t last_cycle_count = __rdtsc();
        LARGE_INTEGER last_perf_counter;
        QueryPerformanceCounter(&last_perf_counter);
    
        while (g_running)
        {
            // We don't really need old input for keyboard as all keyboard
            // events are processed by wm msgs. So, just copy the old state.
            game_controller_input *kbd_controller = &new_input->kbd_controller;
            *kbd_controller = old_input->kbd_controller;

            win32_process_wm_msg_synchonously(kbd_controller);

            if (!g_running)
            {
                break;
            }

            constexpr uint32_t max_controller_count = std::min(
                XUSER_MAX_COUNT, game_input::max_controller_count);
            for (DWORD controller_index = 0;
                 controller_index < max_controller_count;
                 ++controller_index)
            {
                XINPUT_STATE controller_state {};
                // Simply get the state of the controller from XInput.
                if (ERROR_SUCCESS == XInputGetState(controller_index,
                                                    &controller_state))
                {
                    // Controller is connected
                    game_controller_input *new_controller =
                            &new_input->controllers[controller_index];
                    const game_controller_input *old_controller =
                            &old_input->controllers[controller_index];
                    // dwPacketNumber indicates if there have been state changes
                    controller_state.dwPacketNumber;
                    const XINPUT_GAMEPAD *pad = &controller_state.Gamepad;
                    // these are what we care about
                    bool32 up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                    bool32 down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                    bool32 left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                    bool32 right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                    // bool32 start = pad->wButtons & XINPUT_GAMEPAD_START;
                    // bool32 back = pad->wButtons & XINPUT_GAMEPAD_BACK;
                
                    win32_process_xinput_digital_button(
                        &new_controller->a,
                        &old_controller->a,
                        pad, XINPUT_GAMEPAD_A);
                    win32_process_xinput_digital_button(
                        &new_controller->b,
                        &old_controller->b,
                        pad, XINPUT_GAMEPAD_B);
                    win32_process_xinput_digital_button(
                        &new_controller->x,
                        &old_controller->x,
                        pad, XINPUT_GAMEPAD_X);
                    win32_process_xinput_digital_button(
                        &new_controller->y,
                        &old_controller->y,
                        pad, XINPUT_GAMEPAD_Y);
                    win32_process_xinput_digital_button(
                        &new_controller->left_shoulder,
                        &old_controller->left_shoulder,
                        pad, XINPUT_GAMEPAD_LEFT_SHOULDER);
                    win32_process_xinput_digital_button(
                        &new_controller->right_shoulder,
                        &old_controller->right_shoulder,
                        pad, XINPUT_GAMEPAD_RIGHT_SHOULDER);
                
 
                    auto left_stick_xy =
                            win32_xinput_thumb_resolve_deadzone_normalize(
                                pad->sThumbLX, pad->sThumbLY,
                                left_thumb_norm_deadzone);
                    real32 left_stick_x = left_stick_xy.first;
                    real32 left_stick_y = left_stick_xy.second;
                    auto right_stick_xy =
                            win32_xinput_thumb_resolve_deadzone_normalize(
                                pad->sThumbRX, pad->sThumbRY,
                                right_thumb_norm_deadzone);
                    real32 right_stick_x = right_stick_xy.first;
                    real32 right_stick_y = right_stick_xy.second;
                    // std::stringstream ss;
                    // ss << "stickx = " << left_stick_x
                    // << " sticky=" << left_stick_y << std::endl;
                    // OutputDebugStringA(ss.str().c_str());
                    new_controller->is_analog = true;

                    new_controller->left_stick.start_x
                            = old_controller->left_stick.end_x;
                    new_controller->left_stick.end_x = left_stick_x;
                    new_controller->left_stick.min_x
                            = new_controller->left_stick.max_x
                            = new_controller->left_stick.end_x;
                    new_controller->left_stick.start_y
                            = old_controller->left_stick.end_y;
                    new_controller->left_stick.end_y = left_stick_y;
                    new_controller->left_stick.min_y
                            = new_controller->left_stick.max_y
                            = new_controller->left_stick.end_y;

                    new_controller->right_stick.start_x
                            = old_controller->right_stick.end_x;
                    new_controller->right_stick.end_x = right_stick_x;
                    new_controller->right_stick.min_x
                            = new_controller->right_stick.max_x
                            = new_controller->right_stick.end_x;
                    new_controller->right_stick.start_y
                            = old_controller->right_stick.end_y;
                    new_controller->right_stick.end_y = right_stick_y;
                    new_controller->right_stick.min_y
                            = new_controller->right_stick.max_y
                            = new_controller->right_stick.end_y;
                }
                else
                {
                    // Controller is not connected 
                }
            }

            // TODO: Fix This!!!
            // DirectSound output test
            uint32_t byte_to_lock = 0;
            uint32_t bytes_to_write = 0;
            if (g_sound_buffer)
            {
                DWORD play_cursor;
                DWORD write_cursor;
                if (SUCCEEDED(g_sound_buffer->GetCurrentPosition(&play_cursor,
                                                                 &write_cursor)))
                {
                    // fill the buffer till the play cursor + latency,
                    // start location is the last location we wrote to
                    byte_to_lock = (sound_output.running_sample_index *
                                    sound_output.bytes_per_sample)
                            % sound_output.sound_buffer_size;

                    // TODO: this may be dangerous! overwriting part of memory
                    // between play cursor and write cursor
                    DWORD target_to_cursor = (play_cursor +
                                              sound_output.latency_sample_count
                                              * sound_output.bytes_per_sample)
                            % sound_output.sound_buffer_size;
                    if (byte_to_lock > target_to_cursor)
                    {
                        bytes_to_write = sound_output.sound_buffer_size -
                                byte_to_lock + target_to_cursor;
                    }
                    else
                    {
                        bytes_to_write = target_to_cursor - byte_to_lock;
                    }
                }
            }

            game_sound_buffer game_sound_buffer {};
            if (bytes_to_write > 0)
            {
                game_sound_buffer.samples = samples; 
                game_sound_buffer.sample_count = bytes_to_write /
                        sound_output.bytes_per_sample;
                game_sound_buffer.samples_per_sec = sound_output.samples_per_sec;
            }
        
            game_offscreen_buffer buffer {};
            buffer.width = g_backbuffer.width;
            buffer.height = g_backbuffer.height;
            buffer.pitch = g_backbuffer.pitch;
            buffer.memory = g_backbuffer.memory;

            game_update_and_render(&memory, &buffer, &game_sound_buffer, new_input);

            if (bytes_to_write > 0)
            {
                win32_fill_sound_buffer(&sound_output, &game_sound_buffer,
                                        byte_to_lock, bytes_to_write);
            }
        
            HDC device_context = GetDC(hwnd);
            win32_window_dimension dimension = win32_get_window_dimension(hwnd);
            win32_display_offscreen_buffer(&g_backbuffer, device_context,
                                           dimension.width, dimension.height);
            ReleaseDC(hwnd, device_context);

            // swap game input
            game_input *tmp_input = new_input;
            new_input = old_input;
            old_input = tmp_input;

            uint64_t end_cycle_count = __rdtsc();
            LARGE_INTEGER end_perf_counter;
            QueryPerformanceCounter(&end_perf_counter);

            // use signed, as it may go backward
            int64_t cycles_elapsed = end_cycle_count - last_cycle_count;
            int64_t counter_elapsed = end_perf_counter.QuadPart -
                    last_perf_counter.QuadPart;
            real32 mega_cycles_per_frame = static_cast<real32>(cycles_elapsed) /
                    1000000.0f;
            real32 ms_per_frame = 1000.0f * static_cast<real32>(counter_elapsed)
                    / static_cast<real32>(perf_count_freq);
            real32 fps = static_cast<real32>(perf_count_freq) /
                    static_cast<real32>(counter_elapsed);

            char buf[256];
            sprintf_s(buf, sizeof(buf), "%.2f Mc/f, %.2f ms/f, %.2f fps\n",
                      mega_cycles_per_frame, ms_per_frame, fps);
            OutputDebugStringA(buf);
        
            last_perf_counter = end_perf_counter;
            last_cycle_count = end_cycle_count;
        }
    }
    else
    {
        // fail to allocate memory, no game.
        OutputDebugStringA("Fail to alloc memory to backbuffer, sound buffer, "
                           "or game memory.\n");
    }
    // MessageBoxW(nullptr, L"hello world", L"hello", MB_OK | MB_ICONINFORMATION);
    return 0;
}
