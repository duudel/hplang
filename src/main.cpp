
#include "hplang.h"
#include "lexer.h"

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
    printf("hplang %d.%d.%s\n",
            HPLANG_VER_MAJOR, HPLANG_VER_MINOR, HPLANG_VER_PATCH);
    printf("Copyright (c) 2016 Henrik Paananen\n");
}

struct String
{
    s64 size;
    char *data;
};

struct Open_File
{
    String filename;
    Pointer contents;
};

struct Memory_Block
{
    Pointer memory;
    s64 top_pointer;
    Memory_Block *prev;
};

struct Memory_Arena
{
    Memory_Block *head;
};


b32 AllocateNewMemoryBlock(Memory_Arena *arena)
{
    s64 MEMORY_BLOCK_SIZE = MBytes(16);
    Pointer data = Alloc(sizeof(Memory_Block) + MEMORY_BLOCK_SIZE);
    if (!data.ptr) return false;

    Memory_Block *block = (Memory_Block*)data.ptr;
    block->memory.ptr = (void*)(block + 1);
    block->memory.size = MEMORY_BLOCK_SIZE;
    block->top_pointer = 0;

    block->prev = arena->head;
    arena->head = block;
    return true;
}


void* AllocateFromMemoryBlock(Memory_Block *block, s64 size, s64 alignment)
{
    if (!block)
        return nullptr;
    if (block->top_pointer + size > block->memory.size)
        return nullptr;
    char *ptr = (char*)block->memory.ptr + block->top_pointer;
    block->top_pointer += size;
    return (void*)ptr;
}


void* PushData(Memory_Arena *arena, s64 size, s64 alignment)
{
    void *ptr = AllocateFromMemoryBlock(arena->head, size, alignment);
    if (!ptr)
    {
        if (!AllocateNewMemoryBlock(arena))
            return nullptr;
        ptr = AllocateFromMemoryBlock(arena->head, size, alignment);
    }
    return ptr;
}

Pointer PushDataPointer(Memory_Arena *arena, s64 size, s64 alignment)
{
    Pointer result = { };
    result.ptr = PushData(arena, size, alignment);
    result.size = result.ptr ? size : 0;
    return result;
}

template <class S>
S* PushStruct(Memory_Arena *arena)
{
    return (S*)PushData(arena, sizeof(S), alignof(S));
}

String PushString(Memory_Arena *arena, const char *s, s64 size)
{
    String result = { };
    result.data = (char*)PushData(arena, size, 1);
    if (result.data)
    {
        for (s64 i = 0; i < size; i++)
            result.data[i] = s[i];
        result.size = size;
    }
    return result;
}

String PushString(Memory_Arena *arena, const char *s)
{
    return PushString(arena, s, strlen(s));
}

struct Compiler_Context
{
    Memory_Arena arena;
};

Open_File* OpenFile(Compiler_Context *ctx, const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file) return nullptr;

    Open_File *open_file = PushStruct<Open_File>(&ctx->arena);
    open_file->filename = PushString(&ctx->arena, filename);
    if (open_file && open_file->filename.data)
    {
        fseek(file, 0, SEEK_END);
        s64 file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        // NOTE(henrik): Allocate one extra byte for null termination.
        open_file->contents = PushDataPointer(&ctx->arena, file_size + 1, 1);

        if (fread(open_file->contents.ptr, 1, file_size, file) != file_size)
        {
            // NOTE(henrik): We do not free the open_file->contents here as
            // the file contents will be freed with the compiler context.
            // There was already an error reading the file, so the compilation
            // will not complete in any case. But if we want, we could introduce
            // an API to "rewind" the memory arena, but only if the pointers are
            // freed in the correct order. This needs information on how big the
            // allocation was to make sure we rewound the arena correctly.
            fprintf(stderr, "Error reading source file '%s', exiting...\n", filename);
            open_file->contents = { };
        }
        else
        {
            char *text = (char*)open_file->contents.ptr;
            text[file_size] = 0;
        }
        fclose(file);
    }
    return open_file;
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

    Compiler_Context cctx = { };

    Open_File *file = OpenFile(&cctx, source);

    //FILE *source_file = fopen(source, "rb");
    //if (!source_file)
    //{
    //    printf("Could not open source file '%s', exiting...\n", source);
    //    return -1;
    //}

    //fseek(source_file, 0, SEEK_END);
    //s64 file_size = ftell(source_file);
    //fseek(source_file, 0, SEEK_SET);

    //Pointer source_data = Alloc(file_size + 1);
    //if (fread(source_data.ptr, 1, file_size, source_file) != file_size)
    //{
    //    Free(source_data);
    //    printf("Error reading source file '%s', exiting...\n", source);
    //    return -1;
    //}
    //fclose(source_file);

    //char *source_text = (char*)source_data.ptr;
    //source_text[source_data.size - 1] = 0;

    //printf("source %d: %s\n", source_data.size, source_text);

    char *source_text = (char*)file->contents.ptr;
    printf("source %s\n", source_text);

    LexerContext lexer_ctx = NewLexerContext();
    Lex(&lexer_ctx, source_text, file->contents.size);

//    const char text[] = "if (test) return;\nelse a + b;\n    /*+-*/ ++ += . :: import for (bool) while string 0.0 3.12f, 43.0d @ #";
//    s64 text_length = sizeof(text);
//    Lex(&lexer_ctx, text, text_length);

    return 0;
}
