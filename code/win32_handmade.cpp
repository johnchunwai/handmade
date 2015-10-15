#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
#include <mmsystem.h>
#include <dsound.h>
#include <cassert>
#include <cstdint>
#include <utility>
//#include <sstream>
#include <cmath>

#define local_persist static
#define global_variable static
#define internal static

typedef int32_t bool32;

struct Win32OffscreenBuffer
{
    BITMAPINFO info;
    int32_t width;
    int32_t height;
    ptrdiff_t pitch;
    void *memory;
};

struct Win32WindowDimension
{
    int32_t width;
    int32_t height;
};

// TODO: global for now
global_variable bool32 gRunning = false;
global_variable Win32OffscreenBuffer gBackbuffer {};
global_variable IDirectSoundBuffer *gSoundBuffer = nullptr;
global_variable const float PI_FLOAT = std::atan(1.0f) * 4.0f;
constexpr float epsilon = 0.00001f;
constexpr float gXInputMaxStickVal = 32767.0f;
// constexpr float MIN_XINPUT_STICK_VAL = -32768;

internal float Win32GetXInputStickNormalizedDeadzone(float magnitudeDeadzone)
{
    return magnitudeDeadzone / std::sqrt(gXInputMaxStickVal * gXInputMaxStickVal * 2.0f);
}
global_variable float gXInputLeftThumbNormalizedDeadzone =
        Win32GetXInputStickNormalizedDeadzone(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
// constexpr float gXInputRightThumbNormalizedDeadzone(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD, XINPUT_STATE*)
typedef X_INPUT_GET_STATE(XInputGetStateTypedef);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable XInputGetStateTypedef *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD, XINPUT_VIBRATION*)
typedef X_INPUT_SET_STATE(XInputSetStateTypedef);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable XInputSetStateTypedef *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID, LPDIRECTSOUND *, LPUNKNOWN)
typedef DIRECT_SOUND_CREATE(DirectSoundCreateTypedef);
// DIRECT_SOUND_CREATE(DirectSoundCreateStub)
// {
//     return DSERR_NODRIVER;
// }
// global_variable DirectSoundCreateTypedef *DirectSoundCreate_ = DirectSoundCreateStub;
// #define DirectSoundCreate DirectSoundCreate_

internal void Win32LoadXInput()
{
    char *xInputDlls[] = {
        "xinput1_4.dll",
        "xinput1_3.dll",
        "xinput9_1_0.dll"
    };
    for (auto &xInputDll : xInputDlls)
    {
        HMODULE xInputLibrary = LoadLibraryA(xInputDll);
        if (xInputLibrary)
        {
            XInputGetState = reinterpret_cast<XInputGetStateTypedef*>(GetProcAddress(xInputLibrary,
                                                                                     "XInputGetState"));
            XInputSetState = reinterpret_cast<XInputSetStateTypedef*>(GetProcAddress(xInputLibrary,
                                                                                     "XInputSetState"));
            assert(XInputGetState && XInputSetState);
            break;
        }
    }
}

internal std::pair<float, float> Win32NormalizeXInputStickMagnitude(float xVal, float yVal, float deadzone)
{
    // normalize the input first (-1.0f to 1.0f)
    // max with -1 because abs(min val) is 1 great then max val
    xVal = fmaxf(-1.0f, xVal / gXInputMaxStickVal);
    yVal = fmaxf(-1.0f, yVal / gXInputMaxStickVal);

    // adjust for deadzone
    if (xVal >= 0.0f)
    {
        xVal = xVal < deadzone ? 0 : xVal - deadzone;
    }
    else
    {
        xVal = xVal > -deadzone ? 0 : xVal + deadzone;
    }
    if (yVal >= 0.0f)
    {
        yVal = yVal < deadzone ? 0 : yVal - deadzone;
    }
    else
    {
        yVal = yVal > -deadzone ? 0 : yVal + deadzone;
    }

    // scale the val for smooth transition outside deadzone
    xVal *= (1 / (1 - deadzone));
    yVal *= (1 / (1 - deadzone));

    return std::make_pair(xVal, yVal);
}

