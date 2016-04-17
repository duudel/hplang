
#include "../src/hplang.h"
#include "../src/compiler.h"

#include <cstdio>

FILE *nulldev;

using namespace hplang;

struct Line_Col
{
    s64 line, column;
};

struct Test
{
    const char *filename;
    Line_Col fail_lexing;  // if line, column != 0 should fail lexing at the location
    Line_Col fail_parsing; // if line, column != 0 should fail parsing at the location
};

Test tests[] = {
    (Test){ "tests/crlf_test.hp", {4, 26}, { } },
    (Test){ "tests/token_test.hp", { }, {1, 1} },
    (Test){ "tests/hello_test.hp", { }, { } },
    (Test){ "tests/expr_test.hp", { }, { } },
    (Test){ "tests/stmt_test.hp", { }, { } },
};

b32 CheckLexingResult(Compiler_Context *compiler_ctx,
        const Test &test, b32 compiler_result)
{
    b32 should_fail_lexing =
        (test.fail_lexing.line && test.fail_lexing.column);

    if (compiler_result)
    {
        if (!should_fail_lexing)
        {
            return true;
        }
    }
    else if (should_fail_lexing)
    {
        if (compiler_ctx->error_ctx.compilation_phase == COMP_Lexing)
        {
            File_Location error_loc = compiler_ctx->error_ctx.first_error_loc;
            if (error_loc.line == test.fail_lexing.line &&
                error_loc.column == test.fail_lexing.column)
            {
                return true;
            }
            fprintf(stderr, "%s:%d:%d: Test result: Unexpected lexing error\n",
                    test.filename,
                    error_loc.line,
                    error_loc.column);
        }
        fprintf(stderr, "%s:%d:%d: Test result: Expecting lexing error\n",
                test.filename,
                test.fail_lexing.line,
                test.fail_lexing.column);
        return false;
    }
    else // compilation failed, was not expecting lexing failure
    {
        if (compiler_ctx->error_ctx.compilation_phase == COMP_Lexing)
        {
            File_Location error_loc = compiler_ctx->error_ctx.first_error_loc;
            fprintf(stderr, "%s:%d:%d: Test result: Unexpected lexing error\n",
                    test.filename,
                    error_loc.line,
                    error_loc.column);
            return false;
        }
        return true;
    }
}

b32 CheckParsingResult(Compiler_Context *compiler_ctx,
        const Test &test, b32 compiler_result)
{
    b32 should_fail_parsing =
        (test.fail_parsing.line && test.fail_parsing.column);

    if (compiler_result)
    {
        if (!should_fail_parsing)
        {
            return true;
        }
    }
    else if (should_fail_parsing)
    {
        if (compiler_ctx->error_ctx.compilation_phase == COMP_Parsing)
        {
            File_Location error_loc = compiler_ctx->error_ctx.first_error_loc;
            if (error_loc.line == test.fail_parsing.line &&
                error_loc.column == test.fail_parsing.column)
            {
                return true;
            }
            fprintf(stderr, "%s:%d:%d: Test result: Unexpected lexing error\n",
                    test.filename,
                    error_loc.line,
                    error_loc.column);
        }
        fprintf(stderr, "%s:%d:%d: Test result: Expecting lexing error\n",
                test.filename,
                test.fail_parsing.line,
                test.fail_parsing.column);
        return false;
    }
    else // compilation failed, was not expecting lexing failure
    {
        if (compiler_ctx->error_ctx.compilation_phase == COMP_Parsing)
        {
            File_Location error_loc = compiler_ctx->error_ctx.first_error_loc;
            fprintf(stderr, "%s:%d:%d: Test result: Unexpected parsing error\n",
                    test.filename,
                    error_loc.line,
                    error_loc.column);
            return false;
        }
        return true;
    }
}

s64 RunTest(const Test &test)
{
    fprintf(stderr, "Running test '%s'\n", test.filename);
    fprintf(stderr, "----\n"); fflush(stderr);

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
    return failed;
}

template <class S, s64 N>
s64 arr_len(S (&)[N])
{ return N; }

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

    s64 failed_tests = 0;
    s64 total_tests = arr_len(tests);
    for (const Test &test : tests)
    {
        failed_tests += RunTest(test);
        fflush(stderr);
    }

    fprintf(stderr, "----\n");
    fprintf(stderr, "%d tests run, %d failed\n", total_tests, failed_tests);
    fprintf(stderr, "----\n");
    return failed_tests;
}

