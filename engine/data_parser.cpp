#include "data_parser.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace {
    // Trims leading/trailing whitespace in place.
    void Trim(char* str) {
        // Trim leading
        char* start = str;
        while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;

        // Trim trailing
        size_t len = std::strlen(start);
        while (len > 0) {
            char c = start[len - 1];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                start[len - 1] = '\0';
                len--;
            } else {
                break;
            }
        }

        if (start != str) std::memmove(str, start, len + 1);
    }

    void CopyBounded(char* dest, size_t destSize, const char* src) {
        std::strncpy(dest, src, destSize - 1);
        dest[destSize - 1] = '\0';
    }
}

const char* DataBlock::GetString(const char* key, const char* def) const {
    for (int i = 0; i < fieldCount; ++i) {
        if (std::strcmp(fields[i].key, key) == 0) return fields[i].value;
    }
    return def;
}

float DataBlock::GetFloat(const char* key, float def) const {
    const char* v = GetString(key, nullptr);
    if (!v) return def;
    return std::strtof(v, nullptr);
}

int DataBlock::GetInt(const char* key, int def) const {
    const char* v = GetString(key, nullptr);
    if (!v) return def;
    return std::atoi(v);
}

bool DataBlock::GetBool(const char* key, bool def) const {
    const char* v = GetString(key, nullptr);
    if (!v) return def;
    return std::strcmp(v, "true") == 0 || std::strcmp(v, "1") == 0 || std::strcmp(v, "yes") == 0;
}

namespace DataParser {

std::vector<DataBlock> ParseFile(const char* path) {
    std::vector<DataBlock> blocks;

    FILE* file = std::fopen(path, "r");
    if (!file) return blocks;

    char line[256];
    DataBlock* current = nullptr;

    while (std::fgets(line, sizeof(line), file)) {
        Trim(line);

        if (line[0] == '\0' || line[0] == ';' || line[0] == '#') continue;

        size_t len = std::strlen(line);
        if (line[0] == '[' && line[len - 1] == ']') {
            blocks.push_back(DataBlock{});
            current = &blocks.back();
            line[len - 1] = '\0'; // strip trailing ]
            CopyBounded(current->name, BLOCK_NAME_LEN, line + 1); // skip leading [
            continue;
        }

        if (!current || current->fieldCount >= MAX_FIELDS) continue;

        char* eq = std::strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char* key = line;
        char* value = eq + 1;
        Trim(key);
        Trim(value);

        DataField& field = current->fields[current->fieldCount++];
        CopyBounded(field.key, FIELD_KEY_LEN, key);
        CopyBounded(field.value, FIELD_VAL_LEN, value);
    }

    std::fclose(file);
    return blocks;
}

} // namespace DataParser