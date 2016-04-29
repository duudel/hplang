
#include "hplang.h"
#include "lexer.h"
#include "compiler.h"
#include "error.h"

#include <cstdio>
#include <cstring>

using namespace hplang;

b32 HasArg(const char *arg, int argc, char *const *argv)
{
    for (s64 i = 1; i < argc; i++)
    {
        if (strcmp(arg, argv[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

void PrintUsage()
{
    printf("hplang <source>\n");
    printf("  compile <source> into binary executable");
    printf("hplang -v");
    printf("  print version of the compiler");
    printf("hplang -h");
    printf("  print help");
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

    const char *source = argv[1];

    Compiler_Context compiler_ctx = NewCompilerContext();

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
