
#include "../src/hplang.h"
#include "../src/common.h"
#include "../src/compiler.h"
#include "../src/token.h"
#include "../src/ast_types.h"

#include <cstdio>
#include <cinttypes>
#include <cstdlib> // for WIFEXITED etc.

#define NO_CRASH_TESTS

FILE *nulldev;
FILE *outfile;

using namespace hplang;

struct Test_Context
{
    s64 errors;
};

bool report_test(Test_Context *test_ctx, bool x, const char *xs,
        const char *file, s64 line)
{
    if (!x)
    {
        test_ctx->errors++;
        fprintf(stderr, "%s:%" PRId64 ":1: TEST FAILURE\n\n", file, line);
        fprintf(stderr, "%s\n\n", xs);
        return false;
    }
    return true;
}

#define TEST(x) report_test(test_ctx, x, #x, __FILE__, __LINE__)

// beer_test.hp
void Beer_Test(Test_Context *test_ctx, Compiler_Context comp_ctx)
{
    Environment env = comp_ctx.env;
    Symbol *main_sym = LookupSymbol(&env, PushName(&comp_ctx.arena, "main"));
    Symbol *beer_sym = LookupSymbol(&env, PushName(&comp_ctx.arena, "beer"));
    Symbol *bottles_sym = LookupSymbol(&env, PushName(&comp_ctx.arena, "bottles"));

    if (TEST(main_sym != nullptr))
    {
        TEST(main_sym->sym_type == SYM_Function);
        if (TEST(main_sym->type->tag == TYP_Function))
        {
            TEST(TypeIsIntegral(main_sym->type->function_type.return_type));
            TEST(TypeIsSigned(main_sym->type->function_type.return_type));
        }
    }
    if (TEST(beer_sym != nullptr))
    {
        TEST(beer_sym->sym_type == SYM_Function);
        if (TEST(beer_sym->type->tag == TYP_Function))
        {
            TEST(TypeIsVoid(beer_sym->type->function_type.return_type));
        }
    }
    if (TEST(bottles_sym != nullptr))
    {
        TEST(bottles_sym->sym_type == SYM_Function);
        if (TEST(bottles_sym->type->tag == TYP_Function))
        {
            TEST(TypeIsVoid(bottles_sym->type->function_type.return_type));
        }
    }
}

// recursive_rt_infer.hp
void RecursiveRtInfer_Test(Test_Context *test_ctx, Compiler_Context comp_ctx)
{
    Environment env = comp_ctx.env;
    Symbol *test_sym = LookupSymbol(&env, PushName(&comp_ctx.arena, "test"));
    if (TEST(test_sym != nullptr))
    {
        TEST(test_sym->sym_type == SYM_Function);
        TEST(test_sym->type->tag == TYP_Function);
    }
    //PrintType(comp_ctx.error_ctx.file, test->type);
    //fprintf(stderr, "\n");
}



struct Line_Col
{
    s32 line, column;
};

typedef void (*Test_Function)(Test_Context *test_ctx, Compiler_Context comp_ctx);

// Test cases that have crashed the compiler
struct Crash_Test
{
    const char *source_filename;
};

struct Fail_Test
{
    Compilation_Phase stop_after;
    const char *source_filename;
    Line_Col fail_location;
};

struct Succeed_Test
{
    Compilation_Phase stop_after;
    const char *source_filename;
    Test_Function test_func;
};

struct Execute_Test
{
    const char *source_filename;
    const char *expected_output_filename;
    s32 expected_exit_code;
};

