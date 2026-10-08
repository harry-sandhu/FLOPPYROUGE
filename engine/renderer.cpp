#include "renderer.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <gdiplus.h>

using namespace Gdiplus;

namespace {
    HWND g_hwnd = nullptr;
    std::vector<uint32_t> g_framebuffer;
    BITMAPINFO g_bmi = {};
    ULONG_PTR g_gdiplusToken = 0;

    struct Sheet {
        std::vector<uint32_t> pixels;
        int width = 0;
        int height = 0;
    };

    std::unordered_map<int, Sheet> g_sheets;

    const Sheet* GetSheet(int sheetId) {
        auto it = g_sheets.find(sheetId);
        return it == g_sheets.end() ? nullptr : &it->second;
    }
}

namespace Renderer {

bool Init(HWND hwnd) {
    g_hwnd = hwnd;
    g_framebuffer.assign(kInternalWidth * kInternalHeight, 0xFF000000);

    GdiplusStartupInput startupInput;
    GdiplusStartup(&g_gdiplusToken, &startupInput, nullptr);

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

bool LoadSheet(int sheetId, const wchar_t* path) {
    if (!path || sheetId < 0) return false;
    Bitmap bitmap(path);
    if (bitmap.GetLastStatus() != Ok) return false;

    Rect rect(0, 0, bitmap.GetWidth(), bitmap.GetHeight());
    BitmapData data = {};
    if (bitmap.LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) return false;

    Sheet sheet;
    sheet.width = (int)bitmap.GetWidth();
    sheet.height = (int)bitmap.GetHeight();
    sheet.pixels.resize((size_t)sheet.width * (size_t)sheet.height);

    for (int y = 0; y < sheet.height; ++y) {
        const uint8_t* src = (const uint8_t*)data.Scan0 + y * data.Stride;
        for (int x = 0; x < sheet.width; ++x) {
            uint8_t b = src[x * 4 + 0];
            uint8_t g = src[x * 4 + 1];
            uint8_t r = src[x * 4 + 2];
            uint8_t a = src[x * 4 + 3];
            sheet.pixels[y * sheet.width + x] = (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
        }
    }

    bitmap.UnlockBits(&data);
    g_sheets[sheetId] = std::move(sheet);
    return true;
}

bool GetSheetSize(int sheetId, int& width, int& height) {
    const Sheet* sheet = GetSheet(sheetId);
    if (!sheet) return false;
    width = sheet->width;
    height = sheet->height;
    return width > 0 && height > 0;
}

Sprite MakeSprite(int sheetId, int x, int y, int w, int h, int pivotX, int pivotY) {
    Sprite s;
    s.sheet = sheetId;
    s.x = x;
    s.y = y;
    s.w = w;
    s.h = h;
    s.pivotX = pivotX;
    s.pivotY = pivotY;
    return s;
}

Sprite MakeGridSprite(int sheetId, int index0, int cols, int rows) {
    int sheetW = 0;
    int sheetH = 0;
    if (cols <= 0 || rows <= 0 || index0 < 0 || !GetSheetSize(sheetId, sheetW, sheetH)) {
        return MakeSprite(-1, 0, 0, 0, 0);
    }

    int col = index0 % cols;
    int row = index0 / cols;
    if (row >= rows) return MakeSprite(-1, 0, 0, 0, 0);

    int left = (col * sheetW) / cols;
    int right = ((col + 1) * sheetW) / cols;
    int top = (row * sheetH) / rows;
    int bottom = ((row + 1) * sheetH) / rows;
    return MakeSprite(sheetId, left, top, right - left, bottom - top);
}

Sprite MakeScaledSprite(int sheetId, int x, int y, int w, int h, int referenceW, int referenceH) {
    int sheetW = 0;
    int sheetH = 0;
    if (referenceW <= 0 || referenceH <= 0 || w <= 0 || h <= 0 ||
        !GetSheetSize(sheetId, sheetW, sheetH)) {
        return MakeSprite(-1, 0, 0, 0, 0);
    }

    int left = (x * sheetW) / referenceW;
    int top = (y * sheetH) / referenceH;
    int right = ((x + w) * sheetW) / referenceW;
    int bottom = ((y + h) * sheetH) / referenceH;
    left = std::clamp(left, 0, sheetW);
    top = std::clamp(top, 0, sheetH);
    right = std::clamp(right, left, sheetW);
    bottom = std::clamp(bottom, top, sheetH);
    return MakeSprite(sheetId, left, top, right - left, bottom - top);
}

void DrawSprite(const Sprite& sprite, int x, int y, int w, int h, bool flipX) {
    const Sheet* sheet = GetSheet(sprite.sheet);
    if (!sheet || sheet->pixels.empty() || sprite.w <= 0 || sprite.h <= 0) return;
    for (int dy = 0; dy < h; ++dy) {
        int sy = sprite.y + (dy * sprite.h) / h;
        if (sy < 0 || sy >= sheet->height) continue;
        for (int dx = 0; dx < w; ++dx) {
            int sx = sprite.x + ((flipX ? (w - 1 - dx) : dx) * sprite.w) / w;
            if (sx < 0 || sx >= sheet->width) continue;
            uint32_t px = sheet->pixels[sy * sheet->width + sx];
            uint8_t alpha = (uint8_t)(px >> 24);
            if (alpha == 0) continue;

            int destX = x + dx - sprite.pivotX;
            int destY = y + dy - sprite.pivotY;
            if (alpha == 255) {
                SetPixel(destX, destY, px);
                continue;
            }

            if (destX < 0 || destY < 0 || destX >= kInternalWidth || destY >= kInternalHeight) continue;
            uint32_t dst = g_framebuffer[destY * kInternalWidth + destX];
            uint8_t invAlpha = (uint8_t)(255 - alpha);
            uint8_t r = (uint8_t)((((px >> 16) & 0xFF) * alpha + ((dst >> 16) & 0xFF) * invAlpha) / 255);
            uint8_t g = (uint8_t)((((px >> 8) & 0xFF) * alpha + ((dst >> 8) & 0xFF) * invAlpha) / 255);
            uint8_t b = (uint8_t)(((px & 0xFF) * alpha + (dst & 0xFF) * invAlpha) / 255);
            SetPixel(destX, destY, 0xFF000000 | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b);
        }
    }
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
    g_sheets.clear();
    if (g_gdiplusToken) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

} // namespace Renderer
