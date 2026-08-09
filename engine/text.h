#pragma once
#include <cstdint>

namespace Text {
    void DrawChar(char c, int x, int y, uint32_t color, int scale = 1);
    void DrawString(const char* str, int x, int y, uint32_t color, int scale = 1);
    int MeasureWidth(const char* str, int scale = 1);
}