internal Win32WindowDimension Win32GetWindowDimension(HWND hwnd)
{
    RECT windowRect;
    // rc.top and rc.left are always 0
    GetClientRect(hwnd, &windowRect);
    Win32WindowDimension result {windowRect.right - windowRect.left, windowRect.bottom - windowRect.top};
    return result;
}

internal void Win32InitDSound(HWND hwnd, uint32_t soundChannels, uint32_t samplesPerSec, uint32_t bufferSize)
{
    // load library
    HMODULE dSoundLibrary = LoadLibraryA("dsound.dll");
    if (dSoundLibrary)
    {
        // get DirectSound object - cooperative mode
        DirectSoundCreateTypedef *DirectSoundCreate = reinterpret_cast<DirectSoundCreateTypedef*>(
            GetProcAddress(dSoundLibrary, "DirectSoundCreate"));
        IDirectSound *directSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &directSound, 0)))
        {
            // just simple 2 ch stereo sound
            WAVEFORMATEX waveFormat {};
            waveFormat.wFormatTag = WAVE_FORMAT_PCM;
            waveFormat.nChannels = static_cast<WORD>(soundChannels);
            waveFormat.nSamplesPerSec = samplesPerSec;
            waveFormat.wBitsPerSample = 16;
            waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
            waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
            // waveFormat.cbSize = 0;
            
            if (SUCCEEDED(directSound->SetCooperativeLevel(hwnd, DSSCL_PRIORITY)))
            {
                // create a primary buffer (object that mixes all sounds)
                DSBUFFERDESC bufferDesc {};
                bufferDesc.dwSize = sizeof(DSBUFFERDESC);
                bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;  // DSBCAPS_GLOBALFOCUS?
                // bufferDesc.dwBufferBytes = 0;
                // bufferDesc.lpwfxFormat = nullptr;
                // bufferDesc.guid3DAlgorithm = DS3DALG_DEFAULT;  // same as 0
                
                IDirectSoundBuffer *primaryBuffer;
                if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDesc, &primaryBuffer, nullptr)))
                {
                    if (SUCCEEDED(primaryBuffer->SetFormat(&waveFormat)))
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
            DSBUFFERDESC bufferDesc {};
            bufferDesc.dwSize = sizeof(DSBUFFERDESC);
            bufferDesc.dwFlags = 0;  // DSBCAPS_GLOBALFOCUS?
            bufferDesc.dwBufferBytes = bufferSize;
            bufferDesc.lpwfxFormat = &waveFormat;
            // bufferDesc.guid3DAlgorithm = DS3DALG_DEFAULT;  // same as 0
            if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDesc, &gSoundBuffer, nullptr)))
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

internal void Win32DebugPrintLastError()
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

internal void RenderWeirdGradient(Win32OffscreenBuffer &buffer, int32_t blueOffset, int32_t greenOffset)
{
    // draw something
    uint8_t *row = static_cast<uint8_t*>(buffer.memory);
    for (int32_t y = 0; y < buffer.height; ++y)
    {
        uint32_t* pixel = reinterpret_cast<uint32_t*>(row);
        for (int32_t x = 0; x < buffer.width; ++x)
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
            uint8_t green = static_cast<uint8_t>(y + greenOffset);
            uint8_t blue = static_cast<uint8_t>(x + blueOffset);
            // little endian - least sig val on smallest addr
            *pixel++ = (red << 16) | (green << 8) | blue;
        }
        row += buffer.pitch;
    }
}

