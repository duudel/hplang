#ifndef H_HPLANG_TYPES_H

#include <cstdint>

namespace hplang
{

typedef int8_t      s8;
typedef uint8_t     u8;
typedef int16_t     s16;
typedef uint16_t    u16;
typedef int32_t     s32;
typedef uint32_t    u32;
typedef int64_t     s64;
typedef uint64_t    u64;

typedef s32         b32;

typedef float       f32;
typedef double      f64;


struct Pointer
{
    void *ptr;
    s64 size;
};

struct String
{
    s64 size;
    char *data;
};

struct File_Location
{
    String filename;
    s64 line, column;
    s64 offset_start;   // token file offset start
    s64 offset_end;     // token file offset end
    // offset from the begin of the file to the start of previous line
    s64 line_offset;
};

struct Open_File
{
    String filename;
    s64 base_end; // The filename base path end position (last '/')
    Pointer contents;
};

struct Name
{
    String str;
    u32 hash;
};

inline u32 Hash(String str)
{
    // Some prime numbers A and B
    const u32 A = 54059;
    const u32 B = 93563;
    const u32 R = 13;
    const char *s = str.data;
    const char *end = s + str.size;
    u32 result = 31;
    while (s != end)
    {
        // Rotate left by R
        u32 rot = (result << R) | (result >> (32 - R));
        result = (rot * A) ^ (s[0] * B);
        s++;
    }
    return result;
}

inline Name MakeName(String str)
{
    Name result;
    result.str = str;
    result.hash = Hash(str);
    return result;
}

} // hplang

#define H_HPLANG_TYPES_H
#endif
