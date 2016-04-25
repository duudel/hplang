
#include "compiler.h"
#include "lexer.h"
#include "parser.h"
#include "ast_types.h"
#include "semantic_check.h"

#include <cstdio>

namespace hplang
{

Compiler_Context NewCompilerContext()
{
    Compiler_Context result = { };
    result.error_ctx.file = stderr;
    result.options = DefaultCompilerOptions();
    result.result = RES_OK;
    return result;
}

void FreeCompilerContext(Compiler_Context *ctx)
{
    FreeMemoryArena(&ctx->arena);
}

static void SetOpenFileBaseEnd(Open_File *open_file)
{
    s64 i = open_file->filename.size;
    while (i > 0)
    {
        i--;
        if (open_file->filename.data[i] == '/')
        {
            i++;
            break;
        }
    }
    open_file->base_end = i;
}

Open_File* OpenFile_(Compiler_Context *ctx, FILE *file, Open_File *open_file)
{
    SetOpenFileBaseEnd(open_file);

    fseek(file, 0, SEEK_END);
    s64 file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // NOTE(henrik): Allocate one extra byte for null termination.
    open_file->contents = PushDataPointer(&ctx->arena, file_size + 1, 1);

    if (fread(open_file->contents.ptr, 1, file_size, file) != (u64)file_size)
    {
        // NOTE(henrik): We do not free the open_file->contents here as
        // the file contents will be freed with the compiler context.
        // There was already an error reading the file, so the compilation
        // will not complete in any case. But if we want, we could introduce
        // an API to "rewind" the memory arena, but only if the pointers are
        // freed in the correct order. This needs information on how big the
        // allocation was to make sure we rewound the arena correctly.
        open_file = nullptr;
    }
    else
    {
        char *text = (char*)open_file->contents.ptr;
        text[file_size] = 0;
    }
    return open_file;
}

Open_File* OpenFile(Compiler_Context *ctx, const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file) return nullptr;

    Open_File *open_file = PushStruct<Open_File>(&ctx->arena);
    if (open_file)
    {
        open_file->filename = PushString(&ctx->arena, filename);
        open_file = OpenFile_(ctx, file, open_file);
    }
    fclose(file);
    return open_file;
}

Open_File* OpenFile(Compiler_Context *ctx, const char *filename,
        const char *filename_end)
{
    String filename_str = PushNullTerminatedString(&ctx->arena, filename, filename_end - filename);
    FILE *file = fopen(filename_str.data, "rb");
    if (!file) return nullptr;

    Open_File *open_file = PushStruct<Open_File>(&ctx->arena);
    if (open_file)
    {
        open_file->filename = filename_str;
        open_file = OpenFile_(ctx, file, open_file);
    }
    fclose(file);
    return open_file;
}

Open_File* OpenFile(Compiler_Context *ctx, String filename)
{
    return OpenFile(ctx, filename.data, filename.data + filename.size);
}

// TODO(henrik): This code path pushes the filename string to ctx->arena first
// here and then in OpenFile. Remove the double pushing.
Open_File* OpenModule(Compiler_Context *ctx,
        Open_File *current_file, String module_name, String *filename_out)
{
    const char extension[] = ".hp";
    String filename_str;

    s64 filename_size = current_file->base_end + module_name.size + sizeof(extension);
    char *filename = (char*)PushData(&ctx->arena, filename_size, 1);

    filename_str.data = filename;
    filename_str.size = filename_size - 1;

    s64 i = 0;
    for (; i < current_file->base_end; i++)
    {
        filename_str.data[i] = current_file->filename.data[i];
    }
    for (; i < current_file->base_end + module_name.size; i++)
    {
        filename_str.data[i] = module_name.data[i - current_file->base_end];
    }
    filename_str.data[i+0] = '.';
    filename_str.data[i+1] = 'h';
    filename_str.data[i+2] = 'p';
    filename_str.data[i+3] = 0;
    *filename_out = filename_str;
    return OpenFile(ctx, filename_str);
}


b32 ContinueCompiling(Compiler_Context *ctx)
{
    return ctx->error_ctx.error_count < ctx->options.max_error_count;
}

b32 HasError(Compiler_Context *ctx)
{
    return ctx->error_ctx.error_count != 0;
}

void PrintSourceLineAndArrow(Compiler_Context *ctx,
        Open_File *open_file, File_Location file_loc)
{
    if (ctx->error_ctx.error_count <= ctx->options.max_line_arrow_error_count)
    {
        fprintf(ctx->error_ctx.file, "\n");
        PrintFileLine(ctx->error_ctx.file, open_file, file_loc);
        PrintFileLocArrow(ctx->error_ctx.file, file_loc);
        fprintf(ctx->error_ctx.file, "\n");
    }
}

b32 Compile(Compiler_Context *ctx, Open_File *file)
{
    // Lexing
    Token_List tokens = { };
    Lexer_Context lexer_ctx = NewLexerContext(&tokens, &ctx->error_ctx);
    lexer_ctx.file_loc.filename = file->filename;
    Lex(&lexer_ctx, (const char*)file->contents.ptr, file->contents.size);
    if (HasError(ctx))
    {
        FreeLexerContext(&lexer_ctx);
        ctx->result = RES_FAIL_Lexing;
        return false;
    }

    UnlinkTokens(&lexer_ctx);
    FreeLexerContext(&lexer_ctx);


    // Parsing
    Ast ast = { };
    Parser_Context parser_ctx = NewParserContext(
            &ast, &tokens, file, ctx);

    Parse(&parser_ctx);
    if (HasError(ctx))
    {
        FreeParserContext(&parser_ctx);
        ctx->result = RES_FAIL_Parsing;
        return false;
    }

    UnlinkAst(&parser_ctx);
    FreeParserContext(&parser_ctx);


    // Semantic checking
    Sem_Check_Context sem_ctx = NewSemanticCheckContext(&ast, file, ctx);

    Check(&sem_ctx);
    if (HasError(ctx))
    {
        FreeSemanticCheckContext(&sem_ctx);
        ctx->result = RES_FAIL_SemanticCheck;
        return false;
    }

    UnlinkAst(&sem_ctx);
    FreeSemanticCheckContext(&sem_ctx);

    // Finally free ast (and contained token list)
    FreeAst(&ast);

    ctx->result = RES_OK;
    return true;
}

bool CompileModule(Compiler_Context *ctx,
        Open_File *open_file, String module_filename)
{
    fprintf(stderr, "Module compilation not implemented yet\n");
    return false;
}

} // hplang
