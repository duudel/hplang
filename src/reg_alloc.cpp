
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
        s64 total_reg_count, const Reg_Info *reg_info)
{
    *reg_alloc = { };
    reg_alloc->reg_count = total_reg_count;
    reg_alloc->reg_info = reg_info;

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
}

void ResetRegAlloc(Reg_Alloc *reg_alloc)
{
    array::Clear(reg_alloc->spills);
    array::Clear(reg_alloc->free_regs);
    array::Clear(reg_alloc->free_float_regs);

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


const Reg* GetReturnRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type, s64 ret_index)
{
    if (data_type == Oper_Data_Type::F32 ||
        data_type == Oper_Data_Type::F64)
    {
        if (ret_index < reg_alloc->float_regs.return_regs.count)
            return &reg_alloc->general_regs.return_regs[ret_index];
    }
    else
    {
        if (ret_index < reg_alloc->general_regs.return_regs.count)
            return &reg_alloc->general_regs.return_regs[ret_index];
    }
    return nullptr;
}


const Reg* GetArgRegister(Reg_Alloc *reg_alloc, Oper_Data_Type data_type, s64 arg_index)
{
    if (data_type == Oper_Data_Type::F32 ||
        data_type == Oper_Data_Type::F64)
    {
        if (arg_index < reg_alloc->float_regs.arg_regs.count)
            return &reg_alloc->float_regs.arg_regs[arg_index];
    }
    else
    {
        if (arg_index < reg_alloc->general_regs.arg_regs.count)
            return &reg_alloc->general_regs.arg_regs[arg_index];
    }
    return nullptr;
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
    switch (data_type)
    {
    case Oper_Data_Type::F32:
    case Oper_Data_Type::F64:
        return GetFreeFloatRegister(reg_alloc);
    default:
        return GetFreeGeneralRegister(reg_alloc);
    }
}

void ReleaseRegister(Reg_Alloc *reg_alloc, Reg reg, Oper_Data_Type data_type)
{
    switch (data_type)
    {
    case Oper_Data_Type::F32:
    case Oper_Data_Type::F64:
        array::Push(reg_alloc->free_float_regs, reg);
        break;
    default:
        array::Push(reg_alloc->free_regs, reg);
        break;
    }
}


#if 0
void ClearRegAllocs(Reg_Alloc *reg_alloc)
{
    array::Clear(reg_alloc->mapped_regs);
    array::Clear(reg_alloc->free_regs);
    array::Clear(reg_alloc->free_float_regs);

    array::Reserve(reg_alloc->free_regs, reg_alloc->general_reg_count);
    for (s64 i = 0; i < reg_alloc->general_reg_count; i++)
    {
        if (IsCalleeSave(reg_alloc, reg_alloc->general_regs[i]))
            array::Push(reg_alloc->free_regs, reg_alloc->general_regs[i]);
    }
    for (s64 i = 0; i < reg_alloc->general_reg_count; i++)
    {
        if (IsCallerSave(reg_alloc, reg_alloc->general_regs[i]))
            array::Push(reg_alloc->free_regs, reg_alloc->general_regs[i]);
    }
    array::Reserve(reg_alloc->free_float_regs, reg_alloc->float_reg_count);
    for (s64 i = 0; i < reg_alloc->float_reg_count; i++)
    {
        if (IsCalleeSave(reg_alloc, reg_alloc->general_regs[i]))
            array::Push(reg_alloc->free_float_regs, reg_alloc->float_regs[i]);
    }
    for (s64 i = 0; i < reg_alloc->float_reg_count; i++)
    {
        if (IsCallerSave(reg_alloc, reg_alloc->general_regs[i]))
            array::Push(reg_alloc->free_float_regs, reg_alloc->float_regs[i]);
    }
}

void DirtyCalleeSaveRegs(Reg_Alloc *reg_alloc)
{
    for (s64 i = 0; i < reg_alloc->reg_flags.count; i++)
    {
        u8 &flag = reg_alloc->reg_flags[i];
        if ((flag & RF_CallerSave) == 0)
            flag |= (RF_Dirty | RF_CalleeSaveDirty);
    }
}

void DirtyRegister(Reg_Alloc *reg_alloc, Reg reg)
{
    reg_alloc->reg_flags[reg.reg_index] |= RF_Dirty;
}

b32 UndirtyRegister(Reg_Alloc *reg_alloc, Reg reg)
{
    u8 &flag = reg_alloc->reg_flags[reg.reg_index];
    if ((flag & RF_Dirty) != 0)
    {
        flag &= ~RF_Dirty;
        return true;
    }
    return false;
}

