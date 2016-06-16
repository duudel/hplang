
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

    array::Reserve(reg_alloc->free_regs, general_reg_count);
    array::Reserve(reg_alloc->free_float_regs, float_reg_count);
}

void FreeRegAlloc(Reg_Alloc *reg_alloc)
{
    array::Free(reg_alloc->free_regs);
    array::Free(reg_alloc->free_float_regs);
    array::Free(reg_alloc->spills);

    array::Free(reg_alloc->general_regs.return_regs);
    array::Free(reg_alloc->general_regs.arg_regs);
    array::Free(reg_alloc->float_regs.return_regs);
    array::Free(reg_alloc->float_regs.arg_regs);
}

void ResetRegAlloc(Reg_Alloc *reg_alloc, b32 use_first_callee_saves /*= true*/)
{
    array::Clear(reg_alloc->spills);
    array::Clear(reg_alloc->free_regs);
    array::Clear(reg_alloc->free_float_regs);

    reg_alloc->dirty_regs = 0;

//#define USE_CALLER_SAVES_FIRST
//#ifdef USE_CALLER_SAVES_FIRST
if (!use_first_callee_saves)
{
    // Add callee saves to free regs
    for (s64 i = 0; i < reg_alloc->reg_count; i++)
    {
        Reg_Info reg_info = reg_alloc->reg_info[i];
        if ((reg_info.reg_flags & RF_NonAllocable) != 0)
            continue;

        if ((reg_info.reg_flags & RF_CallerSave) == 0)
        {
            Reg reg = { reg_info.reg_index };
            if ((reg_info.reg_flags & RF_Float) != 0)
                array::Push(reg_alloc->free_float_regs, reg);
            else
                array::Push(reg_alloc->free_regs, reg);
        }
    }
}
//#endif
    // Add caller saves to free regs
    for (s64 i = 0; i < reg_alloc->reg_count; i++)
    {
        Reg_Info reg_info = reg_alloc->reg_info[i];
        if ((reg_info.reg_flags & RF_NonAllocable) != 0)
            continue;

        if ((reg_info.reg_flags & RF_CallerSave) != 0)
        {
            Reg reg = { reg_info.reg_index };
            if ((reg_info.reg_flags & RF_Float) != 0)
                array::Push(reg_alloc->free_float_regs, reg);
            else
                array::Push(reg_alloc->free_regs, reg);
        }
    }
//#ifndef USE_CALLER_SAVES_FIRST
if (use_first_callee_saves)
{
    // Add callee saves to free regs
    for (s64 i = 0; i < reg_alloc->reg_count; i++)
    {
        Reg_Info reg_info = reg_alloc->reg_info[i];
        if ((reg_info.reg_flags & RF_NonAllocable) != 0)
            continue;

        if ((reg_info.reg_flags & RF_CallerSave) == 0)
        {
            Reg reg = { reg_info.reg_index };
            if ((reg_info.reg_flags & RF_Float) != 0)
                array::Push(reg_alloc->free_float_regs, reg);
            else
                array::Push(reg_alloc->free_regs, reg);
        }
    }
}
//#endif
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
        return reg_alloc->free_float_regs.count > 0;
    return reg_alloc->free_regs.count > 0;
}

static Reg GetFreeGeneralRegister(Reg_Alloc *reg_alloc)
{
    if (reg_alloc->free_regs.count == 0)
    {
        return { };
    }
    Reg reg = array::At(reg_alloc->free_regs, reg_alloc->free_regs.count - 1);
    reg_alloc->free_regs.count--;
    return reg;
}

static Reg GetFreeFloatRegister(Reg_Alloc *reg_alloc)
{
    if (reg_alloc->free_float_regs.count == 0)
    {
        return { };
    }
    Reg reg = array::At(reg_alloc->free_float_regs, reg_alloc->free_float_regs.count - 1);
    reg_alloc->free_float_regs.count--;
    return reg;
}

Reg GetFreeRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type)
{
    if (DataTypeIsFloat(data_type))
        return GetFreeFloatRegister(reg_alloc);
    return GetFreeGeneralRegister(reg_alloc);
}

void ReleaseRegister(Reg_Alloc *reg_alloc, Reg reg, Oper_Data_Type data_type)
{
    if (DataTypeIsFloat(data_type))
        array::Push(reg_alloc->free_float_regs, reg);
    else
        array::Push(reg_alloc->free_regs, reg);
}

} // hplang
