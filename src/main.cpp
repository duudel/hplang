
#include "hplang.h"
#include "lexer.h"
#include "compiler.h"
#include "error.h"

#include <cstdio>
#include <cstring>

using namespace hplang;

s64 GetArgIndex(const char *arg, int argc, char *const *argv)
{
    for (s64 i = 1; i < argc; i++)
    {
        if (strcmp(arg, argv[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}

b32 HasArg(const char *arg, int argc, char *const *argv)
{
    return GetArgIndex(arg, argc, argv) != -1;
}

void PrintUsage()
{
    printf("hplang [options] <source>\n");
    printf("  compile <source> into binary executable\n");
    printf("options:\n");
    printf("  -d M \tdiagnose memory\n");
    printf("hplang -v\n");
    printf("  print version of the compiler\n");
    printf("hplang -h\n");
    printf("  print help\n");
}

void PrintHelp()
{
    printf("Not implemented yet!\n");
}

void PrintVersion()
{
    printf("%s\n", GetVersionString());
    printf("Copyright (c) 2016 Henrik Paananen\n");
}

int main(int argc, char **argv)
{
#if 0
    fprintf(stderr, "sizeof(File_Location) %lld\n", sizeof(File_Location));
    fprintf(stderr, "sizeof(Name) %lld\n", sizeof(Name));
    fprintf(stderr, "sizeof(Ast_Node) %lld\n", sizeof(Ast_Node));
    fprintf(stderr, "sizeof(Ast_Expr) %lld\n", sizeof(Ast_Expr));
    fprintf(stderr, "sizeof(Ast_Function_Def) %lld\n", sizeof(Ast_Function_Def));
    fflush(stderr);
    return 0;
#endif

    if (argc == 1)
    {
        PrintUsage();
        return 0;
    }

    if (HasArg("-v", argc, argv))
    {
        PrintVersion();
        return 0;
    }
    else if (HasArg("-h", argc, argv))
    {
        PrintHelp();
        return 0;
    }

    Compiler_Options options = DefaultCompilerOptions();

    s64 d_index = GetArgIndex("-d", argc, argv);
    if (d_index != -1)
    {
        if (d_index + 1 < argc)
        {
            const char *d_arg = argv[d_index + 1];
            if (strcmp(d_arg, "M") != 0)
            {
                printf("Invalid argument for -d option; must be M\n");
                return -1;
            }
            options.diagnose_memory = true;
        }
        else
        {
            printf("Option -d needs argument; must be M\n");
            return -1;
        }
    }

    const char *source = argv[1];

    Compiler_Context compiler_ctx = NewCompilerContext(options);

    Open_File *file = OpenFile(&compiler_ctx, source);
    if (!file)
    {
        fprintf(stderr, "Error reading source file '%s'\n", source);
    }
    if (file && Compile(&compiler_ctx, file))
    {
        fprintf(stderr, "Compilation ok\n");
    }
    else
    {
        fprintf(stderr, "Compilation failed\n");
    }

    FreeCompilerContext(&compiler_ctx);

    return 0;
}