internal void Win32ResizeBackBuffer(Win32OffscreenBuffer &buffer, int32_t width, int32_t height)
{
    // TODO: bulletproof this.
    // maybe don't free first, free after.
    if (buffer.memory)
    {
        VirtualFree(buffer.memory, 0, MEM_RELEASE);
        buffer.memory = nullptr;
    }

    int32_t bytesPerPixel = 4;
    buffer.width = width;
    buffer.height = height;

    buffer.info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    buffer.info.bmiHeader.biPlanes = 1;
    buffer.info.bmiHeader.biBitCount = 32;  // no alpha, the 8-bit padding is for alignment
    buffer.info.bmiHeader.biCompression = BI_RGB;
    buffer.info.bmiHeader.biWidth = buffer.width;
    buffer.info.bmiHeader.biHeight = -buffer.height;  // negative indicates top down DIB
    
    buffer.pitch = buffer.width * bytesPerPixel;

    // StretchDIBits doesn't need device context or DibSection (compared to BitBlt)
    // just need the memory to be aligned on DWORD boundary
    int32_t bitmapMemorySize = buffer.width * buffer.height * bytesPerPixel;
    buffer.memory = VirtualAlloc(nullptr, bitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(buffer.memory);
}

internal void Win32DisplayOffscreenBuffer(Win32OffscreenBuffer &buffer,
                                          HDC dvcCtx, int32_t width, int32_t height)
{
    // Setting to stretch blt mode to prevent artifacts when shrink blt.
    SetStretchBltMode(dvcCtx, STRETCH_DELETESCANS);
    int32_t stretchResult = StretchDIBits(dvcCtx,
                                      0, 0, width, height,
                                      0, 0, buffer.width, buffer.height,
                                      buffer.memory, &buffer.info,
                                      DIB_RGB_COLORS, SRCCOPY);
    assert(stretchResult);
}

internal LRESULT CALLBACK Win32WndProc(
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
            gRunning = false;
        }
        break;

    case WM_DESTROY:
        {
            // TODO: handle this as error - recreate window?
            gRunning = false;
        }
        break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
        {
            WPARAM vkCode = wparam;
            bool32 wasDown = (lparam & (1 << 30));
            bool32 isDown = (lparam & (1 << 31));
            if (isDown != wasDown)
            {
                switch (vkCode)
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
                        if (isDown)
                        {
                            OutputDebugStringA("isDown ");
                        }
                        if (wasDown)
                        {
                            OutputDebugStringA("wasDown ");
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
                        bool32 altDown = (lparam & (1 << 29));
                        if (altDown)
                        {
                            gRunning = false;
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
            HDC dvcCtx = BeginPaint(hwnd, &paint);
            Win32WindowDimension dimension = Win32GetWindowDimension(hwnd);
            Win32DisplayOffscreenBuffer(gBackbuffer, dvcCtx, dimension.width, dimension.height);
            EndPaint(hwnd, &paint);
        }
        break;

    default:
        {
            lresult = DefWindowProcW (hwnd, msg, wparam, lparam);
        }
        break;
    }
    return lresult;
}

struct Win32SoundOutput
{
    int16_t toneVolume;
    uint32_t soundChannels;
    uint32_t toneHz;
    uint32_t samplesPerSec;
    uint32_t wavePeriod;
    uint32_t halfWavePeriod;
    uint32_t bytesPerSample;
    uint32_t soundBufferSize;
    uint32_t runningSampleIndex;
};

internal void Win32FillSoundBuffer(IDirectSoundBuffer *soundBuffer, Win32SoundOutput &soundOutput,
                                   DWORD byteToLock, DWORD bytesToWrite)
{
    if (bytesToWrite == 0)
    {
        return;
    }
    // int16_t int16_t int16_t ...
    // [left   right]  [left   right] ...
    // since it's a circular buffer, the lock region might be consecutive, or chopped into 2 regions:
    // one at the back of the buffer, the other at the front
    constexpr int32_t maxRegions = 2;
    void *regions[maxRegions] = {};
    DWORD regionSizes[maxRegions] = {};
                
    if (SUCCEEDED(soundBuffer->Lock(
            byteToLock,
            bytesToWrite,
            &regions[0], &regionSizes[0],
            &regions[1], &regionSizes[1],
            0)))
    {
        for (int32_t regionIndex = 0; regionIndex < maxRegions; ++regionIndex)
        {
            // TODO assert regionSizes are valid
            assert(0 == (regionSizes[regionIndex] % (sizeof(int16_t) * soundOutput.soundChannels)));
            // Just do a sine wave
            int16_t *sampleOut = static_cast<int16_t*>(regions[regionIndex]);
            for (DWORD sampleIndex = 0, sampleEnd = regionSizes[regionIndex] / soundOutput.bytesPerSample;
                 sampleIndex < sampleEnd;
                 ++sampleIndex)
            {
                float sineVal = sin(soundOutput.runningSampleIndex * PI_FLOAT / soundOutput.halfWavePeriod);
                int16_t sampleVal = static_cast<int16_t>(sineVal * soundOutput.toneVolume);
                // square wave
                // int16_t sampleVal = ((soundOutput.runningSampleIndex / soundOutput.halfWavePeriod) % 2) ?
                //         soundOutput.toneVolume : -soundOutput.toneVolume;
                *sampleOut++ = sampleVal;
                *sampleOut++ = sampleVal;
                ++soundOutput.runningSampleIndex;
            }
        }
        soundBuffer->Unlock(regions[0], regionSizes[0], regions[1], regionSizes[1]);
    }
}

int32_t CALLBACK wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,  // hPrevInst is useless
    LPWSTR,     // not using lpCmdLine
    int)        // not using nCmdShow
{
    Win32LoadXInput();
    Win32ResizeBackBuffer(gBackbuffer, 1280, 720);
    wchar_t *wndClassName = L"Handmade Hero Window Class";
    WNDCLASSEXW wndClass = {};  // c++11 aggregate initialization to zero the struct
    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = Win32WndProc;
    wndClass.hInstance = hInstance;
    wndClass.lpszClassName = wndClassName;

    ATOM wndClsAtom = RegisterClassExW(&wndClass);
    if (0 == wndClsAtom)
    {
        Win32DebugPrintLastError();
        assert(0 != wndClsAtom);
        return 0;
    }

    HWND hwnd = CreateWindowExW(0, wndClassName, L"Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                nullptr, nullptr, hInstance, nullptr);
    if (hwnd == nullptr)
    {
        Win32DebugPrintLastError();
        assert(nullptr != hwnd);
        return 0;
    }

    // graphics test
    int32_t blueOffset = 0;
    int32_t greenOffset = 0;

    // test sound
    Win32SoundOutput soundOutput {};
    soundOutput.toneVolume = 1000;
    soundOutput.soundChannels = 2;
    soundOutput.toneHz = 261;
    soundOutput.samplesPerSec = 48000;
    soundOutput.wavePeriod = soundOutput.samplesPerSec / soundOutput.toneHz;
    soundOutput.halfWavePeriod = soundOutput.wavePeriod / 2;
    soundOutput.bytesPerSample = sizeof(int16_t) * soundOutput.soundChannels;
    soundOutput.soundBufferSize = soundOutput.samplesPerSec * soundOutput.bytesPerSample * 2; // 2 sec buffer
    soundOutput.runningSampleIndex = 0;

    // create buffer for 2 sec
    Win32InitDSound(hwnd, soundOutput.soundChannels, soundOutput.samplesPerSec, soundOutput.soundBufferSize);
    if (gSoundBuffer)
    {
        // fill the whole buffer and started playing sound.
        Win32FillSoundBuffer(gSoundBuffer, soundOutput, 0, soundOutput.soundBufferSize);
        gSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
    }

    gRunning = true;
    while (gRunning)
    {
        MSG msg;
        // BOOL msgResult = GetMessageW(&msg, nullptr, 0, 0);
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            // if (msgResult > 0)
        {
            if (msg.message == WM_QUIT)
            {
                gRunning = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!gRunning)
        {
            break;
        }

        for (DWORD controllerIndex = 0; controllerIndex< XUSER_MAX_COUNT; ++controllerIndex)
        {
            XINPUT_STATE controllerState {};
            // Simply get the state of the controller from XInput.
            if (ERROR_SUCCESS == XInputGetState(controllerIndex, &controllerState))
            {
                // Controller is connected
                // dwPacketNumber indicates whether there have been state changes
                controllerState.dwPacketNumber;
                XINPUT_GAMEPAD &pad = controllerState.Gamepad;
                // these are what we care about
                // bool32 up = pad.wButtons & XINPUT_GAMEPAD_DPAD_UP;
                // bool32 down = pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                // bool32 left = pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                // bool32 right = pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                // bool32 start = pad.wButtons & XINPUT_GAMEPAD_START;
                // bool32 back = pad.wButtons & XINPUT_GAMEPAD_BACK;
                // bool32 leftShoulder = pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                // bool32 rightShoulder = pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
                // bool32 aButton = pad.wButtons & XINPUT_GAMEPAD_A;
                // bool32 bButton = pad.wButtons & XINPUT_GAMEPAD_B;
                bool32 xButton = pad.wButtons & XINPUT_GAMEPAD_X;
                bool32 yButton = pad.wButtons & XINPUT_GAMEPAD_Y;

                auto stickXY = Win32NormalizeXInputStickMagnitude(pad.sThumbLX, pad.sThumbLY,
                                                                  gXInputLeftThumbNormalizedDeadzone);
                float lStickX = stickXY.first;
                float lStickY = stickXY.second;
                // std::stringstream ss;
                // ss << "stickx = " << lStickX << " sticky=" << lStickY << std::endl;
                // OutputDebugStringA(ss.str().c_str());

                if (lStickX > epsilon || lStickX < -epsilon)
                {
                    blueOffset -= static_cast<int>(lStickX * 10.0f);
                }
                if (lStickY > epsilon || lStickY < -epsilon)
                {
                    greenOffset += static_cast<int>(lStickY * 5.0f);
                }
                if (xButton)
                {
                    blueOffset += 5;

                    // Add some vibration left motor
                    XINPUT_VIBRATION vibration {};
                    vibration.wLeftMotorSpeed = 65535;
                    XInputSetState(controllerIndex, &vibration);
                }
                
                if (yButton)
                {
                    greenOffset += 2;

                    // Add some vibration right motor
                    XINPUT_VIBRATION vibration {};
                    vibration.wRightMotorSpeed = 10000;
                    XInputSetState(controllerIndex, &vibration);
                }
            }
            else
            {
                // Controller is not connected 
            }
        }

        RenderWeirdGradient(gBackbuffer, blueOffset, greenOffset);

        // DirectSound output test
        if (gSoundBuffer)
        {
            DWORD playCursor;
            DWORD writeCursor;
            if (SUCCEEDED(gSoundBuffer->GetCurrentPosition(&playCursor, &writeCursor)))
            {
                // fill the buffer till the play cursor
                DWORD byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample)
                        % soundOutput.soundBufferSize;
                DWORD bytesToWrite;
                if (byteToLock == playCursor)
                {
                    // this happens when we write the full buffer but player cursor hasn't moved.
                    // so, write nothing or it'll screw up.
                    bytesToWrite = 0;
                }
                else if (byteToLock > playCursor)
                {
                    bytesToWrite = soundOutput.soundBufferSize - byteToLock + playCursor;
                }
                else
                {
                    bytesToWrite = playCursor - byteToLock;
                }
                Win32FillSoundBuffer(gSoundBuffer, soundOutput, byteToLock, bytesToWrite);
            }
        }        
        HDC dvcCtx = GetDC(hwnd);
        Win32WindowDimension dimension = Win32GetWindowDimension(hwnd);
        Win32DisplayOffscreenBuffer(gBackbuffer, dvcCtx, dimension.width, dimension.height);
        ReleaseDC(hwnd, dvcCtx);
    }
    
    // MessageBoxW(nullptr, L"hello world", L"hello", MB_OK | MB_ICONINFORMATION);
    return 0;
}
