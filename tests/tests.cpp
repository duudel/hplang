
#include "../src/hplang.h"
#include "../src/common.h"
#include "../src/compiler.h"
#include "../src/token.h"
#include "../src/ast_types.h"

#include <cstdio>
#include <cinttypes>
#include <cstdlib> // for system()


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

static Fail_Test fail_tests[] = {
    //          stop after      test source                                             expected fail location {line, column}
    (Fail_Test){ CP_Lexing,     "tests/lexer_fail/crlf_test.hp",                        {4, 26} },
    (Fail_Test){ CP_Lexing,     "tests/lexer_fail/only_one_dquote.hp",                  {1, 1} },
    (Fail_Test){ CP_Lexing,     "tests/lexer_fail/non_ending_mlc.hp",                   {6, 5} },
    (Fail_Test){ CP_Parsing,    "tests/parser_fail/token_test.hp",                      {1, 1} },
    (Fail_Test){ CP_Parsing,    "tests/parser_fail/if_paren_test.hp",                   {8, 23} },
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
    (Execute_Test){ "tests/exec/reg_pressure.hp",   "tests/exec/reg_pressure.stdout",   0 },
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

b32 RunTest(const Fail_Test &test)
{
    fprintf(outfile, "Running test '%s'\n", test.source_filename);

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
                fprintf(outfile, "TEST ERROR: Was expecting lexing failure at %d:%d\n",
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
                fprintf(outfile, "TEST ERROR: Was expecting parsing failure at %d:%d\n",
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
                fprintf(outfile, "TEST ERROR: Was expecting semantic check failure at %d:%d\n",
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

    fprintf(outfile, "----\n"); fflush(outfile);
    return !failed;
}

b32 RunTest(const Succeed_Test &test)
{
    fprintf(outfile, "Running test '%s'\n", test.source_filename);

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

    fprintf(outfile, "----\n"); fflush(outfile);
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
    fprintf(outfile, "Running test '%s'\n", test.source_filename);
    fflush(outfile);

    b32 failed = false;
    Compiler_Context compiler_ctx = NewCompilerContext();

    Open_File *file = OpenFile(&compiler_ctx, test.source_filename);
    if (file)
    {
        compiler_ctx.error_ctx.file = (IoFile*)outfile;
        compiler_ctx.options.output_filename = test_exe;

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

    fprintf(outfile, "----\n"); fflush(outfile);
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
    return 0; //failed_tests;
}

