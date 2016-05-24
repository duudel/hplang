
#include "reg_alloc.h"
#include "common.h"
#include "symbols.h"

namespace hplang
{

enum Reg_Flag
{
    RF_ArgReg           = 1,
    RF_FloatReg         = 2,
    RF_CallerSave       = 4,

    RF_Dirty            = 8,
    RF_CalleeSaveDirty  = 16,
};

void InitRegAlloc(Reg_Alloc *reg_alloc,
        s64 total_reg_count,
        s64 general_reg_count, const Reg *general_regs,
        s64 float_reg_count, const Reg *float_regs,
        s64 arg_reg_count, const Reg *arg_regs,
        s64 float_arg_reg_count, const Reg *float_arg_regs,
        s64 caller_save_count, const Reg *caller_save_regs,
        s64 callee_save_count, const Reg *callee_save_regs)
{
    *reg_alloc = { };
    reg_alloc->general_reg_count = general_reg_count;
    reg_alloc->general_regs = general_regs;
    reg_alloc->float_reg_count = float_reg_count;
    reg_alloc->float_regs = float_regs;
    reg_alloc->arg_reg_count = arg_reg_count;
    reg_alloc->arg_regs = arg_regs;
    reg_alloc->float_arg_reg_count = float_arg_reg_count;
    reg_alloc->float_arg_regs = float_arg_regs;
    reg_alloc->caller_save_count = caller_save_count;
    reg_alloc->caller_save_regs = caller_save_regs;
    reg_alloc->callee_save_count = callee_save_count;
    reg_alloc->callee_save_regs = callee_save_regs;

    total_reg_count += 1; // REG_NONE
    array::Resize(reg_alloc->reg_flags, total_reg_count); // the memory is zeroed
    for (s64 i = 0; i < float_reg_count; i++)
    {
        Reg reg = float_regs[i];
        u8 &flag = reg_alloc->reg_flags[reg.reg_index];
        flag |= RF_FloatReg;
    }
    for (s64 i = 0; i < arg_reg_count; i++)
    {
        Reg reg = arg_regs[i];
        u8 &flag = reg_alloc->reg_flags[reg.reg_index];
        flag |= RF_ArgReg;
    }
    for (s64 i = 0; i < float_arg_reg_count; i++)
    {
        Reg reg = float_arg_regs[i];
        u8 &flag = reg_alloc->reg_flags[reg.reg_index];
        flag |= RF_ArgReg;
    }
    for (s64 i = 0; i < caller_save_count; i++)
    {
        Reg reg = caller_save_regs[i];
        u8 &flag = reg_alloc->reg_flags[reg.reg_index];
        flag |= RF_CallerSave;
    }
}

void FreeRegAlloc(Reg_Alloc *reg_alloc)
{
    array::Free(reg_alloc->mapped_regs);
    array::Free(reg_alloc->free_regs);
    array::Free(reg_alloc->free_float_regs);
    array::Free(reg_alloc->reg_flags);
}


b32 IsCallerSave(Reg_Alloc *reg_alloc, Reg reg)
{
    return (reg_alloc->reg_flags[reg.reg_index] & RF_CallerSave) != 0;
}

b32 IsCalleeSave(Reg_Alloc *reg_alloc, Reg reg)
{
    return (reg_alloc->reg_flags[reg.reg_index] & RF_CallerSave) == 0;
}

b32 IsFloatRegister(Reg_Alloc *reg_alloc, Reg reg)
{
    return (reg_alloc->reg_flags[reg.reg_index] & RF_FloatReg) != 0;
}

const Reg* GetArgRegister(Reg_Alloc *reg_alloc, s64 arg_index)
{
    if (arg_index < reg_alloc->arg_reg_count)
    {
        return reg_alloc->arg_regs + arg_index;
    }
    return nullptr;
}

const Reg* GetFloatArgRegister(Reg_Alloc *reg_alloc, s64 arg_index)
{
    if (arg_index < reg_alloc->float_arg_reg_count)
    {
        return reg_alloc->float_arg_regs + arg_index;
    }
    return nullptr;
}

void ClearRegAllocs(Reg_Alloc *reg_alloc)
{
    array::Clear(reg_alloc->mapped_regs);
    array::Clear(reg_alloc->free_regs);
    array::Clear(reg_alloc->free_float_regs);

    array::Reserve(reg_alloc->free_regs, reg_alloc->general_reg_count);
    for (s64 i = 0; i < reg_alloc->general_reg_count; i++)
    {
        array::Push(reg_alloc->free_regs, reg_alloc->general_regs[i]);
    }
    array::Reserve(reg_alloc->free_float_regs, reg_alloc->float_reg_count);
    for (s64 i = 0; i < reg_alloc->float_reg_count; i++)
    {
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
        Reg_Var reg_var = array::At(reg_alloc->mapped_regs, i);
        if (reg_var.var_name == name)
        {
            ASSERT(reg_var.data_type == data_type);
            return;
        }
    }
    Reg_Var reg_var = { };
    reg_var.var_name = name;
    reg_var.reg = reg;
    reg_var.data_type = data_type;
    array::Push(reg_alloc->mapped_regs, reg_var);
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

static Reg GetFreeGeneralRegister(Reg_Alloc *reg_alloc)
{
    if (reg_alloc->free_regs.count == 0)
    {
        return FreeRegister(reg_alloc);
    }
    Reg reg = array::At(reg_alloc->free_regs, reg_alloc->free_regs.count - 1);
    reg_alloc->free_regs.count--;
    return reg;
}

static Reg GetFreeFloatRegister(Reg_Alloc *reg_alloc)
{
    if (reg_alloc->free_float_regs.count == 0)
    {
        return FreeFloatRegister(reg_alloc);
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

} // hplang
