#pragma once
#include <windows.h>

namespace Window {

bool Create(int width, int height, const char* title);
bool PollEvents(); // returns false when the app should quit
HWND GetHandle();
void Destroy();

} // namespace Window