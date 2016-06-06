#ifndef H_HPLANG_REG_ALLOC_H

#include "types.h"
#include "array.h"
#include "codegen.h"

namespace hplang
{

enum Reg_Flag
{
    RF_None         = 0,
    RF_Return       = 1,
    RF_Arg          = 2,
    RF_CallerSave   = 4,
    RF_Float        = 8,
    RF_NonAllocable = 16, // base pointer and stack pointer

    //RF_Dirty            = 8,
    //RF_CalleeSaveDirty  = 16,
};

struct Reg_Info
{
    u8 reg_index;
    s8 index;       // Argument or return register index
    u8 reg_flags;
};

struct Live_Interval
{
    s32 start;
    s32 end;
    Name name;
    Reg reg;
    Oper_Data_Type data_type;
    b32 is_fixed;
};

struct Spill_Info
{
    Live_Interval spill;
    s32 instr_index;
    b32 is_spill; // true, if this is a spill, otherwise this is an unspill.
};

#if 0
struct Reg_Var
{
    Name var_name;
    Reg reg;
    Oper_Data_Type data_type;
};
#endif

struct Special_Regs
{
    Array<Reg> return_regs;
    Array<Reg> arg_regs;
};

struct Reg_Alloc
{
#if 0
    Array<u8> reg_flags;
    Array<Reg_Var> mapped_regs;
#endif
    Array<Reg> free_regs;
    Array<Reg> free_float_regs;

    Array<Spill_Info> spills;

    s64 reg_count;
    const Reg_Info *reg_info;

    Special_Regs general_regs;
    Special_Regs float_regs;
#if 0
    s64 general_reg_count;
    const Reg *general_regs;
    s64 float_reg_count;
    const Reg *float_regs;

    s64 arg_reg_count;
    const Reg *arg_regs;
    s64 float_arg_reg_count;
    const Reg *float_arg_regs;

    s64 caller_save_count;
    const Reg *caller_save_regs;
    s64 callee_save_count;
    const Reg *callee_save_regs;
#endif
};

void InitRegAlloc(Reg_Alloc *reg_alloc,
        s64 total_reg_count, const Reg_Info *reg_info);

void FreeRegAlloc(Reg_Alloc *reg_alloc);

void ResetRegAlloc(Reg_Alloc *reg_alloc);

b32 IsCallerSave(Reg_Alloc *reg_alloc, Reg reg);
b32 IsCalleeSave(Reg_Alloc *reg_alloc, Reg reg);
b32 IsFloatRegister(Reg_Alloc *reg_alloc, Reg reg);

Reg GetFreeRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type);
void ReleaseRegister(Reg_Alloc *reg_alloc, Reg reg, Oper_Data_Type data_type);

const Reg* GetReturnRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type, s64 ret_index);
const Reg* GetArgRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type, s64 arg_index);
//const Reg* GetArgRegister(Reg_Alloc *reg_alloc, s64 arg_index);
//const Reg* GetFloatArgRegister(Reg_Alloc *reg_alloc, s64 arg_index);

#if 0
void ClearRegAllocs(Reg_Alloc *reg_alloc);
void DirtyCalleeSaveRegs(Reg_Alloc *reg_alloc);

void DirtyRegister(Reg_Alloc *reg_alloc, Reg reg);
b32 UndirtyRegister(Reg_Alloc *reg_alloc, Reg reg);
b32 UndirtyCalleeSave(Reg_Alloc *reg_alloc, Reg reg);

void MapRegister(Reg_Alloc *reg_alloc, Name name, Reg reg, Oper_Data_Type data_type);
void UnmapRegister(Reg_Alloc *reg_alloc, Reg reg);
const Reg* GetMappedRegister(Reg_Alloc *reg_alloc, Name name);
const Reg_Var* GetMappedVar(Reg_Alloc *reg_alloc, Reg reg);
#endif

} // hplang

#define H_HPLANG_REG_ALLOC_H
#endif
