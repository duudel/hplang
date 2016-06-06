
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
    printf("  compile <source> into binary executable\n");
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

static const char *diag_args[] = {
    "memory",
    "ir",
    "regalloc",
    nullptr
};

static const Arg_Option options[] = {
    {"output", 'o', nullptr, nullptr, "Sets the output filename", "filename"},
    {"diagnostic", 'd', diag_args, "MiR", "Selects the diagnostic options", nullptr},
    {"help", 'h', nullptr, nullptr, "Shows this help and exits", nullptr},
    {"version", 'v', nullptr, nullptr, "Prints the version information", nullptr},
    { }
};

int main(int argc, char **argv)
{
#if 0
    fprintf(stderr, "sizeof(File_Location) %lld\n", sizeof(File_Location));
    fprintf(stderr, "sizeof(Name) %lld\n", sizeof(Name));
    fprintf(stderr, "sizeof(Ast_Node) %lld\n", sizeof(Ast_Node));
    fprintf(stderr, "sizeof(Ast_Expr) %lld\n", sizeof(Ast_Expr));
    fprintf(stderr, "sizeof(Ast_Function_Def) %lld\n", sizeof(Ast_Function_Def));
    fprintf(stderr, "sizeof(Ir_Operand) %lld\n", sizeof(Ir_Operand));
    fprintf(stderr, "sizeof(Ir_Instruction) %lld\n", sizeof(Ir_Instruction));
    fprintf(stderr, "sizeof(Operand) %lld\n", sizeof(Operand));
    fprintf(stderr, "sizeof(Instruction) %lld\n", sizeof(Instruction));
    fprintf(stderr, "sizeof(Oper_Data_Type) %lld\n", sizeof(Oper_Data_Type));
    fprintf(stderr, "sizeof(Oper_Access_Flags) %lld\n", sizeof(Oper_Access_Flags));
    fflush(stderr);
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
                case 'd':
                {
                    if (option_result.short_args)
                    {
                        const char *p = option_result.short_args;
                        for (; p[0] != '\0'; p++)
                        {
                            switch (p[0])
                            {
                                case 'M':
                                    options.diagnose_memory = true;
                                    break;
                                case 'i':
                                    options.debug_ir = true;
                                    break;
                                case 'R':
                                    options.debug_reg_alloc = true;
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
                            options.diagnose_memory = true;
                        else if (strcmp(arg, "ir") == 0)
                            options.debug_ir = true;
                        else if (strcmp(arg, "regalloc") == 0)
                            options.debug_reg_alloc = true;
                        else
                        {
                            printf("Unrecognized argument %s for --%s\n",
                                    arg, option_result.option->long_name);
                            return -1;
                        }
                    }
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
