#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

global_variable bool gRunning = false;
global_variable Win32OffscreenBuffer gBackbuffer {};

struct Win32WindowDimension
{
    int width;
    int height;
};

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

internal void RenderWeirdGradient(Win32OffscreenBuffer buffer, int blueOffset, int greenOffset)
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

internal void Win32DisplayOffscreenBuffer(Win32OffscreenBuffer buffer,
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

LRESULT CALLBACK Win32WndProc(
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

        RenderWeirdGradient(gBackbuffer, blueOffset, greenOffset);
        HDC dvcCtx = GetDC(hwnd);
        Win32WindowDimension dimension = Win32GetWindowDimension(hwnd);
        Win32DisplayOffscreenBuffer(gBackbuffer, dvcCtx, dimension.width, dimension.height);
        ReleaseDC(hwnd, dvcCtx);
        ++blueOffset;
        greenOffset += 2;
    }
    
    // MessageBoxW(nullptr, L"hello world", L"hello", MB_OK | MB_ICONINFORMATION);
    return 0;
}
