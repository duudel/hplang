
#include "codegen.h"
#include "amd64_codegen.h"
#include "compiler.h"

namespace hplang
{

Codegen_Context NewCodegenContext(
        Compiler_Context *comp_ctx, Codegen_Target cg_target)
{
    Codegen_Context cg_ctx = { };
    cg_ctx.target = cg_target;
    cg_ctx.code_out = comp_ctx->error_ctx.file; // NOTE(henrik): Just for testing
    switch (cg_target)
    {
        case CGT_COUNT:
            INVALID_CODE_PATH;
            break;
        case CGT_AMD64_Windows:
        case CGT_AMD64_Unix:
            InitializeCodegen_Amd64(&cg_ctx, cg_target);
            break;
    }
    cg_ctx.comp_ctx = comp_ctx;
    return cg_ctx;
}

void FreeCodegenContext(Codegen_Context *ctx)
{
    FreeRegAlloc(&ctx->reg_alloc);
    for (s64 i = 0; i < ctx->routine_count; i++)
    {
        Routine *routine = &ctx->routines[i];
        array::Free(routine->instructions);
    }
    ctx->routine_count = 0;
    ctx->routines = nullptr;
    FreeMemoryArena(&ctx->arena);
}

void GenerateCode(Codegen_Context *ctx, Ir_Routine_List routines)
{
    switch (ctx->target)
    {
        case CGT_COUNT:
            INVALID_CODE_PATH;
            break;
        case CGT_AMD64_Windows:
        case CGT_AMD64_Unix:
            GenerateCode_Amd64(ctx, routines);
            break;
    }
}

void OutputCode(Codegen_Context *ctx)
{
    switch (ctx->target)
    {
        case CGT_COUNT:
            INVALID_CODE_PATH;
            break;
        case CGT_AMD64_Windows:
        case CGT_AMD64_Unix:
            OutputCode_Amd64(ctx);
            break;
    }
}

static const char *target_strings[CGT_COUNT] = {
    /*[CGT_AMD64_Windows] =*/   "AMD64 Windows",
    /*[CGT_AMD64_Unix] =*/      "AMD64 Unix",
};

const char* GetTargetString(Codegen_Target target)
{
    return target_strings[target];
}

} // hplang
