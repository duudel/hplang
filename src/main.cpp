
#include "hplang.h"
#include "lexer.h"
#include "compiler.h"
#include "error.h"

#include "ir_types.h"
#include "codegen.h"

#include <cstdio>
#include <cstring>

#include "args_util.h"

using namespace hplang;


void PrintUsage(Arg_Options_Context *options_ctx)
{
    printf("hplang [options] <source>\n");
    printf("  compile <source> into binary executable\n\n");
    printf("options:\n");
    PrintOptions(options_ctx);
}

void PrintHelp(Arg_Options_Context *options_ctx)
{
    PrintUsage(options_ctx);
}

void PrintVersion()
{
    printf("%s\n", GetVersionString());
    printf("Copyright (c) 2016 Henrik Paananen\n");
}

static const char *target_args[] = {
    "win64",
    "win_amd64",
    "elf64",
    "linux64",
    nullptr
};

static const char *diag_args[] = {
    "memory",
    "ast",
    "ir",
    "regalloc",
    nullptr
};

static const char *profile_args[] = {
    "instrcount",
    nullptr
};

static const Arg_Option options[] = {
    {"output", 'o', nullptr, nullptr, "Sets the output filename", "filename", nullptr},
    {"target", 'T', nullptr, nullptr, "Sets the output target", "target", target_args},
    {"diagnostic", 'd', diag_args, "MAiR", "Selects the diagnostic options", nullptr, nullptr},
    {"profile", 'p', profile_args, "i", "Selects profiling options", nullptr, nullptr},
    {"help", 'h', nullptr, nullptr, "Shows this help and exits", nullptr, nullptr},
    {"version", 'v', nullptr, nullptr, "Prints the version information", nullptr, nullptr},
    { }
};

static int ParseTargetOption(Arg_Option_Result option_result, Compiler_Options *options)
{
    const char *arg = option_result.arg;
    if (!arg)
    {
        printf("No <target> given for -T <target>, aborting...\n");
        return -1;
    }
    if (strcmp(arg, "win64") == 0 ||
        strcmp(arg, "win_amd64") == 0)
    {
        options->target = CGT_AMD64_Windows;
    }
    else if (strcmp(arg, "elf64") == 0 ||
                strcmp(arg, "linux64") == 0)
    {
        options->target = CGT_AMD64_Unix;
    }
    else
    {
        printf("Invalid target \"%s\"\n, aborting...", arg);
        return -1;
    }
    return 0;
}

static int ParseDiagnosticOption(Arg_Option_Result option_result, Compiler_Options *options)
{
    if (option_result.short_args)
    {
        const char *p = option_result.short_args;
        for (; p[0] != '\0'; p++)
        {
            switch (p[0])
            {
                case 'M':
                    options->diagnose_memory = true;
                    break;
                case 'A':
                    options->debug_ast = true;
                    break;
                case 'i':
                    options->debug_ir = true;
                    break;
                case 'R':
                    options->debug_reg_alloc = true;
                    break;

                default:
                    printf("Unrecognized argument %c for -%c\n",
                            p[0], option_result.option->short_name);
                    return -1;
            }
        }
    }
    else if (option_result.arg)
    {
        const char *arg = option_result.arg;
        if (strcmp(arg, "memory") == 0)
            options->diagnose_memory = true;
        else if (strcmp(arg, "ast") == 0)
            options->debug_ast = true;
        else if (strcmp(arg, "ir") == 0)
            options->debug_ir = true;
        else if (strcmp(arg, "regalloc") == 0)
            options->debug_reg_alloc = true;
        else
        {
            printf("Unrecognized argument %s for --%s\n",
                    arg, option_result.option->long_name);
            return -1;
        }
    }
    return 0;
}

static int ParseProfilingOption(Arg_Option_Result option_result, Compiler_Options *options)
{
    if (option_result.short_args)
    {
        const char *p = option_result.short_args;
        for (; p[0] != '\0'; p++)
        {
            switch (p[0])
            {
                case 'i':
                    options->profile_instr_count = true;
                    break;

                default:
                    printf("Unrecognized argument %c for -%c\n",
                            p[0], option_result.option->short_name);
                    return -1;
            }
        }
    }
    else if (option_result.arg)
    {
        const char *arg = option_result.arg;
        if (strcmp(arg, "instrcount") == 0)
            options->profile_instr_count = true;
        else
        {
            printf("Unrecognized argument %s for --%s\n",
                    arg, option_result.option->long_name);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
#if 0
    printf("sizeof(File_Location) %lld\n", sizeof(File_Location));
    printf("sizeof(Name) %lld\n", sizeof(Name));
    printf("sizeof(Ast_Node) %lld\n", sizeof(Ast_Node));
    printf("sizeof(Ast_Expr) %lld\n", sizeof(Ast_Expr));
    printf("sizeof(Ast_Function_Def) %lld\n", sizeof(Ast_Function_Def));
    printf("sizeof(Ir_Operand) %lld\n", sizeof(Ir_Operand));
    printf("sizeof(Ir_Instruction) %lld\n", sizeof(Ir_Instruction));
    printf("sizeof(Operand) %lld\n", sizeof(Operand));
    printf("sizeof(Instruction) %lld\n", sizeof(Instruction));
    printf("sizeof(Oper_Data_Type) %lld\n", sizeof(Oper_Data_Type));
    printf("sizeof(Oper_Access_Flags) %lld\n", sizeof(Oper_Access_Flags));
    fflush(stdout);
    return 0;
#endif

    Arg_Options_Context options_ctx = NewArgOptionsCtx(options, argc, argv);
    Arg_Option_Result option_result = { };

    Compiler_Options options = DefaultCompilerOptions();
    const char *source = nullptr;

    while (GetNextOption(&options_ctx, &option_result))
    {
        if (option_result.unrecognized)
        {
            printf("Unrecognized option '%s', aborting...\n", option_result.unrecognized);
            return -1;
        }
        if (option_result.option)
        {
            switch (option_result.option->short_name)
            {
                case 'o':
                {
                    options.output_filename = option_result.arg;
                } break;
                case 'T':
                {
                    int result = ParseTargetOption(option_result, &options);
                    if (result != 0) return result;
                } break;
                case 'd':
                {
                    int result = ParseDiagnosticOption(option_result, &options);
                    if (result != 0) return result;
                } break;
                case 'p':
                {
                    int result = ParseProfilingOption(option_result, &options);
                    if (result != 0) return result;
                } break;

                case 'h':
                {
                    PrintHelp(&options_ctx);
                    return 0;
                }
                case 'v':
                {
                    PrintVersion();
                    return 0;
                }
            }
        }
        else
        {
            source = option_result.arg;
        }
    }

    if (!source)
    {
        printf("No source file specified\n\n");
        PrintUsage(&options_ctx);
        return 0;
    }

    Compiler_Context compiler_ctx = NewCompilerContext(options);

    Open_File *file = OpenFile(&compiler_ctx, source);
    if (!file)
    {
        printf("Error reading source file '%s'\n", source);
    }
    if (file && Compile(&compiler_ctx, file))
    {
        printf("Compilation ok\n");
    }
    else
    {
        printf("Compilation failed\n");
    }

    FreeCompilerContext(&compiler_ctx);

    return 0;
}