#ifndef NO_CRASH_TESTS
static Crash_Test crash_tests[] = {
    { "tests/crash/id-000000,sig-11,src-000000,op-flip1,pos-43" },
    { "tests/crash/id-000001,sig-11,src-000000,op-flip1,pos-43" },
    { "tests/crash/id-000002,sig-11,src-000000,op-flip1,pos-67" },
    { "tests/crash/id-000003,sig-11,src-000000,op-flip1,pos-75" },
    { "tests/crash/id-000004,sig-11,src-000000,op-flip1,pos-81" },
    { "tests/crash/id-000005,sig-11,src-000000,op-flip1,pos-83" },
    { "tests/crash/id-000006,sig-11,src-000000,op-flip1,pos-84" },
    { "tests/crash/id-000007,sig-11,src-000000,op-flip1,pos-89" },
    { "tests/crash/id-000008,sig-11,src-000000,op-flip1,pos-121" },
    { "tests/crash/id-000009,sig-11,src-000000,op-flip1,pos-136" },
    { "tests/crash/id-000010,sig-11,src-000000,op-flip1,pos-169" },
    { "tests/crash/id-000011,sig-11,src-000000,op-flip1,pos-180" },
    { "tests/crash/id-000012,sig-11,src-000000,op-flip1,pos-202" },
    { "tests/crash/id-000013,sig-11,src-000000,op-flip1,pos-229" },
    { "tests/crash/id-000014,sig-11,src-000000,op-flip1,pos-258" },
    { "tests/crash/id-000015,sig-11,src-000000,op-flip1,pos-275" },
    { "tests/crash/id-000016,sig-11,src-000000,op-flip1,pos-293" },
    { "tests/crash/id-000017,sig-11,src-000000,op-flip1,pos-296" },
    { "tests/crash/id-000018,sig-11,src-000000,op-flip1,pos-321" },
    { "tests/crash/id-000019,sig-11,src-000000,op-flip1,pos-350" },
    { "tests/crash/id-000020,sig-11,src-000000,op-flip1,pos-366" },
    { "tests/crash/id-000021,sig-11,src-000000,op-flip1,pos-381" },
    { "tests/crash/id-000022,sig-11,src-000000,op-flip1,pos-427" },
    { "tests/crash/id-000023,sig-11,src-000000,op-flip1,pos-472" },
    { "tests/crash/id-000024,sig-11,src-000000,op-flip1,pos-519" },
    { "tests/crash/id-000025,sig-11,src-000000,op-flip1,pos-531" },
    { "tests/crash/id-000026,sig-11,src-000000,op-flip2,pos-43" },
    { "tests/crash/id-000027,sig-11,src-000000,op-flip2,pos-43" },
    { "tests/crash/id-000028,sig-11,src-000000,op-flip2,pos-43" },
    { "tests/crash/id-000029,sig-11,src-000000,op-flip2,pos-43" },
    { "tests/crash/id-000030,sig-11,src-000000,op-flip2,pos-528" },
    { "tests/crash/id-000031,sig-11,src-000000,op-flip2,pos-529" },
    { "tests/crash/id-000032,sig-11,src-000000,op-flip2,pos-533" },
    { "tests/crash/id-000033,sig-11,src-000000,op-flip4,pos-43" },
    { "tests/crash/id-000034,sig-11,src-000000,op-flip4,pos-43" },
    { "tests/crash/id-000035,sig-11,src-000000,op-flip4,pos-292" },
    { "tests/crash/id-000036,sig-11,src-000000,op-arith8,pos-43,val-+5" },
    { "tests/crash/id-000037,sig-11,src-000000,op-arith8,pos-43,val-+10" },
    { "tests/crash/id-000038,sig-11,src-000000,op-arith8,pos-43,val-+11" },
    { "tests/crash/id-000039,sig-11,src-000000,op-arith8,pos-43,val-+14" },
    { "tests/crash/id-000040,sig-11,src-000000,op-arith8,pos-44,val--34" },
    { "tests/crash/id-000041,sig-11,src-000000,op-arith8,pos-102,val-+23" },
    { "tests/crash/id-000042,sig-11,src-000000,op-arith8,pos-155,val-+23" },
    { "tests/crash/id-000043,sig-11,src-000000,op-arith8,pos-394,val-+23" },
    { "tests/crash/id-000044,sig-11,src-000000,op-arith8,pos-438,val-+31" },
    { "tests/crash/id-000045,sig-11,src-000000,op-arith8,pos-440,val-+31" },
    { "tests/crash/id-000046,sig-11,src-000000,op-arith8,pos-527,val-+23" },
    { "tests/crash/id-000047,sig-11,src-000000,op-int8,pos-66,val-+0" },
    { "tests/crash/id-000048,sig-11,src-000000,op-int8,pos-69,val-+0" },
    { "tests/crash/id-000049,sig-11,src-000000,op-int8,pos-73,val-+0" },
    { "tests/crash/id-000050,sig-11,src-000000,op-int8,pos-82,val-+0" },
    { "tests/crash/id-000051,sig-05,src-000000,op-int8,pos-121,val-+0" },
    { "tests/crash/id-000052,sig-11,src-000000,op-int8,pos-533,val-+64" },
    { "tests/crash/id-000053,sig-11,src-000000,op-int16,pos-44,val-+100" },
    { "tests/crash/id-000054,sig-11,src-000000,op-int16,pos-61,val-+32" },
    { "tests/crash/id-000055,sig-11,src-000000,op-int16,pos-64,val-+32" },
    { "tests/crash/id-000056,sig-11,src-000000,op-int16,pos-69,val-+32" },
    { "tests/crash/id-000057,sig-11,src-000000,op-int16,pos-69,val-+100" },
    { "tests/crash/id-000058,sig-11,src-000000,op-int16,pos-70,val-+32" },
    { "tests/crash/id-000059,sig-11,src-000000,op-int16,pos-70,val-+100" },
    { "tests/crash/id-000060,sig-11,src-000000,op-int16,pos-71,val-+100" },
    { "tests/crash/id-000061,sig-11,src-000000,op-int16,pos-80,val-+100" },
    { "tests/crash/id-000062,sig-11,src-000000,op-int16,pos-81,val-+100" },
    { "tests/crash/id-000063,sig-11,src-000000,op-int16,pos-83,val-+100" },
    { "tests/crash/id-000064,sig-11,src-000000,op-int16,pos-85,val-+32" },
    { "tests/crash/id-000065,sig-05,src-000000,op-int16,pos-229,val-+0" },
    { "tests/crash/id-000066,sig-11,src-000000,op-int16,pos-279,val-+0" },
    { "tests/crash/id-000067,sig-11,src-000000,op-int16,pos-512,val-+32" },
    { "tests/crash/id-000068,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000069,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000070,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000071,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000072,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000073,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000074,sig-05,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000075,sig-05,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000076,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000077,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000078,sig-11,src-000000,op-havoc,rep-64" },
    { "tests/crash/id-000079,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000080,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000081,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000082,sig-11,src-000000,op-havoc,rep-128" },
    { "tests/crash/id-000083,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000084,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000085,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000086,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000087,sig-11,src-000000,op-havoc,rep-64" },
    { "tests/crash/id-000088,sig-11,src-000000,op-havoc,rep-64" },
    { "tests/crash/id-000089,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000090,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000091,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000092,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000093,sig-11,src-000000,op-havoc,rep-64" },
    { "tests/crash/id-000094,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000095,sig-11,src-000000,op-havoc,rep-2" },
    { "tests/crash/id-000096,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000097,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000098,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000099,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000100,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000101,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000102,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000103,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000104,sig-11,src-000000,op-havoc,rep-2" },
    { "tests/crash/id-000105,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000106,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000107,sig-11,src-000000,op-havoc,rep-64" },
    { "tests/crash/id-000108,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000109,sig-11,src-000000,op-havoc,rep-64" },
    { "tests/crash/id-000110,sig-11,src-000000,op-havoc,rep-2" },
    { "tests/crash/id-000111,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000112,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000113,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000114,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000115,sig-11,src-000000,op-havoc,rep-64" },
    { "tests/crash/id-000116,sig-05,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000117,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000118,sig-11,src-000000,op-havoc,rep-64" },
    { "tests/crash/id-000119,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000120,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000121,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000122,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000123,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000124,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000125,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000126,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000127,sig-11,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000128,sig-05,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000129,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000130,sig-11,src-000000,op-havoc,rep-2" },
    { "tests/crash/id-000131,sig-05,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000132,sig-05,src-000000,op-havoc,rep-8" },
    { "tests/crash/id-000133,sig-11,src-000000,op-havoc,rep-2" },
    { "tests/crash/id-000134,sig-11,src-000000,op-havoc,rep-64" },
    { "tests/crash/id-000135,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000136,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000137,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000138,sig-11,src-000000,op-havoc,rep-64" },
    { "tests/crash/id-000139,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000140,sig-11,src-000000,op-havoc,rep-64" },
    { "tests/crash/id-000141,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000142,sig-05,src-000000,op-havoc,rep-2" },
    { "tests/crash/id-000143,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000144,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000145,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000146,sig-11,src-000000,op-havoc,rep-64" },
    { "tests/crash/id-000147,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000148,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000149,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000150,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000151,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000152,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000153,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000154,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000155,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000156,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000157,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000158,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000159,sig-11,src-000000,op-havoc,rep-2" },
    { "tests/crash/id-000160,sig-11,src-000000,op-havoc,rep-32" },
    { "tests/crash/id-000161,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000162,sig-11,src-000000,op-havoc,rep-16" },
    { "tests/crash/id-000163,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000164,sig-11,src-000000,op-havoc,rep-4" },
    { "tests/crash/id-000165,sig-11,src-000001,op-flip1,pos-139" },
    { "tests/crash/id-000166,sig-11,src-000001,op-flip1,pos-281" },
    { "tests/crash/id-000167,sig-11,src-000001,op-flip1,pos-300" },
    { "tests/crash/id-000168,sig-11,src-000001,op-flip1,pos-307" },
    { "tests/crash/id-000169,sig-11,src-000001,op-arith8,pos-277,val--34" },
    { "tests/crash/id-000170,sig-11,src-000001,op-arith8,pos-284,val-+10" },
    { "tests/crash/id-000171,sig-11,src-000001,op-arith8,pos-404,val-+19" },
    { "tests/crash/id-000172,sig-11,src-000001,op-arith8,pos-405,val-+31" },
    { "tests/crash/id-000173,sig-11,src-000001,op-havoc,rep-16" },
    { "tests/crash/id-000174,sig-11,src-000001,op-havoc,rep-16" },
    { "tests/crash/id-000175,sig-11,src-000001,op-havoc,rep-8" },
    { "tests/crash/id-000176,sig-11,src-000001,op-havoc,rep-8" },
    { "tests/crash/id-000177,sig-11,src-000001,op-havoc,rep-16" },
    { "tests/crash/id-000178,sig-11,src-000001,op-havoc,rep-16" },
    { "tests/crash/id-000179,sig-11,src-000001,op-havoc,rep-16" },
    { "tests/crash/id-000180,sig-11,src-000001,op-havoc,rep-16" },
    { "tests/crash/id-000181,sig-11,src-000001,op-havoc,rep-8" },
    { "tests/crash/id-000182,sig-11,src-000001,op-havoc,rep-32" },
    { "tests/crash/id-000183,sig-05,src-000001,op-havoc,rep-16" },
    { "tests/crash/id-000184,sig-05,src-000001,op-havoc,rep-4" },
    { "tests/crash/id-000185,sig-11,src-000001,op-havoc,rep-8" },
    { "tests/crash/id-000186,sig-11,src-000001,op-havoc,rep-32" },
    { "tests/crash/id-000187,sig-11,src-000001,op-havoc,rep-32" },
    { "tests/crash/id-000188,sig-11,src-000001,op-havoc,rep-16" },
    { "tests/crash/id-000189,sig-11,src-000001,op-havoc,rep-4" },
    { "tests/crash/id-000190,sig-11,src-000002,op-flip1,pos-46" },
    { "tests/crash/id-000191,sig-11,src-000002,op-flip1,pos-86" },
    { "tests/crash/id-000192,sig-11,src-000002,op-flip1,pos-142" },
    { "tests/crash/id-000193,sig-11,src-000002,op-arith8,pos-83,val--10" },
    { "tests/crash/id-000194,sig-11,src-000002,op-arith8,pos-125,val-+31" },
    { "tests/crash/id-000195,sig-11,src-000002,op-arith8,pos-314,val-+1" },
    { "tests/crash/id-000196,sig-11,src-000002,op-havoc,rep-4" },
    { "tests/crash/id-000197,sig-11,src-000002,op-havoc,rep-32" },
    { "tests/crash/id-000198,sig-11,src-000002,op-havoc,rep-32" },
    { "tests/crash/id-000199,sig-11,src-000002,op-havoc,rep-16" },
    { "tests/crash/id-000200,sig-11,src-000002,op-havoc,rep-4" },
    { "tests/crash/id-000201,sig-11,src-000002,op-havoc,rep-4" },
    { "tests/crash/id-000202,sig-11,src-000002,op-havoc,rep-64" },
    { "tests/crash/id-000203,sig-11,src-000002,op-havoc,rep-32" },
    { "tests/crash/id-000204,sig-11,src-000002,op-havoc,rep-4" },
    { "tests/crash/id-000205,sig-11,src-000002,op-havoc,rep-4" },
    { "tests/crash/id-000206,sig-11,src-000002,op-havoc,rep-8" },
    { "tests/crash/id-000207,sig-11,src-000002,op-havoc,rep-8" },
    { "tests/crash/id-000208,sig-05,src-000002,op-havoc,rep-8" },
    { "tests/crash/id-000209,sig-11,src-000002,op-havoc,rep-16" },
    { "tests/crash/id-000210,sig-11,src-000002,op-havoc,rep-8" },
    { "tests/crash/id-000211,sig-05,src-000002,op-havoc,rep-8" },
    { "tests/crash/id-000212,sig-11,src-000002,op-havoc,rep-8" },
    { "tests/crash/id-000213,sig-11,src-000002,op-havoc,rep-32" },
    { "tests/crash/id-000214,sig-11,src-000002,op-havoc,rep-4" },
    { "tests/crash/id-000215,sig-11,src-000002,op-havoc,rep-32" },
    { "tests/crash/id-000216,sig-11,src-000003,op-flip1,pos-590" },
    { "tests/crash/id-000217,sig-11,src-000003,op-flip1,pos-596" },
    { "tests/crash/id-000218,sig-11,src-000003,op-flip1,pos-707" },
    { "tests/crash/id-000219,sig-11,src-000003,op-flip2,pos-563" },
    { "tests/crash/id-000220,sig-11,src-000003,op-flip4,pos-589" },
    { "tests/crash/id-000221,sig-11,src-000003,op-int8,pos-767,val-+0" },
    { "tests/crash/id-000222,sig-11,src-000003,op-int8,pos-768,val-+0" },
};
#endif

