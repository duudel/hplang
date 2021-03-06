
#include "compiler.h"
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "ast_types.h"
#include "semantic_check.h"
#include "ir_gen.h"
#include "codegen.h"
#include "time_profiler.h"

#include <cstdio>
#include <cinttypes>

#include <cstdlib> // for system()

namespace hplang
{

Compiler_Context NewCompilerContext(Compiler_Options options)
{
    Compiler_Context result = { };
    result.error_ctx.file = (IoFile*)stdout;
    result.debug_file = (IoFile*)stderr;
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
// NOTE(henrik): This version null terminates the string.
Open_File* OpenModule(Compiler_Context *ctx,
        Open_File *current_file, String module_name, String *filename_out)
{
    const char extension[] = ".hp";
    String filename_str;

    // NOTE(henrik): Module name starting with colon ':' is a system module.
    s64 i = 0;
    if (module_name.size > 0 && module_name.data[0] == ':')
    {
        const char stdlib[] = "stdlib/";
        s64 sizeof_stdlib = sizeof(stdlib) - 1; // discard null teermination
        s64 filename_size = sizeof_stdlib + module_name.size + sizeof(extension);
        char *filename = (char*)PushData(&ctx->arena, filename_size, 1);

        filename_str.data = filename;
        filename_str.size = filename_size - 1;

        for (; i < sizeof_stdlib; i++)
        {
            filename_str.data[i] = stdlib[i];
        }
        for (; i < sizeof_stdlib + module_name.size - 1; i++)
        {
            filename_str.data[i] = module_name.data[1 + i - sizeof_stdlib];
        }
    }
    else
    {
        s64 filename_size = current_file->base_end + module_name.size + sizeof(extension);
        char *filename = (char*)PushData(&ctx->arena, filename_size, 1);

        filename_str.data = filename;
        filename_str.size = filename_size - 1;

        for (; i < current_file->base_end; i++)
        {
            filename_str.data[i] = current_file->filename.data[i];
        }
        for (; i < current_file->base_end + module_name.size; i++)
        {
            filename_str.data[i] = module_name.data[i - current_file->base_end];
        }
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
        //fprintf((FILE*)ctx->error_ctx.file, "\n");
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

    IoFile *file = ctx->debug_file;
    FILE *f = (FILE*)file;
    s64 used, unused;

    GetMemoryArenaUsage(&ctx->arena, &used, &unused);
    fprintf(f, "ctx (used = %" PRId64 "; unused = %" PRId64 ")\n",
            used, unused);

    GetMemoryArenaUsage(&ctx->env.arena, &used, &unused);
    fprintf(f, "env (used = %" PRId64 "; unused = %" PRId64 ")\n",
            used, unused);

    for (s64 i = 0; i < ctx->modules.count; i++)
    {
        Module *module = array::At(ctx->modules, i);
        fprintf(f, "module '");
        PrintString(file, module->module_file->filename);
        fprintf(f, "':\n");
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
    //fprintf(stderr, "Invoking: %s\n", buf);
    return system(buf);
}

static String StripExtension(String filename)
{
    for (s64 i = filename.size - 1; i >= 0; i--)
    {
        if (filename.data[i] == '.')
        {
            filename.size = i;
            break;
        }
        else if (filename.data[i] == '/')
        {
            break;
        }
    }
    return filename;
}

static b32 CompileModule(Compiler_Context *ctx, Open_File *open_file, Module *module)
{
    // Lexing
    Token_List tokens = { };
    {
        PROFILE_SCOPE("Lexing");
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
    }

    if (ctx->options.stop_after == PHASE_Lexing)
    {
        FreeTokenList(&tokens);
        ctx->result = RES_OK;
        return true;
    }

    // Parsing
    Ast *ast = &module->ast;
    {
        PROFILE_SCOPE("Parsing");
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

        if (ctx->options.debug_ast)
            PrintAst(ctx->debug_file, ast);

        FreeTokenList(&tokens);
        FreeParserContext(&parser_ctx);
    }

    if (ctx->options.stop_after == PHASE_Parsing)
    {
        ctx->result = RES_OK;
        return true;
    }

    // Semantic checking
    {
        PROFILE_SCOPE("Semantic check");
        Sem_Check_Context sem_ctx = NewSemanticCheckContext(ast, open_file, ctx);

        Check(&sem_ctx);
        if (HasError(ctx))
        {
            FreeSemanticCheckContext(&sem_ctx);
            ctx->result = RES_FAIL_SemanticCheck;
            return false;
        }

        FreeSemanticCheckContext(&sem_ctx);
    }

    if (ctx->options.stop_after == PHASE_SemanticCheck)
    {
        ctx->result = RES_OK;
        return true;
    }

    ctx->result = RES_OK;
    return true;
}

b32 CompileModule(Compiler_Context *ctx, Open_File *open_file)
{
    Module *module = PushStruct<Module>(&ctx->arena);
    *module = { };
    module->module_file = open_file;
    array::Push(ctx->modules, module);
    return CompileModule(ctx, open_file, module);
}

static b32 Compile_(Compiler_Context *ctx, Open_File *open_file);

b32 Compile(Compiler_Context *ctx, Open_File *open_file)
{
    b32 result = Compile_(ctx, open_file);
    CollateProfilingData(ctx);
    return result;
}

static b32 Compile_(Compiler_Context *ctx, Open_File *open_file)
{
    PROFILE_SCOPE("Compilation");

    Module *root_module = PushStruct<Module>(&ctx->arena);
    *root_module = { };
    root_module->module_file = open_file;
    array::Push(ctx->modules, root_module);

    b32 result = CompileModule(ctx, open_file, root_module);
    if (!result ||
        ctx->options.stop_after == PHASE_Lexing ||
        ctx->options.stop_after == PHASE_Parsing ||
        ctx->options.stop_after == PHASE_SemanticCheck)
    {
        return result;
    }

    PrintMemoryDiagnostic(ctx);

    // TODO(henrik): rename?
    ResolveTypeInformation(&ctx->env);

    // IR generation
    Ir_Gen_Context ir_ctx = NewIrGenContext(ctx);
    {
        PROFILE_SCOPE("IR generation");
        GenIr(&ir_ctx);

        if (ctx->options.debug_ir)
            PrintIr(ctx->debug_file, &ir_ctx);
    }

    PrintMemoryDiagnostic(ctx);

    if (ctx->options.stop_after == PHASE_IrGen)
    {
        FreeIrGenContext(&ir_ctx);
        ctx->result = RES_OK;
        return true;
    }

    const char *asm_filename = "out.s";
    {
        PROFILE_SCOPE("Code generation");
        FILE *asm_file = fopen(asm_filename, "w");
        if (!asm_file)
        {
            fprintf((FILE*)ctx->error_ctx.file, "Could not open '%s' for output\n",
                    asm_filename);
            return false;
        }

        Codegen_Context cg_ctx = NewCodegenContext((IoFile*)asm_file, ctx, ctx->options.target);
        GenerateCode(&cg_ctx, ir_ctx.routines, ir_ctx.foreign_routines, ir_ctx.global_vars);

        OutputCode(&cg_ctx);

        fclose(asm_file);

        FreeIrGenContext(&ir_ctx);
        FreeCodegenContext(&cg_ctx);

        fflush((FILE*)ctx->error_ctx.file);
        fflush((FILE*)ctx->debug_file);
    }

    if (ctx->options.stop_after == PHASE_CodeGen)
    {
        ctx->result = RES_OK;
        return true;
    }

    // TODO(henrik): Specify the options for nasm and gcc somewhere else.
    // Mayby also move the assembling and linking to their own place.

    const char *obj_filename = "out.o";
    const char *nasm_fmt = nullptr;
    switch (ctx->options.target)
    {
        case CGT_AMD64_Windows:
            nasm_fmt = "-fwin64";
            break;
        case CGT_AMD64_Unix:
            nasm_fmt = "-felf64";
            break;
        case CGT_COUNT:
            INVALID_CODE_PATH;
            break;
    }
    const char *nasm_args[] = {
        nasm_fmt,
        "-o", obj_filename,
        "--", asm_filename};
    {
        PROFILE_SCOPE("Assembling");
        if (Invoke("nasm", nasm_args, array_length(nasm_args)) != 0)
        {
            fprintf((FILE*)ctx->error_ctx.file, "Could not assemble the file '%s'\n",
                    asm_filename);
            ctx->result = RES_FAIL_InternalError;
            return false;
        }
    }

    if (ctx->options.stop_after == PHASE_Assembling)
    {
        ctx->result = RES_OK;
        return true;
    }

    // TODO(henrik): derive the default output filenames from the source filename:
    // samples/factorial.hp -> samples/factorial.exe
    const char *bin_filename = ctx->options.output_filename;
    //if (!bin_filename) bin_filename = "out";
    if (!bin_filename)
    {
        String output_fname = StripExtension(open_file->filename);
        output_fname = PushNullTerminatedString(&ctx->arena, output_fname.data, output_fname.size);
        bin_filename = output_fname.data;
    }
    const char *gcc_target = nullptr;
    switch (ctx->options.target)
    {
        case CGT_AMD64_Windows:
            //gcc_target = "pei-x86-64";
            gcc_target = "-Wl,--oformat=pei-x86-64";
            break;
        case CGT_AMD64_Unix:
            //gcc_target = "elf64-x86-64";
            gcc_target = "-Wl,--oformat=elf64-x86-64";
            break;
        case CGT_COUNT:
            INVALID_CODE_PATH;
            break;
    }
    const char *gcc_args[] = {
        //"-O3",
        //"-flto",
        //"-Wl,--oformat=", gcc_target,
        gcc_target,
        "-Wl,-einit_",
        "-Lstdlib",
        "-o", bin_filename,
        obj_filename,
        "-lstdlib"};
    {
        PROFILE_SCOPE("Linking");
        if (Invoke("gcc", gcc_args, array_length(gcc_args)) != 0)
        {
            fprintf((FILE*)ctx->error_ctx.file, "Could not link the file '%s'\n",
                    obj_filename);
            ctx->result = RES_FAIL_Linking;
            return false;
        }
    }

    ASSERT(ctx->options.stop_after == PHASE_Linking);
    ctx->result = RES_OK;
    return true;
}

} // hplang
