#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int CALLBACK wWinMain(
                      HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine,
                      int nCmdShow
                      )
{
    MessageBox(NULL, L"hello world", L"hello", MB_OK | MB_ICONINFORMATION);
    return 0;
}