static Fail_Test fail_tests[] = {
    //          stop after      test source                                             expected fail location {line, column}
    (Fail_Test){ CP_Lexing,     "tests/lexer_fail/crlf_test.hp",                        {4, 26} },
    (Fail_Test){ CP_Lexing,     "tests/lexer_fail/only_one_dquote.hp",                  {1, 1} },
    (Fail_Test){ CP_Lexing,     "tests/lexer_fail/non_ending_mlc.hp",                   {6, 5} },
    (Fail_Test){ CP_Parsing,    "tests/parser_fail/token_test.hp",                      {1, 1} },
    (Fail_Test){ CP_Parsing,    "tests/parser_fail/if_paren_test.hp",                   {8, 23} },
    (Fail_Test){ CP_Parsing,    "tests/parser_fail/extra_comma_in_params.hp",           {4, 31} },
    (Fail_Test){ CP_Checking,   "tests/sem_check_fail/infer_var_type_from_null.hp",     {4, 1} },
    (Fail_Test){ CP_Checking,   "tests/sem_check_fail/dup_func_param.hp",               {4, 39} },
    (Fail_Test){ CP_Checking,   "tests/sem_check_fail/dup_variable.hp",                 {7, 5} },
    (Fail_Test){ CP_Checking,   "tests/sem_check_fail/var_shadows_param.hp",            {6, 5} },
    (Fail_Test){ CP_Checking,   "tests/sem_check_fail/ambiguous_func_call.hp",          {8, 9} },
    (Fail_Test){ CP_Checking,   "tests/sem_check_fail/dup_func_def.hp",                 {5, 1} },
    (Fail_Test){ CP_Checking,   "tests/sem_check_fail/return_infer_fail.hp",            {11, 12} },
    (Fail_Test){ CP_Checking,   "tests/sem_check_fail/void_func_return.hp",             {6, 12} },
    (Fail_Test){ CP_Checking,   "tests/sem_check_fail/non_void_func_return.hp",         {6, 5} },
    (Fail_Test){ CP_Checking,   "tests/sem_check_fail/infer_ret_type_from_null.hp",     {4, 1} },
    (Fail_Test){ CP_Checking,   "tests/sem_check_fail/access_non_struct.hp",            {7, 10} },
    (Fail_Test){ CP_Checking,   "tests/sem_check_fail/deref_void_ptr.hp",               {7, 10} },
    (Fail_Test){ CP_Checking,   "tests/sem_check_fail/break_out_of_place.hp",           {6, 5} },
};

