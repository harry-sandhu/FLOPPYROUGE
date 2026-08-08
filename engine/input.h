#pragma once

namespace Input {

void Update(); // call once per frame, after Window::PollEvents()
bool IsDown(int virtualKeyCode);      // held right now
bool IsPressed(int virtualKeyCode);   // went down this frame

} // namespace Input