
#include "compiler.h"
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "ast_types.h"
#include "semantic_check.h"
#include "ir_gen.h"
#include "codegen.h"

#include <cstdio>
#include <cinttypes>

#include <cstdlib> // for system()

namespace hplang
{

Compiler_Context NewCompilerContext(Compiler_Options options)
{
    Compiler_Context result = { };
    result.error_ctx.file = (IoFile*)stderr;
    result.options = options;
    result.env = NewEnvironment("main");
    result.result = RES_OK;
    return result;
}

Compiler_Context NewCompilerContext()
{
    return NewCompilerContext(DefaultCompilerOptions());
}

static void FreeModule(Module *module)
{
    FreeAst(&module->ast);
}

void FreeCompilerContext(Compiler_Context *ctx)
{
    FreeEnvironment(&ctx->env);
    for (s64 i = 0; i < ctx->modules.count; i++)
    {
        FreeModule(array::At(ctx->modules, i));
    }
    array::Free(ctx->modules);
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

static Open_File* OpenFile_(Compiler_Context *ctx, FILE *file, Open_File *open_file)
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
    String filename_str = PushNullTerminatedString(&ctx->arena, filename, filename_end);
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

void PrintSourceLineAndArrow(Compiler_Context *ctx, File_Location file_loc)
{
    if (ctx->error_ctx.error_count <= ctx->options.max_line_arrow_error_count)
    {
        fprintf((FILE*)ctx->error_ctx.file, "\n");
        PrintFileLine(ctx->error_ctx.file, file_loc);
        PrintFileLocArrow(ctx->error_ctx.file, file_loc);
        fprintf((FILE*)ctx->error_ctx.file, "\n");
    }
}

static void PrintAstMem(IoFile *file, Ast *ast)
{
    s64 used, unused;
    GetMemoryArenaUsage(&ast->arena, &used, &unused);
    fprintf((FILE*)file, " ast (used = %" PRId64 "; unused = %" PRId64 "; stmts = %d; exprs = %d)\n",
            used, unused, ast->stmt_count, ast->expr_count);
}

static void PrintMemoryDiagnostic(Compiler_Context *ctx)
{
    if (!ctx->options.diagnose_memory)
        return;

    IoFile *file = ctx->error_ctx.file;
    s64 used, unused;

    GetMemoryArenaUsage(&ctx->arena, &used, &unused);
    fprintf((FILE*)file, "ctx (used = %" PRId64 "; unused = %" PRId64 ")\n",
            used, unused);

    GetMemoryArenaUsage(&ctx->env.arena, &used, &unused);
    fprintf((FILE*)file, "env (used = %" PRId64 "; unused = %" PRId64 ")\n",
            used, unused);

    for (s64 i = 0; i < ctx->modules.count; i++)
    {
        Module *module = array::At(ctx->modules, i);
        fprintf((FILE*)file, "module '");
        PrintString(file, module->module_file->filename);
        fprintf((FILE*)file, "':\n");
        PrintAstMem(file, &module->ast);
    }
}

s64 Invoke(const char *command, const char **args, s64 arg_count)
{
    const s64 buf_size = 1024;
    char buf[buf_size];
    s64 len = snprintf(buf, buf_size, "%s", command);
    for (s64 i = 0; i < arg_count; i++)
    {
        len += snprintf(buf + len, buf_size - len, " %s", args[i]);
    }
    fprintf(stderr, "Invoking: %s\n", buf);
    return system(buf);
}

b32 Compile(Compiler_Context *ctx, Open_File *open_file)
{
    Module *root_module = PushStruct<Module>(&ctx->arena);
    *root_module = { };
    root_module->module_file = open_file;
    array::Push(ctx->modules, root_module);

    // Lexing
    Token_List tokens = { };
    Lexer_Context lexer_ctx = NewLexerContext(&tokens, open_file, ctx);
    Lex(&lexer_ctx);
    if (HasError(ctx))
    {
        FreeTokenList(&tokens);
        FreeLexerContext(&lexer_ctx);
        ctx->result = RES_FAIL_Lexing;
        return false;
    }

    FreeLexerContext(&lexer_ctx);

    PrintMemoryDiagnostic(ctx);

    if (ctx->options.stop_after == CP_Lexing)
    {
        FreeTokenList(&tokens);
        ctx->result = RES_OK;
        return true;
    }

    // Parsing
    Ast *ast = &root_module->ast;
    Parser_Context parser_ctx = NewParserContext(
            ast, &tokens, open_file, ctx);

    Parse(&parser_ctx);
    if (HasError(ctx))
    {
        FreeTokenList(&tokens);
        FreeParserContext(&parser_ctx);
        ctx->result = RES_FAIL_Parsing;
        return false;
    }

    FreeTokenList(&tokens);
    FreeParserContext(&parser_ctx);

    PrintMemoryDiagnostic(ctx);

    if (ctx->options.stop_after == CP_Parsing)
    {
        ctx->result = RES_OK;
        return true;
    }

    // Semantic checking
    Sem_Check_Context sem_ctx = NewSemanticCheckContext(ast, open_file, ctx);

    Check(&sem_ctx);
    if (HasError(ctx))
    {
        FreeSemanticCheckContext(&sem_ctx);
        ctx->result = RES_FAIL_SemanticCheck;
        return false;
    }

    FreeSemanticCheckContext(&sem_ctx);

    PrintMemoryDiagnostic(ctx);

    if (ctx->options.stop_after == CP_Checking)
    {
        ctx->result = RES_OK;
        return true;
    }

    // IR generation
    Ir_Gen_Context ir_ctx = NewIrGenContext(ctx);

    GenIr(&ir_ctx);
    PrintIr(ctx->error_ctx.file, &ir_ctx);

    PrintMemoryDiagnostic(ctx);

    if (ctx->options.stop_after == CP_IrGen)
    {
        FreeIrGenContext(&ir_ctx);
        ctx->result = RES_OK;
        return true;
    }

    const char *asm_filename = "./out.s";
    FILE *code_file = fopen(asm_filename, "w");
    if (!code_file)
    {
        fprintf((FILE*)ctx->error_ctx.file, "Could not open '%s' for output\n",
                asm_filename);
        return false;
    }

    Codegen_Context cg_ctx = NewCodegenContext((IoFile*)code_file, ctx, CGT_AMD64_Windows);
    GenerateCode(&cg_ctx, ir_ctx.routines, ir_ctx.foreign_routines);

    OutputCode(&cg_ctx);

    FreeIrGenContext(&ir_ctx);
    FreeCodegenContext(&cg_ctx);

    //Invoke("compile_out.sh", nullptr, 0);

    //const char *obj_filename = "./out.o";
    //const char *nasm_args[] = {"-fwin64", "-o", obj_filename, "--", asm_filename};
    //if (Invoke("nasm", nasm_args, array_length(nasm_args)) != 0)
    //{
    //    fprintf((FILE*)ctx->error_ctx.file, "Could not write assemble the file '%s'\n",
    //            asm_filename);
    //    return false;
    //}

    //const char *exe_filename = "./out.exe";
    //const char *gcc_args[] = {"-o", exe_filename, obj_filename};
    //if (Invoke("gcc", gcc_args, array_length(gcc_args)) != 0)
    //{
    //    fprintf((FILE*)ctx->error_ctx.file, "Could not link the file '%s'\n",
    //            obj_filename);
    //    return false;
    //}

    ctx->result = RES_OK;
    return true;
}

b32 CompileModule(Compiler_Context *ctx, Open_File *open_file)
{
    Module *module = PushStruct<Module>(&ctx->arena);
    *module = { };
    module->module_file = open_file;
    array::Push(ctx->modules, module);

    // Lexing
    Token_List tokens = { };
    Lexer_Context lexer_ctx = NewLexerContext(&tokens, open_file, ctx);
    Lex(&lexer_ctx);
    if (HasError(ctx))
    {
        FreeTokenList(&tokens);
        FreeLexerContext(&lexer_ctx);
        ctx->result = RES_FAIL_Lexing;
        return false;
    }

    FreeLexerContext(&lexer_ctx);

    if (ctx->options.stop_after == CP_Lexing)
    {
        FreeTokenList(&tokens);
        ctx->result = RES_OK;
        return true;
    }

    // Parsing
    Ast *ast = &module->ast;
    Parser_Context parser_ctx = NewParserContext(
            ast, &tokens, open_file, ctx);

    Parse(&parser_ctx);
    if (HasError(ctx))
    {
        FreeTokenList(&tokens);
        FreeParserContext(&parser_ctx);
        ctx->result = RES_FAIL_Parsing;
        return false;
    }

    FreeTokenList(&tokens);
    FreeParserContext(&parser_ctx);

    if (ctx->options.stop_after == CP_Parsing)
    {
        ctx->result = RES_OK;
        return true;
    }

    // Semantic checking
    Sem_Check_Context sem_ctx = NewSemanticCheckContext(ast, open_file, ctx);

    Check(&sem_ctx);
    if (HasError(ctx))
    {
        FreeSemanticCheckContext(&sem_ctx);
        ctx->result = RES_FAIL_SemanticCheck;
        return false;
    }

    FreeSemanticCheckContext(&sem_ctx);

    if (ctx->options.stop_after == CP_Checking)
    {
        ctx->result = RES_OK;
        return true;
    }

    ctx->result = RES_OK;
    return true;
}

} // hplang
