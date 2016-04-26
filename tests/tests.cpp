
#include "../src/hplang.h"
#include "../src/common.h"
#include "../src/compiler.h"
#include "../src/token.h"
#include "../src/ast_types.h"

#include <cstdio>
#include <cinttypes>

FILE *nulldev;

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

struct Line_Col
{
    s64 line, column;
};

struct Test
{
    const char *filename;
    Line_Col fail_lexing;  // if line, column != 0, should fail lexing at the location
    Line_Col fail_parsing; // if line, column != 0, should fail parsing at the location
};

Test tests[] = {
    (Test){ "tests/lexer_fail/crlf_test.hp", {4, 26}, { } },
    (Test){ "tests/parser_fail/token_test.hp", { }, {1, 1} },
    (Test){ "tests/hello_test.hp", { }, { } },
    (Test){ "tests/expr_test.hp", { }, { } },
    (Test){ "tests/stmt_test.hp", { }, { } },
    (Test){ "tests/beer_test.hp", { }, { } },
    (Test){ "tests/if_paren_test.hp", { }, {8, 23} },
    (Test){ "tests/module_test.hp", { }, { } },
};

b32 CheckLexingResult(Compiler_Context *compiler_ctx,
        const Test &test, b32 compiler_result)
{
    b32 should_fail_lexing =
        (test.fail_lexing.line && test.fail_lexing.column);

    if (should_fail_lexing)
    {
        if (compiler_ctx->result == RES_FAIL_Lexing)
        {
            File_Location error_loc = compiler_ctx->error_ctx.first_error_loc;
            if (error_loc.line == test.fail_lexing.line &&
                error_loc.column == test.fail_lexing.column)
            {
                return true;
            }
            fprintf(stderr, "%s:%lld:%lld: Test result: Unexpected lexing error\n",
                    test.filename,
                    error_loc.line,
                    error_loc.column);
        }
        fprintf(stderr, "%s:%lld:%lld: Test result: Expecting lexing error\n",
                test.filename,
                test.fail_lexing.line,
                test.fail_lexing.column);
        return false;
    }
    else // compilation failed, was not expecting lexing failure
    {
        if (compiler_ctx->result == RES_FAIL_Lexing)
        {
            File_Location error_loc = compiler_ctx->error_ctx.first_error_loc;
            fprintf(stderr, "%s:%lld:%lld: Test result: Unexpected lexing error\n",
                    test.filename,
                    error_loc.line,
                    error_loc.column);
            return false;
        }
    }
    return true;
}

b32 CheckParsingResult(Compiler_Context *compiler_ctx,
        const Test &test, b32 compiler_result)
{
    b32 should_fail_parsing =
        (test.fail_parsing.line && test.fail_parsing.column);

    if (should_fail_parsing)
    {
        if (compiler_ctx->result == RES_FAIL_Parsing)
        {
            File_Location error_loc = compiler_ctx->error_ctx.first_error_loc;
            if (error_loc.line == test.fail_parsing.line &&
                error_loc.column == test.fail_parsing.column)
            {
                return true;
            }
            fprintf(stderr, "%s:%lld:%lld: Test result: Unexpected parsing error\n",
                    test.filename,
                    error_loc.line,
                    error_loc.column);
        }
        fprintf(stderr, "%s:%lld:%lld: Test result: Expecting parsing error\n",
                test.filename,
                test.fail_parsing.line,
                test.fail_parsing.column);
        return false;
    }
    else // compilation failed, was not expecting lexing failure
    {
        if (compiler_ctx->result == RES_FAIL_Parsing)
        {
            File_Location error_loc = compiler_ctx->error_ctx.first_error_loc;
            fprintf(stderr, "%s:%lld:%lld: Test result: Unexpected parsing error\n",
                    test.filename,
                    error_loc.line,
                    error_loc.column);
            return false;
        }
    }
    return true;
}

s64 RunTest(Test_Context *ctx, const Test &test)
{
    fprintf(stderr, "Running test '%s'\n", test.filename);

    s64 failed = 0;
    Compiler_Context compiler_ctx = NewCompilerContext();

    Open_File *file = OpenFile(&compiler_ctx, test.filename);
    if (file)
    {
        b32 should_fail_lexing =
            (test.fail_lexing.line && test.fail_lexing.column);
        b32 should_fail_parsing =
            (test.fail_parsing.line && test.fail_parsing.column);

        if (!should_fail_lexing && !should_fail_parsing)
            compiler_ctx.error_ctx.file = stderr;
        else
            compiler_ctx.error_ctx.file = nulldev;

        b32 result = Compile(&compiler_ctx, file);

        if (!CheckLexingResult(&compiler_ctx, test, result))
        {
            failed = 1;
        }
        else if (!CheckParsingResult(&compiler_ctx, test, result))
        {
            failed = 1;
        }
    }
    else
    {
        fprintf(stderr, "Error: Could not open test file '%s'\n", test.filename);
        failed = 1;
    }

    FreeCompilerContext(&compiler_ctx);

    fprintf(stderr, "----\n"); fflush(stderr);
    return failed;
}

int main(int argc, char **argv)
{
    nulldev = fopen("/dev/null", "w");
    if (!nulldev)
        nulldev = fopen("nul", "w");
    if (!nulldev)
    {
        fprintf(stderr, "Could not open null device, exiting...\n");
        return -1;
    }

    fprintf(stderr, "----\n");

    Test_Context test_ctx = { };

    s64 failed_tests = 0;
    s64 total_tests = array_length(tests);
    for (const Test &test : tests)
    {
        failed_tests += RunTest(&test_ctx, test);
        fflush(stderr);
    }

    fprintf(stderr, "----\n");
    fprintf(stderr, "%" PRId64 " tests run, %" PRId64 " failed\n", total_tests, failed_tests);
    fprintf(stderr, "----\n");
    return failed_tests;
}

