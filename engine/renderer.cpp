#include "renderer.h"
#include <vector>

namespace {
    HWND g_hwnd = nullptr;
    std::vector<uint32_t> g_framebuffer;
    BITMAPINFO g_bmi = {};
}

namespace Renderer {

bool Init(HWND hwnd) {
    g_hwnd = hwnd;
    g_framebuffer.assign(kInternalWidth * kInternalHeight, 0xFF000000);

    g_bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    g_bmi.bmiHeader.biWidth       = kInternalWidth;
    g_bmi.bmiHeader.biHeight      = -kInternalHeight; // negative = top-down
    g_bmi.bmiHeader.biPlanes      = 1;
    g_bmi.bmiHeader.biBitCount    = 32;
    g_bmi.bmiHeader.biCompression = BI_RGB;

    return true;
}

void Clear(uint32_t colorRGBA) {
    for (auto& px : g_framebuffer) px = colorRGBA;
}

void SetPixel(int x, int y, uint32_t colorRGBA) {
    if (x < 0 || y < 0 || x >= kInternalWidth || y >= kInternalHeight) return;
    g_framebuffer[y * kInternalWidth + x] = colorRGBA;
}

void DrawRect(int x, int y, int w, int h, uint32_t colorRGBA) {
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i)
            SetPixel(i, j, colorRGBA);
}

void Present() {
    HDC hdc = GetDC(g_hwnd);

    RECT clientRect;
    GetClientRect(g_hwnd, &clientRect);
    int destW = clientRect.right - clientRect.left;
    int destH = clientRect.bottom - clientRect.top;

    SetStretchBltMode(hdc, COLORONCOLOR); // nearest-neighbor, no blending
    StretchDIBits(
        hdc,
        0, 0, destW, destH,
        0, 0, kInternalWidth, kInternalHeight,
        g_framebuffer.data(), &g_bmi,
        DIB_RGB_COLORS, SRCCOPY
    );

    ReleaseDC(g_hwnd, hdc);
}

void Shutdown() {
    g_framebuffer.clear();
}

} // namespace Renderer