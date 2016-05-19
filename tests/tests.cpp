
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
            TEST(TypeIsVoid(main_sym->type->function_type.return_type));
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
    s64 line, column;
};

typedef void (*Test_Function)(Test_Context *test_ctx, Compiler_Context comp_ctx);

struct Test
{
    Compilation_Phase stop_after;
    const char *filename;
    Line_Col fail_lexing;       // if line, column != 0, should fail lexing at the location
    Line_Col fail_parsing;      // if line, column != 0, should fail parsing at the location
    Line_Col fail_sem_check;    // if line, column != 0, should fail checking at the location
    Test_Function test_func;
};

Test tests[] = {
    (Test){ CP_Lexing,  "tests/lexer_fail/crlf_test.hp",        {4, 26}, {}, {}, nullptr },
    (Test){ CP_Lexing,  "tests/lexer_fail/only_one_dquote.hp",  {1, 1}, {}, {}, nullptr },
    (Test){ CP_Lexing,  "tests/lexer_fail/non_ending_mlc.hp",   {6, 5}, {}, {}, nullptr },
    (Test){ CP_Parsing, "tests/parser_fail/token_test.hp",      {}, {1, 1}, {}, nullptr },
    (Test){ CP_Parsing, "tests/parser_fail/if_paren_test.hp",   {}, {8, 23}, {}, nullptr },
    (Test){ CP_Parsing, "tests/expr_test.hp", {}, {}, {}, nullptr },
    (Test){ CP_Parsing, "tests/stmt_test.hp", {}, {}, {}, nullptr },
    (Test){ CP_Checking, "tests/sem_check_fail/infer_var_type_from_null.hp",    {}, {}, {4, 1}, nullptr },
    (Test){ CP_Checking, "tests/sem_check_fail/dup_func_param.hp",              {}, {}, {4, 39}, nullptr },
    (Test){ CP_Checking, "tests/sem_check_fail/dup_variable.hp",                {}, {}, {7, 5}, nullptr },
    (Test){ CP_Checking, "tests/sem_check_fail/var_shadows_param.hp",           {}, {}, {6, 5}, nullptr },
    (Test){ CP_Checking, "tests/sem_check_fail/ambiguous_func_call.hp",         {}, {}, {8, 9}, nullptr },
    (Test){ CP_Checking, "tests/sem_check_fail/dup_func_def.hp",                {}, {}, {5, 1}, nullptr },
    (Test){ CP_Checking, "tests/sem_check_fail/return_infer_fail.hp",           {}, {}, {11, 12}, nullptr },
    (Test){ CP_Checking, "tests/sem_check_fail/void_func_return.hp",            {}, {}, {6, 12}, nullptr },
    (Test){ CP_Checking, "tests/sem_check_fail/non_void_func_return.hp",        {}, {}, {6, 5}, nullptr },
    (Test){ CP_Checking, "tests/sem_check_fail/infer_ret_type_from_null.hp",    {}, {}, {4, 1}, nullptr },
    (Test){ CP_Checking, "tests/sem_check_fail/access_non_struct.hp",           {}, {}, {7, 10}, nullptr },
    (Test){ CP_Checking, "tests/sem_check_fail/deref_void_ptr.hp",              {}, {}, {7, 10}, nullptr },
    (Test){ CP_Checking, "tests/empty.hp",              {}, {}, {}, nullptr },
    (Test){ CP_Checking, "tests/variable_scope.hp",     {}, {}, {}, nullptr },
    (Test){ CP_Checking, "tests/struct_access.hp",      {}, {}, {}, nullptr },
    (Test){ CP_CodeGen, "tests/hello_test.hp",          {}, {}, {}, nullptr },
    (Test){ CP_CodeGen, "tests/beer_test.hp",           {}, {}, {}, Beer_Test },
    (Test){ CP_CodeGen, "tests/pointer_arith.hp",       {}, {}, {}, nullptr },
    (Test){ CP_CodeGen, "tests/member_access.hp",       {}, {}, {}, nullptr },
    (Test){ CP_CodeGen, "tests/module_test.hp",         {}, {}, {}, nullptr },
    (Test){ CP_CodeGen, "tests/modules_test.hp",        {}, {}, {}, nullptr },
    (Test){ CP_CodeGen, "tests/recursive_rt_infer.hp",  {}, {}, {}, RecursiveRtInfer_Test },
    (Test){ CP_CodeGen, "tests/difficult_rt_infer.hp",  {}, {}, {}, nullptr },
    (Test){ CP_CodeGen, "tests/function_var.hp",        {}, {}, {}, nullptr },
    (Test){ CP_CodeGen, "tests/struct_as_arg.hp",       {}, {}, {}, nullptr },
    (Test){ CP_CodeGen, "tests/arg_passing.hp",         {}, {}, {}, nullptr },
};

