#pragma once
#include <windows.h>

class Timer {
public:
    Timer() {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&last);
    }

    float Tick() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        float dt = float(now.QuadPart - last.QuadPart) / float(freq.QuadPart);
        last = now;
        return dt;
    }

private:
    LARGE_INTEGER freq;
    LARGE_INTEGER last;
};