b32 UndirtyCalleeSave(Reg_Alloc *reg_alloc, Reg reg)
{
    u8 &flag = reg_alloc->reg_flags[reg.reg_index];
    if ((flag & RF_CalleeSaveDirty) != 0)
    {
        flag &= ~RF_CalleeSaveDirty;
        return true;
    }
    return false;
}

void MapRegister(Reg_Alloc *reg_alloc, Name name, Reg reg, Oper_Data_Type data_type)
{
    for (s64 i = 0; i < reg_alloc->mapped_regs.count; i++)
    {
        Reg_Var *reg_var = &reg_alloc->mapped_regs[i];
        if (reg_var->var_name == name)
        {
            reg_var->reg = reg;
            ASSERT(reg_var->data_type == data_type);
            return;
        }
    }
    Reg_Var reg_var = { };
    reg_var.var_name = name;
    reg_var.reg = reg;
    reg_var.data_type = data_type;
    array::Push(reg_alloc->mapped_regs, reg_var);
}

void UnmapRegister(Reg_Alloc *reg_alloc, Reg reg)
{
    s64 idx = 0;
    for (; idx < reg_alloc->mapped_regs.count; idx++)
    {
        Reg r = reg_alloc->mapped_regs[idx].reg;
        if (r == reg)
            break;
    }
    ASSERT(idx < reg_alloc->mapped_regs.count);
    for (s64 i = idx; i < reg_alloc->mapped_regs.count - 1; i++)
    {
        Reg_Var rv = array::At(reg_alloc->mapped_regs, i + 1);
        array::Set(reg_alloc->mapped_regs, i, rv);
    }
    reg_alloc->mapped_regs.count--;
}

const Reg* GetMappedRegister(Reg_Alloc *reg_alloc, Name name)
{
    for (s64 i = 0; i < reg_alloc->mapped_regs.count; i++)
    {
        const Reg_Var *reg_var = &reg_alloc->mapped_regs.data[i];
        if (reg_var->var_name == name)
        {
            return &reg_var->reg;
        }
    }
    return nullptr;
}

const Reg_Var* GetMappedVar(Reg_Alloc *reg_alloc, Reg reg)
{
    for (s64 i = 0; i < reg_alloc->mapped_regs.count; i++)
    {
        const Reg_Var *reg_var = &reg_alloc->mapped_regs[i];
        if (reg_var->reg == reg)
        {
            return reg_var;
        }
    }
    return nullptr;
}

static Reg FreeRegister(Reg_Alloc *reg_alloc)
{
    s64 result_idx = 0;
    Reg_Var result = array::At(reg_alloc->mapped_regs, result_idx);
    if ((reg_alloc->reg_flags[result.reg.reg_index] & RF_FloatReg) != 0)
    {
        for (result_idx = 1; result_idx < reg_alloc->mapped_regs.count; result_idx++)
        {
            result = reg_alloc->mapped_regs[result_idx];
            u8 flag = reg_alloc->reg_flags[result.reg.reg_index];
            if ((flag & RF_FloatReg) == 0)
                break;
        }
        ASSERT(result_idx < reg_alloc->mapped_regs.count);
    }
    //for (s64 i = result_idx; i < reg_alloc->mapped_regs.count - 1; i++)
    //{
    //    Reg_Var rv = array::At(reg_alloc->mapped_regs, i + 1);
    //    array::Set(reg_alloc->mapped_regs, i, rv);
    //}
    //reg_alloc->mapped_regs.count--;
    return result.reg;
}

static Reg FreeFloatRegister(Reg_Alloc *reg_alloc)
{
    s64 result_idx = 0;
    Reg_Var result = array::At(reg_alloc->mapped_regs, result_idx);
    if ((reg_alloc->reg_flags[result.reg.reg_index] & RF_FloatReg) == 0)
    {
        for (result_idx = 1; result_idx < reg_alloc->mapped_regs.count; result_idx++)
        {
            result = reg_alloc->mapped_regs[result_idx];
            u8 flag = reg_alloc->reg_flags[result.reg.reg_index];
            if ((flag & RF_FloatReg) != 0)
                break;
        }
        ASSERT(result_idx < reg_alloc->mapped_regs.count);
    }
    //for (s64 i = result_idx; i < reg_alloc->mapped_regs.count - 1; i++)
    //{
    //    Reg_Var rv = array::At(reg_alloc->mapped_regs, i + 1);
    //    array::Set(reg_alloc->mapped_regs, i, rv);
    //}
    //reg_alloc->mapped_regs.count--;
    return result.reg;
}
#endif

} // hplang
