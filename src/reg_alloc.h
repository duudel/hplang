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
    Live_Interval *next;

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

struct Special_Regs
{
    Array<Reg> return_regs;
    Array<Reg> arg_regs;
};

struct Reg_Alloc
{
    Array<Reg> free_regs;
    Array<Reg> free_float_regs;

    Array<Spill_Info> spills;

    s64 reg_count;
    const Reg_Info *reg_info;

    // is argument index shared between arguments passed in general registers
    // and float registers.
    b32 arg_index_shared;

    Special_Regs general_regs;
    Special_Regs float_regs;

    u64 dirty_regs; // assumes that total register count < 64
};

void InitRegAlloc(Reg_Alloc *reg_alloc,
        s64 total_reg_count, const Reg_Info *reg_info,
        b32 arg_index_shared);

void FreeRegAlloc(Reg_Alloc *reg_alloc);

void ResetRegAlloc(Reg_Alloc *reg_alloc);
void DirtyRegister(Reg_Alloc *reg_alloc, Reg reg);
b32 IsRegisterDirty(Reg_Alloc *reg_alloc, Reg reg);

b32 IsCallerSave(Reg_Alloc *reg_alloc, Reg reg);
b32 IsCalleeSave(Reg_Alloc *reg_alloc, Reg reg);
b32 IsFloatRegister(Reg_Alloc *reg_alloc, Reg reg);

b32 HasFreeRegisters(Reg_Alloc *reg_alloc, Oper_Data_Type data_type);
Reg GetFreeRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type);
void ReleaseRegister(Reg_Alloc *reg_alloc, Reg reg, Oper_Data_Type data_type);

const Reg* GetReturnRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type, s64 ret_index);
const Reg* GetArgRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type, s64 arg_index);
void AdvanceArgIndex(Reg_Alloc *reg_alloc, Oper_Data_Type data_type,
        s64 *arg_index, s64 *float_arg_index);

} // hplang

#define H_HPLANG_REG_ALLOC_H
#endif
