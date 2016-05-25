
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float   f32;
typedef double  f64;

FILE* hp_get_stdout() { return stdout; }
FILE* hp_get_stderr() { return stderr; }
FILE* hp_get_stdin() { return stdin; }

s64 hp_fwrite(FILE *file, s64 size, u8 *data)
{ return fwrite(data, 1, size, stdout); }

s64 hp_write(s64 size, u8 *data)
{ return fwrite(data, 1, size, stdout); }

s64 hp_fprint_uint(FILE *file, u64 x)
{ return fprintf(stdout, "%" PRIu64, x); }

s64 hp_fprint_int(FILE *file, s64 x)
{ return fprintf(stdout, "%" PRId64, x); }

s64 hp_fprint_f32(FILE *file, f32 x)
{ return fprintf(stdout, "%f", x); }

s64 hp_fprint_f64(FILE *file, f64 x)
{ return fprintf(stdout, "%f", x); }
