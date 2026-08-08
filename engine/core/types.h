#pragma once

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Rect {
    float x, y, w, h;

    bool Intersects(const Rect& other) const {
        return x < other.x + other.w &&
               x + w > other.x &&
               y < other.y + other.h &&
               y + h > other.y;
    }
};