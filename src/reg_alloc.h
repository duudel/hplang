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
    b32 is_spilled;
};

enum class Spill_Type : u8
{
    Spill,      // A spill from register to a stack slot
    Unspill,    // An unspill (or fill) from stack slot to a register
    Move,       // A copy from a register to another register
};

struct Spill_Info
{
    const char *note;

    Live_Interval interval;
    s32 instr_index;

    Spill_Type spill_type;
    Reg target;
};

// Argument and return register index type for iterating over argument or
// return registers.
struct Reg_Seq_Index
{
    s32 general_reg;
    s32 float_reg;
    s32 total_arg_count;
    s32 stack_arg_count;
};

struct Reg_Class
{
    Array<Reg> return_regs;
    Array<Reg> arg_regs;
    Array<Reg> free_regs;
};

struct Reg_Alloc
{
    s64 reg_count;
    const Reg_Info *reg_info;

    Reg_Class general_regs;
    Reg_Class float_regs;

    // Specifies if argument index is shared between arguments passed in
    // general registers and float registers.
    b32 arg_index_shared;

    // Specifies how many argument registers need backing in stack shadow space.
    s32 shadow_arg_reg_count;

    // An array of all "spills" that need to be inserted in to the code after
    // register allocation.
    Array<Spill_Info> spills;

    // Used to determine callee save registers used by a routine.
    u64 dirty_regs; // NOTE(henrik): Assumes that total register count < 64
};

void InitRegAlloc(Reg_Alloc *reg_alloc,
        s64 total_reg_count, const Reg_Info *reg_info,
        b32 arg_index_shared, s32 shadow_arg_reg_count);

void FreeRegAlloc(Reg_Alloc *reg_alloc);

void ResetRegAlloc(Reg_Alloc *reg_alloc, b32 use_callee_saves_first = true);
void DirtyRegister(Reg_Alloc *reg_alloc, Reg reg);
b32 IsRegisterDirty(Reg_Alloc *reg_alloc, Reg reg);

b32 IsCallerSave(Reg_Alloc *reg_alloc, Reg reg);
b32 IsCalleeSave(Reg_Alloc *reg_alloc, Reg reg);
b32 IsFloatRegister(Reg_Alloc *reg_alloc, Reg reg);

b32 HasFreeRegisters(Reg_Alloc *reg_alloc, Oper_Data_Type data_type);
Reg AllocateFreeRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type);
void AllocateRegister(Reg_Alloc *reg_alloc, Reg reg, Oper_Data_Type data_type);
b32 TryAllocateRegister(Reg_Alloc *reg_alloc, Reg reg, Oper_Data_Type data_type);
void ReleaseRegister(Reg_Alloc *reg_alloc, Reg reg, Oper_Data_Type data_type);

const Reg* GetReturnRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type, s64 ret_index);
const Reg* GetArgRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type, Reg_Seq_Index *arg_index);

s64 GetArgStackAllocSize(Reg_Alloc *reg_alloc, Reg_Seq_Index arg_index);
s64 GetOffsetFromBasePointer(Reg_Alloc *reg_alloc, Reg_Seq_Index arg_index);
s64 GetOffsetFromStackPointer(Reg_Alloc *reg_alloc, Reg_Seq_Index arg_index);

// The function adds a spill to Reg_Alloc::spills.
void Spill(Reg_Alloc *reg_alloc, Live_Interval interval,
        s64 instr_index, s64 bias = 0, const char *note = nullptr);
// The function adds a fill to Reg_Alloc::spills.
void Unspill(Reg_Alloc *reg_alloc, Live_Interval interval,
        s64 instr_index, s64 bias = 0, const char *note = nullptr);
// The function adds a move to Reg_Alloc::spills.
void Move(Reg_Alloc *reg_alloc, Live_Interval interval, Reg target,
        s64 instr_index, const char *note = nullptr);

} // hplang

#define H_HPLANG_REG_ALLOC_H
#endif
