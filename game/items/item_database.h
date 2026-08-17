#pragma once
#include "item.h"

constexpr int MAX_ITEM_TEMPLATES = 256;

namespace ItemDatabase {
    bool Load(const char* path);
    int Count();
    const ItemTemplate* Get(int index);
    const ItemTemplate* Find(const char* name);
    int IndexOf(const char* name);
    int Pick(const char* pools, int minTier = 1, int maxTier = 5);
}
