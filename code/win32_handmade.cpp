#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cassert>

#define local_persist static
#define global_variable static
#define internal static


global_variable bool gRunning = false;

global_variable BITMAPINFO bitmapInfo{};
global_variable void *bitmapMemory = NULL;

internal void Win32DebugPrintLastError()
{
    wchar_t *msg = NULL;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                   FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL,
                   GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&msg,
                   0,
                   NULL);
    OutputDebugStringW(msg);
    LocalFree(msg);
}

internal void Win32ResizeDibSection(int width, int height)
{
    // TODO: bulletproof this.
    // maybe don't free first, free after.

    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;  // no alpha, the 8-bit padding is for alignment
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;  // negative indicates top down DIB

    // StretchDIBits doesn't need device context or DibSection (compared to BitBlt)
    // just need the memory to be aligned on DWORD boundary
    bitmapMemory = ;
}

internal void Win32UpdateWindow(HDC dvcCtx, int x, int y, int w, int h)
{
    int stretchResult = StretchDIBits(dvcCtx,
                                      x, y, w, h,
                                      x, y, w, h,
                                      bitmapMemory, &bitmapInfo,
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
            // two ways to get it
            // RECT rc;
            // // rc.top and rc.left are always 0
            // GetClientRect(hwnd, &rc);
            // int width = rc.right;
            // int height = rc.bottom;
            int width = LOWORD(lparam);
            int height = HIWORD(lparam);
            Win32ResizeDibSection(width, height);
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
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int w = paint.rcPaint.right - x;
            int h = paint.rcPaint.bottom - y;
            Win32UpdateWindow(dvcCtx, x, y, w, h);
            EndPaint(hwnd, &paint);
        }
        break;

    default:
        lresult = DefWindowProcW(hwnd, msg, wparam, lparam);
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
    wchar_t *wndClassName = L"Handmade Hero Window Class";
    WNDCLASSEXW wndClass = {};  // c++11 aggregate initialization to zero the struct
    wndClass.cbSize = sizeof(WNDCLASSEX);
    // wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
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
                                NULL, NULL, hInstance, NULL);
    if (hwnd == NULL)
    {
        Win32DebugPrintLastError();
        assert(NULL != hwnd);
        return 0;
    }

    // message pump
    gRunning = true;
    while (gRunning)
    {
        MSG msg;
        BOOL msgResult = GetMessageW(&msg, NULL, 0, 0);
        if (msgResult > 0)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        else
        {
            if (msgResult == 0)
            {
                break;
            }
            if (msgResult == -1)
            {
                Win32DebugPrintLastError();
                assert(-1 != msgResult);
                return -1;
            }
        }
    }
    
    // MessageBoxW(NULL, L"hello world", L"hello", MB_OK | MB_ICONINFORMATION);
    return 0;
}
