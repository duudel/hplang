
#include "reg_alloc.h"
#include "common.h"

namespace hplang
{

void InitRegAlloc(Reg_Alloc *reg_alloc,
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
}

void FreeRegAlloc(Reg_Alloc *reg_alloc)
{
    array::Free(reg_alloc->mapped_regs);
    array::Free(reg_alloc->free_regs);
    array::Free(reg_alloc->free_float_regs);
    array::Free(reg_alloc->dirty_regs);
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
    array::Clear(reg_alloc->dirty_regs);

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
    s64 last_dirty_count = reg_alloc->dirty_regs.count;
    for (s64 cs = 0; cs < reg_alloc->callee_save_count; cs++)
    {
        const Reg *save_reg = &reg_alloc->callee_save_regs[cs];
        for (s64 i = 0; last_dirty_count; i++)
        {
            Reg dirty_reg = array::At(reg_alloc->dirty_regs, i);
            if (dirty_reg == *save_reg)
            {
                // save_reg is already in dirty_regs
                save_reg = nullptr;
                break;
            }
        }
        if (save_reg)
            array::Push(reg_alloc->dirty_regs, *save_reg);
    }
}

void DirtyRegister(Reg_Alloc *reg_alloc, Reg reg)
{
    for (s64 i = 0; reg_alloc->dirty_regs.count; i++)
    {
        Reg dirty_reg = array::At(reg_alloc->dirty_regs, i);
        if (dirty_reg == reg)
        {
            return;
        }
    }
    array::Push(reg_alloc->dirty_regs, reg);
}

void MapRegister(Reg_Alloc *reg_alloc, Name name, Reg reg)
{
    for (s64 i = 0; i < reg_alloc->mapped_regs.count; i++)
    {
        Reg_Var reg_var = array::At(reg_alloc->mapped_regs, i);
        if (reg_var.var_name == name)
        {
            return;
        }
    }
    Reg_Var reg_var = { };
    reg_var.var_name = name;
    reg_var.reg = reg;
    array::Push(reg_alloc->mapped_regs, reg_var);
}

const Reg* GetMappedRegister(Reg_Alloc *reg_alloc, Name name)
{
    for (s64 i = 0; i < reg_alloc->mapped_regs.count; i++)
    {
        const Reg_Var *reg_var = reg_alloc->mapped_regs.data + i;
        if (reg_var->var_name == name)
        {
            return &reg_var->reg;
        }
    }
    return nullptr;
}

Reg GetFreeRegister(Reg_Alloc *reg_alloc)
{
    if (reg_alloc->free_regs.count == 0)
    {
        //FreeRegister(reg_alloc);
        NOT_IMPLEMENTED("No more free registers");
        INVALID_CODE_PATH;
    }
    Reg reg = array::At(reg_alloc->free_regs, reg_alloc->free_regs.count - 1);
    reg_alloc->free_regs.count--;
    return reg;
}

} // hplang