static Succeed_Test succeed_tests[] = {
    //              stop after      test source                     test function
    (Succeed_Test){ CP_Parsing,     "tests/expr_test.hp",           nullptr },
    (Succeed_Test){ CP_Parsing,     "tests/stmt_test.hp",           nullptr },
    (Succeed_Test){ CP_Parsing,     "tests/empty_main.hp",          nullptr },
    (Succeed_Test){ CP_Checking,    "tests/empty.hp",               nullptr },
    (Succeed_Test){ CP_Checking,    "tests/variable_scope.hp",      nullptr },
    (Succeed_Test){ CP_Checking,    "tests/struct_access.hp",       nullptr },
    (Succeed_Test){ CP_Checking,    "tests/recursive_rt_infer.hp",  RecursiveRtInfer_Test },
    (Succeed_Test){ CP_Checking,    "tests/difficult_rt_infer.hp",  nullptr },
};

static Execute_Test exec_tests[] = {
    //              test source                     expected output                     expected exit code
    (Execute_Test){ "tests/exec/hello.hp",          "tests/exec/hello.stdout",          0 },
    (Execute_Test){ "tests/exec/factorial.hp",      "tests/exec/factorial.stdout",      0 },
    (Execute_Test){ "tests/exec/fibo.hp",           "tests/exec/fibo.stdout",           0 },
    (Execute_Test){ "tests/exec/beer.hp",           "tests/exec/beer.stdout",           0 },
    (Execute_Test){ "tests/exec/and_or.hp",         nullptr,                            0 },
    (Execute_Test){ "tests/exec/bitshift.hp",       nullptr,                            0 },
    (Execute_Test){ "tests/exec/reg_pressure.hp",   "tests/exec/reg_pressure.stdout",   0 },
    (Execute_Test){ "tests/exec/reg_alloc.hp",      nullptr,                            0 },
    (Execute_Test){ "tests/exec/break.hp",          "tests/exec/break.stdout",          0 },
    (Execute_Test){ "tests/exec/break2.hp",         "tests/exec/break2.stdout",         0 },
    (Execute_Test){ "tests/exec/continue.hp",       "tests/exec/continue.stdout",       0 },
    (Execute_Test){ "tests/exec/continue2.hp",      "tests/exec/continue2.stdout",      0 },
    (Execute_Test){ "tests/exec/struct_as_arg.hp",  nullptr,                            10 },
    (Execute_Test){ "tests/exec/arg_passing.hp",    "tests/exec/arg_passing.stdout",    120 },
    (Execute_Test){ "tests/exec/module_test.hp",    nullptr,                            42 },
    (Execute_Test){ "tests/exec/modules_test.hp",   nullptr,                            210 },
    (Execute_Test){ "tests/exec/nbody.hp",          "tests/exec/nbody.stdout",          0 },
    (Execute_Test){ "tests/exec/nbody_p.hp",        "tests/exec/nbody.stdout",          0 },
    (Execute_Test){ "tests/exec/mandelbrot.hp",     "tests/exec/mandelbrot.stdout",     0 },
    (Execute_Test){ "tests/pointer_arith.hp",       nullptr,                            0 },
    (Execute_Test){ "tests/member_access.hp",       nullptr,                            0 },
    (Execute_Test){ "tests/function_var.hp",        nullptr,                            0 },
};

