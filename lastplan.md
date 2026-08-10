# FloppyRogue — Cross-Platform Port Plan (Linux + macOS)

## Status
Do NOT start this until the Windows contest build is finished and submitted
(deadline Sept 4, 2026). This plan is a follow-up project, not part of the
contest deliverable.

## Prep work (safe to do now, low risk)
These can be done inside the Windows build without affecting it:
- [ ] Remove `WinMain` → use `int main()`
- [ ] Remove `GetTickCount()` → use `std::chrono::steady_clock`
- [ ] Remove raw `VK_RETURN` / `VK_SPACE` checks in game code → route
      through `Input::KEY_ENTER` / `Input::KEY_SPACE` / `Input::KEY_ESCAPE`,
      with `Input` translating OS key codes into these internally

## Step 1 — Define the platform interface
Identify the minimal set of functions the game needs from the OS, based on
current `window.h` / `renderer.h`:
- Window: create window, poll events, report close/resize, raw key state
- Renderer: get a pixel buffer, present/blit that buffer to the screen
- Timing: already OS-agnostic via chrono (see prep work)
- Input: already abstracted via `Input::KEY_*` (see prep work)
- Audio (once implemented): submit a PCM buffer for playback

Output: one shared header per module, no OS-specific code in it.

## Step 2 — Restructure engine/ by backend

engine/
window.h <- shared interface, OS-agnostic
window_win32.cpp <- current Win32 impl, moved here unchanged
window_linux.cpp <- new (X11)
window_macos.mm <- new (Cocoa, Objective-C++)
renderer.h
renderer_win32.cpp <- current StretchDIBits impl, moved here
renderer_linux.cpp <- new (XPutImage / XShmPutImage)
renderer_macos.mm <- new (CGImage/NSBitmapImageRep or Metal blit)
audio.h
audio_win32.cpp <- waveOut
audio_linux.cpp <- ALSA or PulseAudio
audio_macos.mm <- CoreAudio

Game code (`game/`, `src/main.cpp`) stays untouched — it only ever calls the
shared headers. CMake selects the correct backend file per target platform.

## Step 3 — Verify Windows build is unaffected
After moving Win32 code into `window_win32.cpp` / `renderer_win32.cpp`,
rebuild and confirm the Windows game behaves identically. This is the
regression checkpoint before writing any new backend.

## Step 4 — Linux backend (X11)
- Raw X11 (no SDL) to stay dependency-free, matching the project's
  no-external-libraries philosophy.
- Window creation + event loop via Xlib.
- Framebuffer blit via `XPutImage` (or `XShmPutImage` for speed).
- Key translation table: X11 keysyms → `Input::KEY_*`.
- Audio: ALSA (lighter) or PulseAudio.

## Step 5 — macOS backend (Cocoa)
- Objective-C++ (`.mm`) required for `NSWindow` / `NSView` / `NSEvent`.
- Framebuffer blit via `CGImage`/`NSBitmapImageRep`, or Metal if
  performance needs it later.
- Key translation table: Cocoa `NSEvent.keyCode` → `Input::KEY_*`.
- Audio: CoreAudio.
- Needs a macOS toolchain/SDK to build — can't cross-compile from the
  current MinGW/Linux setup; needs an actual Mac (or CI runner with Xcode).

## Step 6 — Build system changes
- CMake target logic: compile `*_win32.cpp` only for Windows builds,
  `*_linux.cpp` only for Linux, `*_macos.mm` only for macOS.
- Three separate build configs / CI jobs, each producing its own binary.

## Step 7 — Re-check size budget per platform
- Confirm whether the 1.44MB cap is Windows-contest-specific or should
  still apply to Linux/macOS builds.
- Linux/macOS static linking pulls in different libc/runtime overhead than
  MinGW — re-measure each binary independently, don't assume parity.

## Order of work
1. Prep work (chrono, Input::KEY_*) — safe to land early.
2. Extract interface headers, move Win32 code into `*_win32.cpp`, confirm
   no regression.
3. Build + test Linux/X11 backend.
4. Build + test macOS/Cocoa backend.
5. Wire input translation tables for each.
6. Port audio once procgen audio exists on Windows.
7. Re-measure binary size on all three platforms.