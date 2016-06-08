
#include <stdlib.h>
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


void* hp_alloc(s64 size) { return malloc(size); }
void hp_free(void *ptr) { free(ptr); }


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
//{ return fprintf(stdout, "%" PRId64, x); }
{
    if (x == 0) return printf("0");
    int neg = (x < 0);
    x = (neg ? -x : x);

    u64 val = x;
    u64 magnitude = 1;
    while (val > 0)
    {
        val /= 10;
        magnitude *= 10;
    }
    magnitude /= 10;

    s64 written = 0;
    if (neg) written += printf("-");
    while (magnitude > 0)
    {
        s64 y = x / magnitude;
        x -= y * magnitude;
        magnitude /= 10;
        char c = '0' + y;
        written += printf("%c", c);
    }
    return written;
}

s64 hp_fprint_f32(FILE *file, f32 x)
{ return fprintf(stdout, "%f", x); }

s64 hp_fprint_f64(FILE *file, f64 x)
{ return fprintf(stdout, "%f", x); }
