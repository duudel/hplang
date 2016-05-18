#ifndef H_HPLANG_COMMON_H

#include "types.h"

namespace hplang
{

template <class T, s64 N>
s64 array_length(T (&)[N])
{ return N; }

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

inline bool operator == (const String &a, const String &b)
{
    if (a.size != b.size) return false;
    for (s64 i = 0; i < a.size; i++)
    {
        if (a.data[i] != b.data[i])
            return false;
    }
    return true;
}

inline bool operator != (const String &a, const String &b)
{
    return !(a == b);
}

inline bool operator == (const Name &a, const Name &b)
{
    if (a.hash != b.hash) return false;
    return a.str == b.str;
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

#define H_HPLANG_COMMON_H
#endif
