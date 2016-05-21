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

typedef intptr_t    iptr;
typedef uintptr_t   uptr;

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
    s32 offset_start;   // token file offset start
    s32 offset_end;     // token file offset end
};

struct Name
{
    String str;
    u32 hash;
};


template <class E, class U>
struct Flag
{
    U value;
    Flag() { }
    Flag(E bit) : value(bit) { }
    //operator U() { return value; }
};

template <class E, class U>
Flag<E, U> operator | (Flag<E, U> flag, E bit)
{
    flag.value |= bit;
    return flag;
}

template <class E, class U>
U operator & (Flag<E, U> flag, E bit)
{
    return flag.value & bit;
}


} // hplang

#define H_HPLANG_TYPES_H
#endif