//static void PrintError(const char *filename, s64 line, s64 column, const char *message)
//{
//    fprintf(stderr, "%s:%" PRId64 ":%" PRId64 ": TEST ERROR: %s\n",
//            filename, line, column, message);
//}

static b32 CheckErrorLocation(Compiler_Context *compiler_ctx, Line_Col fail_location)
{
    File_Location error_loc = compiler_ctx->error_ctx.first_error_loc;
    if (error_loc.line == fail_location.line &&
        error_loc.column == fail_location.column)
    {
        return true;
    }
    return false;
}

b32 RunTest(const Crash_Test &test)
{
    fprintf(outfile, "Running crash test '%s'\n", test.source_filename); fflush(outfile);

    b32 failed = false;
    Compiler_Context compiler_ctx = NewCompilerContext();
    compiler_ctx.options.stop_after = CP_Checking;
    
    Open_File *file = OpenFile(&compiler_ctx, test.source_filename);
    if (file)
    {
        compiler_ctx.error_ctx.file = (IoFile*)nulldev;
        Compile(&compiler_ctx, file);
    }
    else
    {
        fprintf(outfile, "TEST ERROR: Could not open test file '%s'\n", test.source_filename);
        failed = true;
    }

    FreeCompilerContext(&compiler_ctx);

    if (failed)
    {
        fprintf(outfile, "Test '%s' failed\n", test.source_filename);
        fprintf(outfile, "----\n"); fflush(outfile);
    }
    return !failed;
}

