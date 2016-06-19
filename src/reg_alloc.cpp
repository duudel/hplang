
#include "reg_alloc.h"
#include "common.h"
#include "symbols.h"

namespace hplang
{

static void ReorderIndexedRegs(Array<Reg> &regs, const Reg_Info *reg_info)
{
    for (s64 i = 0; i < regs.count; )
    {
        Reg reg = regs[i];
        s64 index = reg_info[reg.reg_index].index;
        ASSERT(index >= 0 && index < regs.count);

        if (i == index)
        {
            i++;
            continue;
        }
        regs[i] = regs[index];
        regs[index] = reg;
    }
}

void InitRegAlloc(Reg_Alloc *reg_alloc,
        s64 total_reg_count, const Reg_Info *reg_info,
        b32 arg_index_shared, s32 shadow_arg_reg_count)
{
    *reg_alloc = { };
    reg_alloc->reg_count = total_reg_count;
    reg_alloc->reg_info = reg_info;
    reg_alloc->arg_index_shared = arg_index_shared;
    reg_alloc->shadow_arg_reg_count = shadow_arg_reg_count;

    s64 general_reg_count = 0;
    s64 float_reg_count = 0;
    for (s64 i = 0; i < total_reg_count; i++)
    {
        Reg_Info reg_info = reg_alloc->reg_info[i];
        ASSERT(reg_info.reg_index == i);

        if ((reg_info.reg_flags & RF_NonAllocable) == 0)
        {
            Reg reg = { reg_info.reg_index };
            if ((reg_info.reg_flags & RF_Float) == 0)
            {
                general_reg_count++;
                if ((reg_info.reg_flags & RF_Return) != 0)
                    array::Push(reg_alloc->general_regs.return_regs, reg);
                if ((reg_info.reg_flags & RF_Arg) != 0)
                    array::Push(reg_alloc->general_regs.arg_regs, reg);
            }
            else
            {
                float_reg_count++;
                if ((reg_info.reg_flags & RF_Return) != 0)
                    array::Push(reg_alloc->float_regs.return_regs, reg);
                if ((reg_info.reg_flags & RF_Arg) != 0)
                    array::Push(reg_alloc->float_regs.arg_regs, reg);
            }
        }
    }

    ReorderIndexedRegs(reg_alloc->general_regs.return_regs, reg_info);
    ReorderIndexedRegs(reg_alloc->general_regs.arg_regs, reg_info);
    ReorderIndexedRegs(reg_alloc->float_regs.return_regs, reg_info);
    ReorderIndexedRegs(reg_alloc->float_regs.arg_regs, reg_info);

    array::Reserve(reg_alloc->general_regs.free_regs, general_reg_count);
    array::Reserve(reg_alloc->float_regs.free_regs, float_reg_count);
}

void FreeRegAlloc(Reg_Alloc *reg_alloc)
{
    array::Free(reg_alloc->spills);

    array::Free(reg_alloc->general_regs.return_regs);
    array::Free(reg_alloc->general_regs.arg_regs);
    array::Free(reg_alloc->general_regs.free_regs);
    array::Free(reg_alloc->float_regs.return_regs);
    array::Free(reg_alloc->float_regs.arg_regs);
    array::Free(reg_alloc->float_regs.free_regs);
}

static void AppendFreeRegs(Reg_Alloc *reg_alloc, u8 reg_flag_mask, u8 reg_flag_result)
{
    for (s64 i = 0; i < reg_alloc->reg_count; i++)
    {
        Reg_Info reg_info = reg_alloc->reg_info[i];
        if ((reg_info.reg_flags & RF_NonAllocable) != 0)
            continue;

        if ((reg_info.reg_flags & reg_flag_mask) == reg_flag_result)
        {
            Reg reg = { reg_info.reg_index };
            if ((reg_info.reg_flags & RF_Float) != 0)
                array::Push(reg_alloc->float_regs.free_regs, reg);
            else
                array::Push(reg_alloc->general_regs.free_regs, reg);
        }
    }
}

void ResetRegAlloc(Reg_Alloc *reg_alloc, b32 use_callee_saves_first /*= true*/)
{
    array::Clear(reg_alloc->spills);
    array::Clear(reg_alloc->general_regs.free_regs);
    array::Clear(reg_alloc->float_regs.free_regs);

    reg_alloc->dirty_regs = 0;

    if (use_callee_saves_first)
    {
        // Add caller saves to free regs
        AppendFreeRegs(reg_alloc, RF_CallerSave, RF_CallerSave);
        // Add callee saves to free regs
        AppendFreeRegs(reg_alloc, RF_CallerSave, 0);
    }
    else
    {
        // Add callee saves to free regs
        AppendFreeRegs(reg_alloc, RF_CallerSave, 0);
        // Add caller saves to free regs
        AppendFreeRegs(reg_alloc, RF_CallerSave, RF_CallerSave);
    }
}

void DirtyRegister(Reg_Alloc *reg_alloc, Reg reg)
{
    reg_alloc->dirty_regs |= (1ull << reg.reg_index);
}

b32 IsRegisterDirty(Reg_Alloc *reg_alloc, Reg reg)
{
    return (reg_alloc->dirty_regs & (1ull << reg.reg_index)) != 0;
}

b32 IsCallerSave(Reg_Alloc *reg_alloc, Reg reg)
{
    return (reg_alloc->reg_info[reg.reg_index].reg_flags & RF_CallerSave) != 0;
}

b32 IsCalleeSave(Reg_Alloc *reg_alloc, Reg reg)
{
    return (reg_alloc->reg_info[reg.reg_index].reg_flags & RF_CallerSave) == 0;
}

b32 IsFloatRegister(Reg_Alloc *reg_alloc, Reg reg)
{
    return (reg_alloc->reg_info[reg.reg_index].reg_flags & RF_Float) != 0;
}


static b32 DataTypeIsFloat(Oper_Data_Type data_type)
{
    return (data_type == Oper_Data_Type::F32) ||
           (data_type == Oper_Data_Type::F64);
}

const Reg* GetReturnRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type, s64 ret_index)
{
    if (DataTypeIsFloat(data_type))
    {
        if (ret_index < reg_alloc->float_regs.return_regs.count)
            return &reg_alloc->float_regs.return_regs[ret_index];
    }
    else
    {
        if (ret_index < reg_alloc->general_regs.return_regs.count)
            return &reg_alloc->general_regs.return_regs[ret_index];
    }
    return nullptr;
}

