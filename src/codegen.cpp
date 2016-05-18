
#include "codegen.h"
#include "amd64_codegen.h"
#include "compiler.h"

namespace hplang
{

void InitRegAlloc(Reg_Alloc *reg_alloc,
        s64 caller_save_count, const Reg *caller_save_regs,
        s64 callee_save_count, const Reg *callee_save_regs)
{
    *reg_alloc = { };
    reg_alloc->caller_save_count = caller_save_count;
    reg_alloc->caller_save_regs = caller_save_regs;
    reg_alloc->callee_save_count = callee_save_count;
    reg_alloc->callee_save_regs = callee_save_regs;
}

Codegen_Context NewCodegenContext(
        Compiler_Context *comp_ctx, Codegen_Target cg_target)
{
    Codegen_Context cg_ctx = { };
    cg_ctx.target = cg_target;
    cg_ctx.code_out = comp_ctx->error_ctx.file; // NOTE(henrik): Just for testing
    switch (cg_target)
    {
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
    array::Free(ctx->instructions);
}

void GenerateCode(Codegen_Context *ctx, Ir_Routine_List routines)
{
    switch (ctx->target)
    {
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
        case CGT_AMD64_Windows:
        case CGT_AMD64_Unix:
            OutputCode_Amd64(ctx);
            break;
    }
}

} // hplang
