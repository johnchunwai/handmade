#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cassert>

void DebugPrintLastError()
{
    wchar_t* msg {0};
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

LRESULT CALLBACK wndProc(
    _In_ HWND   hwnd,
    _In_ UINT   msg,
    _In_ WPARAM wparam,
    _In_ LPARAM lparam)
{
    LRESULT lresult = 0;
            
    switch (msg)
    {
    case WM_CREATE:
        OutputDebugStringA("WM_CREATE\n");
        break;
    case WM_DESTROY:
        OutputDebugStringA("WM_DESTROY\n");
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        OutputDebugStringA("WM_CLOSE\n");
        DestroyWindow(hwnd);
        break;
    case WM_NCDESTROY:
        OutputDebugStringA("WM_NCDESTROY\n");
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC dvcCtx = BeginPaint(hwnd, &paint);
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int w = paint.rcPaint.right - x;
            int h = paint.rcPaint.bottom - y;
            static DWORD op = WHITENESS;
            PatBlt(dvcCtx, x, y, w, h, op);
            if (op == WHITENESS)
            {
                op = BLACKNESS;
            }
            else
            {
                op = WHITENESS;
            }
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
    wchar_t* wndClassName = L"Handmade Hero Window Class";
    WNDCLASSEXW wndClass = {};  // c++11 aggregate initialization to zero the struct
    wndClass.cbSize = sizeof(WNDCLASSEX);
    // wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndClass.lpfnWndProc = wndProc;
    wndClass.hInstance = hInstance;
    wndClass.lpszClassName = wndClassName;

    ATOM wndClsAtom = RegisterClassExW(&wndClass);
    if (0 == wndClsAtom)
    {
        DebugPrintLastError();
        assert(0 != wndClsAtom);
        return 0;
    }

    HWND hwnd = CreateWindowExW(0, wndClassName, L"Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                NULL, NULL, hInstance, NULL);
    if (hwnd == NULL)
    {
        DebugPrintLastError();
        assert(NULL != hwnd);
        return 0;
    }

    // message pump
    MSG msg;
    for (;;)  // same as while (true) but with compiler warning for cond exp always const
    {
        BOOL getMsgResult = GetMessageW(&msg, NULL, 0, 0);
        if (getMsgResult == 0)
        {
            break;
        }
        if (getMsgResult == -1)
        {
            DebugPrintLastError();
            assert(-1 != getMsgResult);
            return -1;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    // MessageBoxW(NULL, L"hello world", L"hello", MB_OK | MB_ICONINFORMATION);
    return (int)msg.wParam;  // cast is needed for 64bit app or windows warns we're downcasting
}