static void AdvanceArgIndex(Reg_Alloc *reg_alloc,
        Oper_Data_Type data_type, Reg_Seq_Index *arg_index, bool arg_passed_in_reg)
{
    arg_index->total_arg_count++;
    if (!arg_passed_in_reg)
    {
        arg_index->stack_arg_count++;
        return;
    }

    if (reg_alloc->arg_index_shared)
    {
        arg_index->general_reg++;
        arg_index->float_reg++;
    }
    else
    {
        if (DataTypeIsFloat(data_type))
        {
            arg_index->float_reg++;
        }
        else
        {
            arg_index->general_reg++;
        }
    }
}

enum {
    WORD_SIZE = 8
};

s64 GetArgStackAllocSize(Reg_Alloc *reg_alloc, Reg_Seq_Index arg_index)
{
    s32 stack_arg_count = reg_alloc->shadow_arg_reg_count;
    stack_arg_count += arg_index.stack_arg_count;
    return stack_arg_count * WORD_SIZE;
}

s64 GetOffsetFromBasePointer(Reg_Alloc *reg_alloc, Reg_Seq_Index arg_index)
{
    s64 stack_slots = 0;
    if (reg_alloc->shadow_arg_reg_count)
    {
        stack_slots += arg_index.total_arg_count;
    }
    else
    {
        stack_slots += arg_index.stack_arg_count;
    }
    // NOTE(henrik): Return -1 when there is no local offset for the argument.
    if (stack_slots == 0) return -1;

    stack_slots += 2 - 1; // old rbp, return address
    return stack_slots * WORD_SIZE;
}

s64 GetOffsetFromStackPointer(Reg_Alloc *reg_alloc, Reg_Seq_Index arg_index)
{
    s64 stack_slots = 0;
    if (reg_alloc->shadow_arg_reg_count)
    {
        stack_slots += arg_index.total_arg_count;
    }
    else
    {
        stack_slots += arg_index.stack_arg_count;
    }
    // NOTE(henrik): Return -1 when there is no local offset for the argument.
    if (stack_slots == 0) return -1;

    stack_slots -= 1; // old rbp, return address
    return stack_slots * WORD_SIZE;
}

const Reg* GetArgRegister(Reg_Alloc *reg_alloc,
        Oper_Data_Type data_type, Reg_Seq_Index *arg_index)
{
    if (DataTypeIsFloat(data_type))
    {
        s32 index = arg_index->float_reg;
        if (index < reg_alloc->float_regs.arg_regs.count)
        {
            AdvanceArgIndex(reg_alloc, data_type, arg_index, true);
            return &reg_alloc->float_regs.arg_regs[index];
        }
    }
    else
    {
        s32 index = arg_index->general_reg;
        if (index < reg_alloc->general_regs.arg_regs.count)
        {
            AdvanceArgIndex(reg_alloc, data_type, arg_index, true);
            return &reg_alloc->general_regs.arg_regs[index];
        }
    }
    AdvanceArgIndex(reg_alloc, data_type, arg_index, false);
    return nullptr;
}


