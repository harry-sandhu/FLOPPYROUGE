#pragma once
#include <vector>

constexpr int MAX_FIELDS = 16;
constexpr int FIELD_KEY_LEN = 32;
constexpr int FIELD_VAL_LEN = 48;
constexpr int BLOCK_NAME_LEN = 32;

struct DataField {
    char key[FIELD_KEY_LEN] = {};
    char value[FIELD_VAL_LEN] = {};
};

struct DataBlock {
    char name[BLOCK_NAME_LEN] = {};
    DataField fields[MAX_FIELDS];
    int fieldCount = 0;

    const char* GetString(const char* key, const char* def = "") const;
    float GetFloat(const char* key, float def = 0.0f) const;
    int GetInt(const char* key, int def = 0) const;
    bool GetBool(const char* key, bool def = false) const;
};

namespace DataParser {
    std::vector<DataBlock> ParseFile(const char* path);
}