void PrintError(const char *filename, s64 line, s64 column, const char *message)
{
    fprintf(stderr, "%s:%" PRId64 ":%" PRId64 ": TEST ERROR: %s\n",
            filename, line, column, message);
}

b32 CheckPhaseResult(Compiler_Context *compiler_ctx,
        const char *filename, Line_Col fail_line_col,
        Compilation_Result expected_result,
        const char *unexpected_error, const char *expecting_error)
{
    b32 should_fail = (fail_line_col.line && fail_line_col.column);
    if (should_fail)
    {
        if (compiler_ctx->result == expected_result)
        {
            File_Location error_loc = compiler_ctx->error_ctx.first_error_loc;
            if (error_loc.line == fail_line_col.line &&
                error_loc.column == fail_line_col.column)
            {
                return true;
            }
            PrintError(filename, error_loc.line, error_loc.column, unexpected_error);
        }
        PrintError(filename, fail_line_col.line, fail_line_col.column, expecting_error);
        return false;
    }
    else if (compiler_ctx->result == expected_result)
    {
        File_Location error_loc = compiler_ctx->error_ctx.first_error_loc;
        PrintError(filename, error_loc.line, error_loc.column, unexpected_error);
        return false;
    }
    return true;
}

b32 CheckLexingResult(Compiler_Context *compiler_ctx,
        const Test &test)
{
    return CheckPhaseResult(compiler_ctx, test.filename,
            test.fail_lexing, RES_FAIL_Lexing,
            "Unexpected lexer error", "Expected lexer error");
}

b32 CheckParsingResult(Compiler_Context *compiler_ctx,
        const Test &test)
{
    return CheckPhaseResult(compiler_ctx, test.filename,
            test.fail_parsing, RES_FAIL_Parsing,
            "Unexpected parser error", "Expected parser error");
}

b32 CheckSemCheckResult(Compiler_Context *compiler_ctx,
        const Test &test)
{
    return CheckPhaseResult(compiler_ctx, test.filename,
            test.fail_sem_check, RES_FAIL_SemanticCheck,
            "Unexpected semantic check error", "Expected semantic check error");
}

s64 RunTest(Test_Context *test_ctx, const Test &test)
{
    fprintf(stderr, "Running test '%s'\n", test.filename);

    s64 failed = 0;
    Compiler_Context compiler_ctx = NewCompilerContext();
    compiler_ctx.options.stop_after = test.stop_after;

    Open_File *file = OpenFile(&compiler_ctx, test.filename);
    if (file)
    {
        b32 should_fail_lexing =
            (test.fail_lexing.line && test.fail_lexing.column);
        b32 should_fail_parsing =
            (test.fail_parsing.line && test.fail_parsing.column);
        b32 should_fail_sem_check =
            (test.fail_sem_check.line && test.fail_sem_check.column);
        b32 should_fail = should_fail_lexing || should_fail_parsing || should_fail_sem_check;

        if (should_fail)
            compiler_ctx.error_ctx.file = (IoFile*)nulldev;
        else
            compiler_ctx.error_ctx.file = (IoFile*)stderr;

        Compile(&compiler_ctx, file);

        if (!CheckLexingResult(&compiler_ctx, test))
            failed = 1;
        else if (!CheckParsingResult(&compiler_ctx, test))
            failed = 1;
        else if (!CheckSemCheckResult(&compiler_ctx, test))
            failed = 1;

        if (test.test_func)
        {
            test.test_func(test_ctx, compiler_ctx);
        }
    }
    else
    {
        fprintf(stderr, "Error: Could not open test file '%s'\n", test.filename);
        failed = 1;
    }

    FreeCompilerContext(&compiler_ctx);

    fprintf(stderr, "----\n"); fflush(stderr);

    if (test_ctx->errors > 0) failed = 1;
    return failed;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

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
    s64 total_tests = array_length(tests);
    for (const Test &test : tests)
    {
        Test_Context test_ctx = { };
        failed_tests += RunTest(&test_ctx, test);
        fflush(stderr);
    }

    fprintf(stderr, "----\n");
    fprintf(stderr, "%" PRId64 " tests run, %" PRId64 " failed\n", total_tests, failed_tests);
    fprintf(stderr, "----\n");
    return failed_tests;
}