b32 HasFreeRegisters(Reg_Alloc *reg_alloc, Oper_Data_Type data_type)
{
    if (DataTypeIsFloat(data_type))
        return reg_alloc->float_regs.free_regs.count > 0;
    return reg_alloc->general_regs.free_regs.count > 0;
}

static Reg GetFreeGeneralRegister(Reg_Alloc *reg_alloc)
{
    if (reg_alloc->general_regs.free_regs.count == 0)
    {
        return { };
    }
    Reg reg = array::Back(reg_alloc->general_regs.free_regs);
    array::Pop(reg_alloc->general_regs.free_regs);
    return reg;
}

static Reg GetFreeFloatRegister(Reg_Alloc *reg_alloc)
{
    if (reg_alloc->float_regs.free_regs.count == 0)
    {
        return { };
    }
    Reg reg = array::Back(reg_alloc->float_regs.free_regs);
    array::Pop(reg_alloc->float_regs.free_regs);
    return reg;
}

Reg AllocateFreeRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type)
{
    if (DataTypeIsFloat(data_type))
        return GetFreeFloatRegister(reg_alloc);
    else
        return GetFreeGeneralRegister(reg_alloc);
}

static bool TryRemoveFromFreeRegs(Array<Reg> &free_regs, Reg reg)
{
    for (s64 i = 0; i < free_regs.count; i++)
    {
        if (free_regs[i] == reg)
        {
            array::EraseBySwap(free_regs, i);
            return true;
        }
    }
    return false;
}

void AllocateRegister(Reg_Alloc *reg_alloc, Reg reg, Oper_Data_Type data_type)
{
    ASSERT(TryAllocateRegister(reg_alloc, reg, data_type) == true);
}

b32 TryAllocateRegister(Reg_Alloc *reg_alloc, Reg reg, Oper_Data_Type data_type)
{
    if (DataTypeIsFloat(data_type))
    {
        ASSERT(IsFloatRegister(reg_alloc, reg));
        return TryRemoveFromFreeRegs(reg_alloc->float_regs.free_regs, reg);
    }
    else
    {
        ASSERT(!IsFloatRegister(reg_alloc, reg));
        return TryRemoveFromFreeRegs(reg_alloc->general_regs.free_regs, reg);
    }
}

void ReleaseRegister(Reg_Alloc *reg_alloc, Reg reg, Oper_Data_Type data_type)
{
    if (DataTypeIsFloat(data_type))
    {
        ASSERT(IsFloatRegister(reg_alloc, reg));
        array::Push(reg_alloc->float_regs.free_regs, reg);
    }
    else
    {
        ASSERT(!IsFloatRegister(reg_alloc, reg));
        array::Push(reg_alloc->general_regs.free_regs, reg);
    }
}

void Spill(Reg_Alloc *reg_alloc, Live_Interval interval, 
        s64 instr_index, s64 bias /*= 0*/,
        const char *note /*= nullptr*/)
{
    ASSERT(instr_index >= 0);
    Spill_Info spill_info = { };
    spill_info.note = note;
    spill_info.interval = interval;
    spill_info.instr_index = instr_index * 2 + bias;
    spill_info.spill_type = Spill_Type::Spill;
    array::Push(reg_alloc->spills, spill_info);
}

void Unspill(Reg_Alloc *reg_alloc, Live_Interval interval,
        s64 instr_index, s64 bias /*= 0*/,
        const char *note /*= nullptr*/)
{
    ASSERT(instr_index >= 0);
    Spill_Info spill_info = { };
    spill_info.note = note;
    spill_info.interval = interval;
    spill_info.instr_index = instr_index * 2 + bias;
    spill_info.spill_type = Spill_Type::Unspill;
    array::Push(reg_alloc->spills, spill_info);
}

void Move(Reg_Alloc *reg_alloc, Live_Interval interval, Reg target,
        s64 instr_index,
        const char *note /*= nullptr*/)
{
    ASSERT(instr_index >= 0);
    Spill_Info spill_info = { };
    spill_info.note = note;
    spill_info.interval = interval;
    spill_info.target = target;
    spill_info.instr_index = instr_index * 2;
    spill_info.spill_type = Spill_Type::Move;
    array::Push(reg_alloc->spills, spill_info);
}

} // hplang
