#include "input.h"
#include <windows.h>
#include <cstring>

namespace {
    bool g_current[256]  = {};
    bool g_previous[256] = {};
}

namespace Input {

void Update() {
    std::memcpy(g_previous, g_current, sizeof(g_current));
    for (int i = 0; i < 256; ++i) {
        g_current[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
    }
}

bool IsDown(int vk) {
    return g_current[vk];
}

bool IsPressed(int vk) {
    return g_current[vk] && !g_previous[vk];
}

} // namespace Input