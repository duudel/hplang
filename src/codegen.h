#ifndef H_HPLANG_CODEGEN_H

#include "types.h"
#include "array.h"
#include "ir_types.h"
#include "io.h"

namespace hplang
{

struct Reg
{
    Name name;
};

struct Reg_Alloc
{
    //map<Variable, Reg> mapped_regs;
    Array<Reg> free_regs;

    s64 caller_save_count;
    const Reg *caller_save_regs; // these registers are considered nonvolatile
    s64 callee_save_count;
    const Reg *callee_save_regs; // these registers are considered volatile
};

void InitRegAlloc(Reg_Alloc *reg_alloc,
        s64 caller_save_count, const Reg *caller_save_regs,
        s64 callee_save_count, const Reg *callee_save_regs);


enum Codegen_Target
{
    CGT_AMD64_Windows,
    CGT_AMD64_Unix
};


enum Operand_Type
{
    OT_None,
    OT_Register,
    OT_Immediate,
    OT_Label,
    OT_IrOperand,
};

struct Label
{
    Name name;
};

struct Operand
{
    Operand_Type type;
    union {
        Reg reg;
        Label label;
        Ir_Operand ir_oper;
        union {
            u64 imm_u64;
            s64 imm_s64;
            f32 imm_f32;
            f64 imm_f64;
        };
    };
};

enum Opcode { };

struct Instruction
{
    Opcode opcode;
    Operand oper1;
    Operand oper2;
    Operand oper3;
};

typedef Array<Instruction> Instructon_List;

struct Compiler_Context;

struct Codegen_Context
{
    Codegen_Target target;
    Reg_Alloc reg_alloc;

    Instructon_List instructions;
    IoFile *code_out;

    Compiler_Context *comp_ctx;
};

Codegen_Context NewCodegenContext(
        Compiler_Context *comp_ctx, Codegen_Target cg_target);
void FreeCodegenContext(Codegen_Context *ctx);

void GenerateCode(Codegen_Context *ctx, Ir_Routine_List routines);

void OutputCode(Codegen_Context *ctx);

} // hplang

#define H_HPLANG_CODEGEN_H
#endif
