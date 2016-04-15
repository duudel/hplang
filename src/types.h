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
    Pointer contents;
};

} // hplang

#define H_HPLANG_TYPES_H
#endif
