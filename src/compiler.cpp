
#include "compiler.h"
#include "lexer.h"

#include <cstdio>

namespace hplang
{

void FreeCompilerContext(Compiler_Context *ctx)
{
    FreeMemoryArena(&ctx->arena);
}

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

b32 Compile(Compiler_Context *ctx, Open_File *file)
{
    ctx->error_ctx.compilation_phase = COMP_Lexing;

    Lexer_Context lexer_ctx = NewLexerContext(&ctx->error_ctx);
    lexer_ctx.file_loc.filename = file->filename;
    Lex(&lexer_ctx, (const char*)file->contents.ptr, file->contents.size);
    if (ctx->error_ctx.error_count != 0)
        return false;

    // parsing

    // semantic checking

    return true;
}

} // hplang