b32 RunTest(const Fail_Test &test)
{
    //fprintf(outfile, "Running test '%s'\n", test.source_filename);

    b32 failed = false;
    Compiler_Context compiler_ctx = NewCompilerContext();
    compiler_ctx.options.stop_after = test.stop_after;

    Open_File *file = OpenFile(&compiler_ctx, test.source_filename);
    if (file)
    {
        compiler_ctx.error_ctx.file = (IoFile*)nulldev;
        Compile(&compiler_ctx, file);

        File_Location error_loc = compiler_ctx.error_ctx.first_error_loc;
        switch (test.stop_after)
        {
        case CP_Lexing:
            if (compiler_ctx.result != RES_FAIL_Lexing)
            {
                fprintf(outfile, "TEST ERROR: Was expecting lexing failure at %d:%d, but the lexing was successful\n",
                        test.fail_location.line, test.fail_location.column);
                failed = true;
            }
            else if (!CheckErrorLocation(&compiler_ctx, test.fail_location))
            {
                fprintf(outfile, "TEST ERROR: Was expecting lexing failure at %d:%d, but got error at %d:%d\n",
                        test.fail_location.line, test.fail_location.column, error_loc.line, error_loc.column);
                failed = true;
            } break;
        case CP_Parsing:
            if (compiler_ctx.result != RES_FAIL_Parsing)
            {
                fprintf(outfile, "TEST ERROR: Was expecting parsing failure at %d:%d, but the parsing was successful\n",
                        test.fail_location.line, test.fail_location.column);
                failed = true;
            }
            else if (!CheckErrorLocation(&compiler_ctx, test.fail_location))
            {
                fprintf(outfile, "TEST ERROR: Was expecting parsing failure at %d:%d, but got error at %d:%d\n",
                        test.fail_location.line, test.fail_location.column, error_loc.line, error_loc.column);
                failed = true;
            } break;
        case CP_Checking:
            if (compiler_ctx.result != RES_FAIL_SemanticCheck)
            {
                fprintf(outfile, "TEST ERROR: Was expecting semantic check failure at %d:%d, but the semantic check was successful\n",
                        test.fail_location.line, test.fail_location.column);
                failed = true;
            }
            else if (!CheckErrorLocation(&compiler_ctx, test.fail_location))
            {
                fprintf(outfile, "TEST ERROR: Was expecting semantic check failure at %d:%d, but got error at %d:%d\n",
                        test.fail_location.line, test.fail_location.column, error_loc.line, error_loc.column);
                failed = true;
            } break;
        case CP_IrGen:
        case CP_CodeGen:
        case CP_Assembling:
        case CP_Linking:
            fprintf(outfile, "INVALID TEST: Failing tests should only be tested for the lexing--semantic check part of the compiler");
            INVALID_CODE_PATH;
            break;
        }
    }
    else
    {
        fprintf(outfile, "TEST ERROR: Could not open test file '%s'\n", test.source_filename);
        failed = true;
    }

    FreeCompilerContext(&compiler_ctx);

    if (failed)
    {
        fprintf(outfile, "Test '%s' failed\n", test.source_filename);
        fprintf(outfile, "----\n"); fflush(outfile);
    }
    return !failed;
}

