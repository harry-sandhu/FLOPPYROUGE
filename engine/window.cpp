#include "window.h"

namespace {
    HWND g_hwnd = nullptr;
    bool g_shouldQuit = false;

    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_CLOSE:
            case WM_DESTROY:
                g_shouldQuit = true;
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProcA(hwnd, msg, wParam, lParam);
        }
    }
}

namespace Window {

bool Create(int width, int height, const char* title) {
    HINSTANCE hInstance = GetModuleHandleA(nullptr);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "FloppyRogueWindowClass";
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);

    if (!RegisterClassA(&wc)) return false;

    // Adjust so the client area is exactly width x height, not the whole window.
    RECT rect = { 0, 0, width, height };
    DWORD style = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    AdjustWindowRect(&rect, style, FALSE);

    g_hwnd = CreateWindowExA(
        0, wc.lpszClassName, title, style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hwnd) return false;

    ShowWindow(g_hwnd, SW_SHOW);
    return true;
}

bool PollEvents() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) g_shouldQuit = true;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return !g_shouldQuit;
}

HWND GetHandle() { return g_hwnd; }

void Destroy() {
    if (g_hwnd) DestroyWindow(g_hwnd);
    g_hwnd = nullptr;
}

} // namespace Window