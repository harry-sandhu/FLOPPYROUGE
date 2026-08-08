#pragma once
#include <windows.h>
#include <cstdint>

namespace Renderer {

constexpr int kInternalWidth  = 320;
constexpr int kInternalHeight = 180;

bool Init(HWND hwnd);
void Clear(uint32_t colorRGBA);
void SetPixel(int x, int y, uint32_t colorRGBA);
void DrawRect(int x, int y, int w, int h, uint32_t colorRGBA);
void Present(); // blits framebuffer to the window, upscaled
void Shutdown();

} // namespace Renderer