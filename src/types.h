#ifndef H_HPLANG_TYPES_H

#include <cstdint>
#include <initializer_list>

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

typedef float       f32;
typedef double      f64;

// NOTE(henrik): b32 should not be implicitely convertible to any integral
// type.  A solution is to wrap it into a struct, but then sizeof(b32)
// might not be 4, as the name implies.
//typedef s32         b32;

struct b32
{
    s32 value;
    b32(bool x) : value((s32)x) { }
    b32(std::initializer_list<bool> x) : value((s32)*x.begin()) { }
    operator bool () { return (bool)value; }
};

static_assert(sizeof(b32) == 4, "assert that sizeof b32 == 4");
static_assert(alignof(b32) == 4, "assert that alignof b32 == 4");


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

struct Open_File
{
    String filename;
    s64 base_end; // The filename base path end position (last '/')
    Pointer contents;
};

struct File_Location
{
    Open_File *file;
    s32 line, column;
    s64 offset_start;   // token file offset start
    s64 offset_end;     // token file offset end
    //// offset from the begin of the file to the start of this line
    //s64 line_offset;
};

inline b32 IsNewlineChar(char c)
{
    return (c == '\n') ||
           (c == '\r') ||
           (c == '\v') ||
           (c == '\f');
}

//const char* SeekToLineStart(File_Location file_loc)
inline const char* SeekToLineStart(Open_File *file, s64 offset)
{
    const char *text = (const char*)file->contents.ptr;
    //s64 text_len = file->contents.size;

    if (offset <= 0)
        return text;

    while (offset > 0)
    {
        if (IsNewlineChar(text[offset - 1]))
            break;
        offset--;
    }
    return text + offset;
}

inline void SeekToEnd(File_Location *file_loc)
{
    Open_File *file = file_loc->file;
    const char *text = (const char*)file->contents.ptr;

    //s64 text_len = file->contents.size;
    //ASSERT(file_loc->offset_end < text_len);

    b32 carriage_return = false;
    s64 pos = file_loc->offset_start;
    while (pos < file_loc->offset_end)
    {
        char c = text[pos];
        file_loc->column++;
        if (c == '\r')
        {
            file_loc->line++;
            file_loc->column = 1;
            carriage_return = true;
        }
        else if (c == '\n' && carriage_return)
        {
            file_loc->column = 1;
            carriage_return = false;
        }
        else if (IsNewlineChar(c))
        {
            file_loc->line++;
            file_loc->column = 1;
            carriage_return = false;
        }
        pos++;
    }
    file_loc->offset_start = pos;
}

struct Name
{
    String str;
    u32 hash;
};

inline bool operator == (const Name &a, const Name &b)
{
    if (a.hash != b.hash) return false;
    if (a.str.size != b.str.size) return false;
    for (s64 i = 0; i < a.str.size; i++)
    {
        if (a.str.data[i] != b.str.data[i])
            return false;
    }
    return true;
}

inline bool operator != (const Name &a, const Name &b)
{
    return !(a == b);
}

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
