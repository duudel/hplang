#ifndef H_HPLANG_REG_ALLOC_H

#include "types.h"
#include "array.h"
#include "codegen.h"

namespace hplang
{

struct Reg_Var
{
    Name var_name;
    Reg reg;
    Oper_Data_Type data_type;
};

struct Reg_Alloc
{
    Array<Reg_Var> mapped_regs;
    Array<Reg> free_regs;
    Array<Reg> free_float_regs;
    Array<u8> reg_flags;

    s64 general_reg_count;
    const Reg *general_regs;
    s64 float_reg_count;
    const Reg *float_regs;

    s64 arg_reg_count;
    const Reg *arg_regs;
    s64 float_arg_reg_count;
    const Reg *float_arg_regs;

    s64 caller_save_count;
    const Reg *caller_save_regs; // these registers are considered nonvolatile
    s64 callee_save_count;
    const Reg *callee_save_regs; // these registers are considered volatile
};

void InitRegAlloc(Reg_Alloc *reg_alloc,
        s64 total_reg_count,
        s64 general_reg_count, const Reg *general_regs,
        s64 float_reg_count, const Reg *float_regs,
        s64 arg_reg_count, const Reg *arg_regs,
        s64 float_arg_reg_count, const Reg *float_arg_regs,
        s64 caller_save_count, const Reg *caller_save_regs,
        s64 callee_save_count, const Reg *callee_save_regs);

void FreeRegAlloc(Reg_Alloc *reg_alloc);


b32 IsCallerSave(Reg_Alloc *reg_alloc, Reg reg);
b32 IsCalleeSave(Reg_Alloc *reg_alloc, Reg reg);
b32 IsFloatRegister(Reg_Alloc *reg_alloc, Reg reg);

const Reg* GetArgRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type, s64 arg_index);
const Reg* GetArgRegister(Reg_Alloc *reg_alloc, s64 arg_index);
const Reg* GetFloatArgRegister(Reg_Alloc *reg_alloc, s64 arg_index);

void ClearRegAllocs(Reg_Alloc *reg_alloc);
void DirtyCalleeSaveRegs(Reg_Alloc *reg_alloc);

void DirtyRegister(Reg_Alloc *reg_alloc, Reg reg);
b32 UndirtyRegister(Reg_Alloc *reg_alloc, Reg reg);
b32 UndirtyCalleeSave(Reg_Alloc *reg_alloc, Reg reg);

void MapRegister(Reg_Alloc *reg_alloc, Name name, Reg reg, Oper_Data_Type data_type);
void UnmapRegister(Reg_Alloc *reg_alloc, Reg reg);
const Reg* GetMappedRegister(Reg_Alloc *reg_alloc, Name name);
const Reg_Var* GetMappedVar(Reg_Alloc *reg_alloc, Reg reg);

Reg GetFreeRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type);

} // hplang

#define H_HPLANG_REG_ALLOC_H
#endif
