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

typedef intptr_t    iptr;
typedef uintptr_t   uptr;

typedef float       f32;
typedef double      f64;

// NOTE(henrik): b32 should not be implicitely convertible to any integral type.
// No easy and portable way of doing this.
typedef s32         b32;

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

inline File_Location NoFileLocation()
{ return { }; }

struct Name
{
    String str;
    u32 hash;
};

enum Codegen_Target
{
    CGT_AMD64_Windows,
    CGT_AMD64_Unix,

    CGT_COUNT
};


template <class E, class U>
struct Flag
{
    U value;
    Flag() { }
    Flag(E bit) : value(bit) { }
    Flag(const Flag &other) : value(other.value) { }
    //operator U() { return value; }

    Flag& operator |= (E bit) { value |= bit; return *this; }
};

template <class E, class U>
Flag<E, U> operator | (Flag<E, U> flag, E bit)
{
    flag.value |= bit;
    return flag;
}

template <class E, class U>
Flag<E, U> operator | (Flag<E, U> flag1, Flag<E, U> flag2)
{
    flag1.value |= flag2.value;
    return flag1;
}

template <class E, class U>
U operator & (Flag<E, U> flag, E bit)
{
    return flag.value & bit;
}

template <class E, class U>
U operator & (Flag<E, U> flag1, Flag<E, U> flag2)
{
    return flag1.value & flag2.value;
}

} // hplang

#define H_HPLANG_TYPES_H
#endif
