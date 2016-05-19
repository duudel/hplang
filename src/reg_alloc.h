#ifndef H_HPLANG_REG_ALLOC_H

#include "types.h"
#include "array.h"

namespace hplang
{

struct Reg
{
    u8 reg_index;
    //Name name;
};

inline operator == (Reg r1, Reg r2)
{ return r1.reg_index == r2.reg_index; }
inline operator != (Reg r1, Reg r2)
{ return r1.reg_index != r2.reg_index; } 

struct Reg_Var
{
    Name var_name;
    Reg reg;
};

struct Reg_Alloc
{
    Array<Reg_Var> mapped_regs;
    Array<Reg> free_regs;
    Array<Reg> free_float_regs;
    Array<Reg> dirty_regs;

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
        s64 general_reg_count, const Reg *general_regs,
        s64 float_reg_count, const Reg *float_regs,
        s64 arg_reg_count, const Reg *arg_regs,
        s64 float_arg_reg_count, const Reg *float_arg_regs,
        s64 caller_save_count, const Reg *caller_save_regs,
        s64 callee_save_count, const Reg *callee_save_regs);

void FreeRegAlloc(Reg_Alloc *reg_alloc);

const Reg* GetArgRegister(Reg_Alloc *reg_alloc, s64 arg_index);
const Reg* GetFloatArgRegister(Reg_Alloc *reg_alloc, s64 arg_index);

void ClearRegAllocs(Reg_Alloc *reg_alloc);
void DirtyCalleeSaveRegs(Reg_Alloc *reg_alloc);

void DirtyRegister(Reg_Alloc *reg_alloc, Reg reg);
void MapRegister(Reg_Alloc *reg_alloc, Name name, Reg reg);
const Reg* GetMappedRegister(Reg_Alloc *reg_alloc, Name name);
Reg GetFreeRegister(Reg_Alloc *reg_alloc);

} // hplang

#define H_HPLANG_REG_ALLOC_H
#endif
