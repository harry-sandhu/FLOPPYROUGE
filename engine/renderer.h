#pragma once
#include <windows.h>
#include <cstdint>

namespace Renderer {

struct Sprite {
    int sheet = -1;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int pivotX = 0;
    int pivotY = 0;
};

constexpr int kInternalWidth  = 320;
constexpr int kInternalHeight = 180;
constexpr int kGameplaySheetId = 16;
constexpr int kLobbySheetId = 24;
constexpr int kPickupSheetId = 17;
constexpr int kThemeRuinsSheetId = 18;
constexpr int kThemeForgeSheetId = 19;
constexpr int kThemeCryptSheetId = 20;
constexpr int kThemeFungalSheetId = 21;
constexpr int kThemeDraconicSheetId = 22;

bool Init(HWND hwnd);
void Clear(uint32_t colorRGBA);
void SetPixel(int x, int y, uint32_t colorRGBA);
void DrawRect(int x, int y, int w, int h, uint32_t colorRGBA);
bool LoadSheet(int sheetId, const wchar_t* path);
bool GetSheetSize(int sheetId, int& width, int& height);
void DrawSprite(const Sprite& sprite, int x, int y, int w, int h, bool flipX = false);
Sprite MakeSprite(int sheetId, int x, int y, int w, int h, int pivotX = 0, int pivotY = 0);
Sprite MakeGridSprite(int sheetId, int index0, int cols, int rows);
Sprite MakeScaledSprite(int sheetId, int x, int y, int w, int h, int referenceW, int referenceH);
void Present(); // blits framebuffer to the window, upscaled
void Shutdown();

} // namespace Renderer