b32 RunTest(const Succeed_Test &test)
{
    b32 failed = false;
    Compiler_Context compiler_ctx = NewCompilerContext();
    compiler_ctx.options.stop_after = test.stop_after;

    Open_File *file = OpenFile(&compiler_ctx, test.source_filename);
    if (file)
    {
        compiler_ctx.error_ctx.file = (IoFile*)outfile;
        Compile(&compiler_ctx, file);

        if (compiler_ctx.result != RES_OK)
        {
            fprintf(outfile, "TEST ERROR: Unexpected errors\n");
            failed = true;
        }
        else if (test.test_func)
        {
            Test_Context test_ctx = { };
            test.test_func(&test_ctx, compiler_ctx);
            failed = (test_ctx.errors > 0);
        }
    }
    else
    {
        fprintf(outfile, "TEST ERROR: Could not open test file '%s'\n", test.source_filename);
        failed = true;
    }

    FreeCompilerContext(&compiler_ctx);

    if (failed)
    {
        fprintf(outfile, "Test '%s' failed\n", test.source_filename);
        fprintf(outfile, "----\n"); fflush(outfile);
    }
    return !failed;
}

#if defined(HP_WIN)
const char *test_exe = "out.exe";
#include <windows.h>

#else
const char *test_exe = "./out";
#endif

