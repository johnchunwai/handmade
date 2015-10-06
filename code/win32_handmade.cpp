#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>

#include <cassert>
#include <cstdint>

#define local_persist static
#define global_variable static
#define internal static


struct Win32OffscreenBuffer
{
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
    ptrdiff_t pitch;
};

struct Win32WindowDimension
{
    int width;
    int height;
};

// TODO: global for now
global_variable bool gRunning = false;
global_variable Win32OffscreenBuffer gBackbuffer {};

// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(XInputGetStateTypedef);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable XInputGetStateTypedef *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(XInputSetStateTypedef);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable XInputSetStateTypedef *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void Win32LoadXInput()
{
    char *xInputDlls[] = {
        "xinput1_4.dll",
        "xinput1_3.dll",
        "xinput9_1_0.dll"
    };
    for (auto &xInputDll : xInputDlls)
    {
        HMODULE xInputLibrary = LoadLibraryA (xInputDll);
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

internal Win32WindowDimension Win32GetWindowDimension(HWND hwnd)
{
    RECT windowRect;
    // rc.top and rc.left are always 0
    GetClientRect(hwnd, &windowRect);
    Win32WindowDimension result {windowRect.right - windowRect.left, windowRect.bottom - windowRect.top};
    return result;
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

internal void RenderWeirdGradient(Win32OffscreenBuffer &buffer, int blueOffset, int greenOffset)
{
    // draw something
    uint8_t *row = static_cast<uint8_t*>(buffer.memory);
    for (int y = 0; y < buffer.height; ++y)
    {
        uint32_t* pixel = reinterpret_cast<uint32_t*>(row);
        for (int x = 0; x < buffer.width; ++x)
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
            uint8_t green = y + greenOffset;
            uint8_t blue = x + blueOffset;
            // little endian - least sig val on smallest addr
            *pixel++ = (red << 16) | (green << 8) | blue;
        }
        row += buffer.pitch;
    }
}

internal void Win32ResizeBackBuffer(Win32OffscreenBuffer &buffer, int width, int height)
{
    // TODO: bulletproof this.
    // maybe don't free first, free after.
    if (buffer.memory)
    {
        VirtualFree(buffer.memory, 0, MEM_RELEASE);
        buffer.memory = nullptr;
    }

    int bytesPerPixel = 4;
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
    int bitmapMemorySize = buffer.width * buffer.height * bytesPerPixel;
    buffer.memory = VirtualAlloc(nullptr, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    assert(buffer.memory);
}

internal void Win32DisplayOffscreenBuffer(Win32OffscreenBuffer &buffer,
                                          HDC dvcCtx, int width, int height)
{
    // Setting to stretch blt mode to prevent artifacts when shrink blt.
    SetStretchBltMode(dvcCtx, STRETCH_DELETESCANS);
    int stretchResult = StretchDIBits(dvcCtx,
                                      0, 0, width, height,
                                      0, 0, buffer.width, buffer.height,
                                      buffer.memory, &buffer.info,
                                      DIB_RGB_COLORS, SRCCOPY);
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
            bool wasDown = ((lparam & (1 << 30)) != 0);
            bool isDown = ((lparam & (1 << 31)) == 0);
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


int CALLBACK wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,  // hPrevInst is useless
    LPWSTR,     // not using lpCmdLine
    int)        // not using nCmdShow
{
    Win32ResizeBackBuffer(gBackbuffer, 1280, 720);
    Win32LoadXInput();
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

    // message pump
    gRunning = true;
    int blueOffset = 0;
    int greenOffset = 0;
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
                bool up = pad.wButtons & XINPUT_GAMEPAD_DPAD_UP;
                bool down = pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                bool left = pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                bool right = pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                bool start = pad.wButtons & XINPUT_GAMEPAD_START;
                bool back = pad.wButtons & XINPUT_GAMEPAD_BACK;
                bool leftShoulder = pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                bool rightShoulder = pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
                bool aButton = pad.wButtons & XINPUT_GAMEPAD_A;
                bool bButton = pad.wButtons & XINPUT_GAMEPAD_B;
                bool xButton = pad.wButtons & XINPUT_GAMEPAD_X;
                bool yButton = pad.wButtons & XINPUT_GAMEPAD_Y;

                int16_t lStickX = pad.sThumbLX;
                int16_t lStickY = pad.sThumbLY;

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
        HDC dvcCtx = GetDC(hwnd);
        Win32WindowDimension dimension = Win32GetWindowDimension(hwnd);
        Win32DisplayOffscreenBuffer(gBackbuffer, dvcCtx, dimension.width, dimension.height);
        ReleaseDC(hwnd, dvcCtx);

        blueOffset++;
    }
    
    // MessageBoxW(nullptr, L"hello world", L"hello", MB_OK | MB_ICONINFORMATION);
    return 0;
}
