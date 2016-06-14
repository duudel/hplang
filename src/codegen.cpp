
#include "codegen.h"
#include "reg_alloc.h"
#include "amd64_codegen.h"
#include "compiler.h"

namespace hplang
{

Codegen_Context NewCodegenContext(IoFile *out,
        Compiler_Context *comp_ctx, Codegen_Target cg_target)
{
    Codegen_Context cg_ctx = { };
    cg_ctx.target = cg_target;
    cg_ctx.reg_alloc = PushStruct<Reg_Alloc>(&cg_ctx.arena);
    cg_ctx.code_out = out;
    cg_ctx.comp_ctx = comp_ctx;

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
    return cg_ctx;
}

void FreeCodegenContext(Codegen_Context *ctx)
{
    FreeRegAlloc(ctx->reg_alloc);
    ctx->reg_alloc = nullptr;
    for (s64 i = 0; i < ctx->routine_count; i++)
    {
        Routine *routine = &ctx->routines[i];
        array::Free(routine->local_offsets);
        array::Free(routine->labels);
        array::Free(routine->instructions);
        array::Free(routine->prologue);
        array::Free(routine->callee_save_spills);
        array::Free(routine->callee_save_unspills);
        array::Free(routine->epilogue);
    }
    ctx->routine_count = 0;
    ctx->routines = nullptr;

    array::Free(ctx->float32_consts);
    array::Free(ctx->float64_consts);
    array::Free(ctx->str_consts);

    FreeMemoryArena(&ctx->arena);
}

void GenerateCode(Codegen_Context *ctx, Ir_Routine_List routines,
        Array<Name> foreign_routines, Array<Symbol*> global_vars)
{
    ctx->foreign_routine_count = foreign_routines.count;
    ctx->foreign_routines = PushArray<Name>(&ctx->arena, foreign_routines.count);
    for (s64 i = 0; i < ctx->foreign_routine_count; i++)
    {
        ctx->foreign_routines[i] = foreign_routines[i];
    }

    ctx->global_var_count = global_vars.count;
    ctx->global_vars = PushArray<Symbol*>(&ctx->arena, global_vars.count);
    for (s64 i = 0; i < ctx->global_var_count; i++)
    {
        ctx->global_vars[i] = global_vars[i];
    }

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