b32 RunTest(const Execute_Test &test)
{
    b32 failed = false;
    Compiler_Context compiler_ctx = NewCompilerContext();

    Open_File *file = OpenFile(&compiler_ctx, test.source_filename);
    if (file)
    {
        compiler_ctx.error_ctx.file = (IoFile*)outfile;
        compiler_ctx.options.output_filename = test_exe;
        //compiler_ctx.options.profile_instr_count = true;

        Compile(&compiler_ctx, file);

        if (compiler_ctx.result != RES_OK)
        {
            fprintf(outfile, "TEST ERROR: Unexpected errors\n");
            failed = true;
        }
        else
        {
            fflush(nullptr);
            FILE *test_output = popen(test_exe, "r");
            if (!test_output)
            {
                fprintf(outfile, "TEST ERROR: Error executing the test '%s'\n", test.source_filename);
                failed = true;
            }
            else if (ferror(test_output))
            {
                fprintf(outfile, "TEST ERROR: pipe in erronous state\n");
                failed = true;
            }
            else
            {
                if (test.expected_output_filename)
                {
                    FILE *expected_output = fopen(test.expected_output_filename, "r");
                    if (expected_output)
                    {
                        Line_Col loc = { };
                        loc.line = 1;
                        loc.column = 1;
                        const s64 buf_size = 256;
                        char test_buf[buf_size];
                        char expected_buf[buf_size];
                        //while (!failed)
                        while (true)
                        {
                            s64 test_size = fread(&test_buf, 1, buf_size, test_output);
                            s64 expected_size = fread(&expected_buf, 1, buf_size, expected_output);
                            if (!failed)
                            {
                                for (s64 i = 0; i < test_size && i < expected_size; i++)
                                {
                                    if (test_buf[i] != expected_buf[i])
                                    {
                                        fprintf(outfile, "TEST ERROR: Test output mismatch\n");
                                        fprintf(outfile, "%s:%d:%d: (test output) '%c' != '%c' ; %d != %d (expected)\n",
                                                test.expected_output_filename, loc.line, loc.column,
                                                test_buf[i], expected_buf[i], test_buf[i], expected_buf[i]);
                                        failed = true;
                                        break;
                                    }
                                    if (test_buf[i] == '\n')
                                    {
                                        loc.line++;
                                        loc.column = 0;
                                    }
                                    else
                                    {
                                        loc.column++;
                                    }
                                }
                                if (!failed && test_size != expected_size)
                                {
                                    fprintf(outfile, "TEST ERROR: Test output length mismatch\n");
                                    fprintf(outfile, "%s:%d:%d: (test output) %" PRId64 " != %" PRId64 " (expected)\n",
                                            test.expected_output_filename, loc.line, loc.column,
                                            test_size, expected_size);
                                    failed = true;
                                }
                            }
                            if (test_size == 0)
                                break;
                        }
                        fclose(expected_output);
                    }
                    else
                    {
                        fprintf(outfile, "TEST ERROR: Could not open file for expected output ('%s')\n\tfor test '%s'\n",
                                test.expected_output_filename, test.source_filename);
                        failed = true;
                    }
                }
                fflush(nullptr);
                int result = pclose(test_output);
#ifdef HP_WIN
                int exit_code = result;
#else
                if (WIFEXITED(result))
                {
                    int exit_code = WEXITSTATUS(result);
#endif
                    if (exit_code != test.expected_exit_code)
                    {
                        fprintf(outfile, "TEST ERROR: Executed test's exit code was %d(%x) and not %d(%x)\n\tfor test '%s'\n",
                                result, result,
                                test.expected_exit_code, test.expected_exit_code,
                                test.source_filename);
                        failed = true;
                    }
#ifndef HP_WIN
                }
                else if (WIFSIGNALED(result))
                {
                    int child_signal = WTERMSIG(result);
                    fprintf(outfile, "\tThe child received signal %d\n", child_signal);
                    failed = true;
                }
#endif
            }
        }
    }
    else
    {
        fprintf(outfile, "TEST ERROR: Could not open test source file '%s'\n", test.source_filename);
        failed = true;
    }

    FreeCompilerContext(&compiler_ctx);

    if (failed)
    {
        fprintf(outfile, "Test '%s' failed\n", test.source_filename);
        fprintf(outfile, "----\n"); fflush(outfile);
    }
    return !failed;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    outfile = stdout;

    nulldev = fopen("/dev/null", "w");
    if (!nulldev)
        nulldev = fopen("nul", "w");
    if (!nulldev)
    {
        fprintf(outfile, "Could not open null device, exiting...\n");
        return -1;
    }

    fprintf(outfile, "----\n");

    s64 total_tests = 0;
    total_tests += array_length(fail_tests);
    total_tests += array_length(succeed_tests);
    total_tests += array_length(exec_tests);

    s64 failed_tests = 0;
#ifndef NO_CRASH_TESTS
    for (const Crash_Test &test : crash_tests)
    {
        failed_tests += RunTest(test) ? 0 : 1;
    }
#endif
    for (const Fail_Test &test : fail_tests)
    {
        failed_tests += RunTest(test) ? 0 : 1;
    }
    for (const Succeed_Test &test : succeed_tests)
    {
        failed_tests += RunTest(test) ? 0 : 1;
    }
    for (const Execute_Test &test : exec_tests)
    {
        failed_tests += RunTest(test) ? 0 : 1;
    }

    fprintf(outfile, "----\n");
    fprintf(outfile, "%" PRId64 " tests run, %" PRId64 " failed\n", total_tests, failed_tests);
    fprintf(outfile, "----\n");

    fclose(nulldev);

    return 0; //failed_tests;
}

