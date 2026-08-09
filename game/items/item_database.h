#pragma once
#include "item.h"

constexpr int MAX_ITEM_TEMPLATES = 64;

namespace ItemDatabase {
    bool Load(const char* path);
    int Count();
    const ItemTemplate* Get(int index);
    const ItemTemplate* Find(const char* name);
    int IndexOf(const char* name);
}
