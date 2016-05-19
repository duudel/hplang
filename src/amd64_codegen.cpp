
#include "amd64_codegen.h"
#include "common.h"
#include "symbols.h"

#include <cstdio>
#include <cinttypes>

namespace hplang
{

#define OPCODES\
    PASTE_OP(LABEL)\
    \
    PASTE_OP(call)\
    PASTE_OP(ret)\
    PASTE_OP(jmp)\
    PASTE_OP(je)\
    PASTE_OP(jne)\
    PASTE_OP(jb)\
    PASTE_OP(jbe)\
    PASTE_OP(ja)\
    PASTE_OP(jae)\
    PASTE_OP(jl)\
    PASTE_OP(jle)\
    PASTE_OP(jg)\
    PASTE_OP(jge)\
    \
    PASTE_OP(cmp)\
    PASTE_OP(comiss)\
    PASTE_OP(comisd)\
    \
    PASTE_OP(lea)\
    PASTE_OP(mov)\
    PASTE_OP(movss)\
    PASTE_OP(movsd)\
    \
    PASTE_OP(cmove)\
    PASTE_OP(cmovne)\
    PASTE_OP(cmova)\
    PASTE_OP(cmovae)\
    PASTE_OP(cmovb)\
    PASTE_OP(cmovbe)\
    PASTE_OP(cmovl)\
    PASTE_OP(cmovle)\
    PASTE_OP(cmovg)\
    PASTE_OP(cmovge)\
    \
    PASTE_OP(add)\
    PASTE_OP(sub)\
    PASTE_OP(mul)\
    PASTE_OP(imul)\
    PASTE_OP(div)\
    PASTE_OP(idiv)\
    PASTE_OP(and)\
    PASTE_OP(or)\
    PASTE_OP(xor)\
    PASTE_OP(neg)\
    PASTE_OP(not)\
    \
    PASTE_OP(addss)\
    PASTE_OP(subss)\
    PASTE_OP(mulss)\
    PASTE_OP(divss)\
    PASTE_OP(addsd)\
    PASTE_OP(subsd)\
    PASTE_OP(mulsd)\
    PASTE_OP(divsd)\
    \
    PASTE_OP(push)\
    PASTE_OP(pop)\
    \
    PASTE_OP(cvtsi2ss)\
    PASTE_OP(cvtsi2sd)\
    PASTE_OP(cvtss2si)\
    PASTE_OP(cvtsd2si)\
    PASTE_OP(cvtss2sd)\
    PASTE_OP(cvtsd2ss)\


#define PASTE_OP(x) OP_##x,
enum Amd64_Opcode
{
    OPCODES
};
#undef PASTE_OP


#define PASTE_OP(x) #x,
const char *opcode_names[] = {
    OPCODES
};
#undef PASTE_OP

/* AMD64 registers
 * rip and mmx registers not listed.
 */
#define REGS\
    PASTE_REG(NONE)\
    PASTE_REG(rax)\
    PASTE_REG(rbx)\
    PASTE_REG(rcx)\
    PASTE_REG(rdx)\
    PASTE_REG(rbp)\
    PASTE_REG(rsi)\
    PASTE_REG(rdi)\
    PASTE_REG(rsp)\
    PASTE_REG(r8)\
    PASTE_REG(r9)\
    PASTE_REG(r10)\
    PASTE_REG(r11)\
    PASTE_REG(r12)\
    PASTE_REG(r13)\
    PASTE_REG(r14)\
    PASTE_REG(r15)\
    \
    PASTE_REG(xmm0)\
    PASTE_REG(xmm1)\
    PASTE_REG(xmm2)\
    PASTE_REG(xmm3)\
    PASTE_REG(xmm4)\
    PASTE_REG(xmm5)\
    PASTE_REG(xmm6)\
    PASTE_REG(xmm7)\
    PASTE_REG(xmm8)\
    PASTE_REG(xmm9)\
    PASTE_REG(xmm10)\
    PASTE_REG(xmm11)\
    PASTE_REG(xmm12)\
    PASTE_REG(xmm13)\
    PASTE_REG(xmm14)\
    PASTE_REG(xmm15)\

#define PASTE_REG(r) REG_##r,
enum Amd64_Register
{
    REGS
};
#undef PASTE_REG

#define PASTE_REG(r) #r,
const char *reg_names[] = {
    REGS
};
#undef PASTE_REG

static const char* GetRegName(Reg reg)
{
    return reg_names[reg.reg_index];
}

Reg MakeReg(Amd64_Register r)
{
    Reg reg;
    reg.reg_index = r;
    return reg;
}

static Reg general_regs[] = {
    MakeReg(REG_rax),
    MakeReg(REG_rbx),
    MakeReg(REG_rcx),
    MakeReg(REG_rdx),
    MakeReg(REG_rdi),
    MakeReg(REG_rsi),
    MakeReg(REG_r8),
    MakeReg(REG_r9),
    MakeReg(REG_r10),
    MakeReg(REG_r12),
    MakeReg(REG_r13),
    MakeReg(REG_r14),
    MakeReg(REG_r15),
};

static Reg float_regs[] = {
    MakeReg(REG_xmm0),
    MakeReg(REG_xmm1),
    MakeReg(REG_xmm2),
    MakeReg(REG_xmm3),
    MakeReg(REG_xmm4),
    MakeReg(REG_xmm5),
    MakeReg(REG_xmm6),
    MakeReg(REG_xmm7),
    MakeReg(REG_xmm8),
    MakeReg(REG_xmm9),
    MakeReg(REG_xmm10),
    MakeReg(REG_xmm11),
    MakeReg(REG_xmm12),
    MakeReg(REG_xmm13),
    MakeReg(REG_xmm14),
    MakeReg(REG_xmm15),
};

// Windows AMD64 ABI register usage
static Reg win_arg_regs[] = {
    MakeReg(REG_rcx),
    MakeReg(REG_rdx),
    MakeReg(REG_r8),
    MakeReg(REG_r9),
};
static Reg win_float_arg_regs[] = {
    MakeReg(REG_xmm0),
    MakeReg(REG_xmm1),
    MakeReg(REG_xmm2),
    MakeReg(REG_xmm3),
};

static Reg win_caller_save[] = {
    MakeReg(REG_rax),
    MakeReg(REG_rcx),
    MakeReg(REG_rdx),
    MakeReg(REG_r8),
    MakeReg(REG_r9),
    MakeReg(REG_r10),
    MakeReg(REG_r11),
    MakeReg(REG_xmm0),
    MakeReg(REG_xmm1),
    MakeReg(REG_xmm2),
    MakeReg(REG_xmm3),
    MakeReg(REG_xmm4),
    MakeReg(REG_xmm5),
    MakeReg(REG_xmm6),
    MakeReg(REG_xmm7),
};
static Reg win_callee_save[] = {
    MakeReg(REG_rbx),
    MakeReg(REG_rdi),
    MakeReg(REG_rsi),
    MakeReg(REG_r12),
    MakeReg(REG_r13),
    MakeReg(REG_r14),
    MakeReg(REG_r15),
    MakeReg(REG_xmm8),
    MakeReg(REG_xmm9),
    MakeReg(REG_xmm10),
    MakeReg(REG_xmm11),
    MakeReg(REG_xmm12),
    MakeReg(REG_xmm13),
    MakeReg(REG_xmm14),
    MakeReg(REG_xmm15),
};

// Unix System V ABI
static Reg nix_arg_regs[] = {
    MakeReg(REG_rdi),
    MakeReg(REG_rsi),
    MakeReg(REG_rdx),
    MakeReg(REG_rcx),
    MakeReg(REG_r8),
    MakeReg(REG_r9),
};
static Reg nix_float_arg_regs[] = {
    MakeReg(REG_xmm0),
    MakeReg(REG_xmm1),
    MakeReg(REG_xmm2),
    MakeReg(REG_xmm3),
    MakeReg(REG_xmm4),
    MakeReg(REG_xmm5),
    MakeReg(REG_xmm6),
    MakeReg(REG_xmm7),
};

static Reg nix_caller_save[] = {
    MakeReg(REG_rax),
    MakeReg(REG_rcx),
    MakeReg(REG_rdx),
    MakeReg(REG_rdi),
    MakeReg(REG_rsi),
    MakeReg(REG_r8),
    MakeReg(REG_r9),
    MakeReg(REG_r10),
    MakeReg(REG_r11),
    MakeReg(REG_xmm0),
    MakeReg(REG_xmm1),
    MakeReg(REG_xmm2),
    MakeReg(REG_xmm3),
    MakeReg(REG_xmm4),
    MakeReg(REG_xmm5),
    MakeReg(REG_xmm6),
    MakeReg(REG_xmm7),
    MakeReg(REG_xmm8),
    MakeReg(REG_xmm9),
    MakeReg(REG_xmm10),
    MakeReg(REG_xmm11),
    MakeReg(REG_xmm12),
    MakeReg(REG_xmm13),
    MakeReg(REG_xmm14),
    MakeReg(REG_xmm15),
};
static Reg nix_callee_save[] = {
    MakeReg(REG_rbx),
    MakeReg(REG_r12),
    MakeReg(REG_r13),
    MakeReg(REG_r14),
    MakeReg(REG_r15),
};

void InitializeCodegen_Amd64(Codegen_Context *ctx, Codegen_Target cg_target)
{
    switch (cg_target)
    {
    case CGT_COUNT:
        INVALID_CODE_PATH;
        break;

    case CGT_AMD64_Windows:
        InitRegAlloc(&ctx->reg_alloc,
                array_length(general_regs), general_regs,
                array_length(float_regs), float_regs,
                array_length(win_arg_regs), win_arg_regs,
                array_length(win_float_arg_regs), win_float_arg_regs,
                array_length(win_caller_save), win_caller_save,
                array_length(win_callee_save), win_callee_save);
        break;
    case CGT_AMD64_Unix:
        InitRegAlloc(&ctx->reg_alloc,
                array_length(general_regs), general_regs,
                array_length(float_regs), float_regs,
                array_length(nix_arg_regs), nix_arg_regs,
                array_length(nix_float_arg_regs), nix_float_arg_regs,
                array_length(nix_caller_save), nix_caller_save,
                array_length(nix_callee_save), nix_callee_save);
        break;
    }
}

static Operand NoneOperand()
{
    Operand result = { };
    result.type = OT_None;
    return result;
}

static Operand RegOperand(Reg reg, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = OT_Register;
    result.access_flags = access_flags;
    result.reg = reg;
    return result;
}

static Operand RegOperand(Amd64_Register reg, Oper_Access_Flags access_flags)
{
    return RegOperand(MakeReg(reg), access_flags);
}

static Operand AddrOperand(Reg base, s64 offset, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = OT_Addr;
    result.access_flags = access_flags;
    result.base_index_offs.base = base;
    result.base_index_offs.offset = offset;
    return result;
}

static Operand AddrOperand(Amd64_Register base, s64 offset, Oper_Access_Flags access_flags)
{
    return AddrOperand(MakeReg(base), offset, access_flags);
}

//static Operand AddrOperand(Reg base, Reg index, s64 scale, s64 offset, Oper_Access_Flags access_flags)
//{
//    Operand result = { };
//    result.type = OT_Addr;
//    result.access_flags = access_flags;
//    result.base_index_offs.base = base;
//    result.base_index_offs.index = index;
//    result.base_index_offs.scale = scale;
//    result.base_index_offs.offset = offset;
//    return result;
//}

static Operand ImmOperand(bool imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = OT_Immediate;
    result.access_flags = access_flags;
    result.imm_bool = imm;
    return result;
}

//static Operand ImmOperand(u64 imm, Oper_Access_Flags access_flags)
//{
//    Operand result = { };
//    result.type = OT_Immediate;
//    result.imm_u64 = imm;
//    return result;
//}

static Operand ImmOperand(s64 imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = OT_Immediate;
    result.access_flags = access_flags;
    result.imm_s64 = imm;
    return result;
}

static Operand ImmOperand(f32 imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = OT_Immediate;
    result.access_flags = access_flags;
    result.imm_f32 = imm;
    return result;
}

static Operand ImmOperand(f64 imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = OT_Immediate;
    result.access_flags = access_flags;
    result.imm_f64 = imm;
    return result;
}

static Operand IrOperand(Ir_Operand *ir_oper, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = OT_IrOperand;
    result.access_flags = access_flags;
    result.ir_oper = ir_oper;
    return result;
}

static Operand IrAddrOperand(Ir_Operand *base, s64 offset, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = OT_IrAddrOper;
    result.access_flags = access_flags;
    result.ir_base_index_offs.base = base;
    result.ir_base_index_offs.offset = offset;
    return result;
}

static Operand IrAddrOperand(Ir_Operand *base, Ir_Operand *index, s64 scale, s64 offset, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = OT_IrAddrOper;
    result.access_flags = access_flags;
    result.ir_base_index_offs.base = base;
    result.ir_base_index_offs.index = index;
    result.ir_base_index_offs.scale = scale;
    result.ir_base_index_offs.offset = offset;
    return result;
}

static void InsertInstruction(Codegen_Context *ctx,
        Amd64_Opcode opcode,
        Operand oper1 = NoneOperand(),
        Operand oper2 = NoneOperand(),
        Operand oper3 = NoneOperand())
{
    Instruction instr = { };
    instr.opcode = (Opcode)opcode;
    instr.oper1 = oper1;
    instr.oper2 = oper2;
    instr.oper3 = oper3;
    if (ctx->comment)
    {
        instr.comment = *ctx->comment;
        ctx->comment = nullptr;
    }
    array::Push(ctx->current_routine->instructions, instr);
}

static void InsertLoad(Codegen_Context *ctx, Operand oper1, Operand oper2)
{
    if (oper1.type == OT_IrOperand)
    {
        if (oper1.ir_oper->type->tag == TYP_f32)
            InsertInstruction(ctx, OP_movss, oper1, oper2);
        else if (oper1.ir_oper->type->tag == TYP_f64)
            InsertInstruction(ctx, OP_movsd, oper1, oper2);
        else
            InsertInstruction(ctx, OP_mov, oper1, oper2);
    }
    else if (oper2.type == OT_IrOperand)
    {
        if (oper2.ir_oper->type->tag == TYP_f32)
            InsertInstruction(ctx, OP_movss, oper1, oper2);
        else if (oper2.ir_oper->type->tag == TYP_f64)
            InsertInstruction(ctx, OP_movsd, oper1, oper2);
        else
            InsertInstruction(ctx, OP_mov, oper1, oper2);
    }
    else
    {
        InsertInstruction(ctx, OP_mov, oper1, oper2);
    }
}

static void InsertLoad(Codegen_Context *ctx, Ir_Operand *ir_oper1, Ir_Operand *ir_oper2)
{
    InsertLoad(ctx, IrOperand(ir_oper1, AF_Write), IrOperand(ir_oper2, AF_Read));
}

static void InsertLoadAddr(Codegen_Context *ctx, Operand oper1, Operand oper2)
{
    InsertInstruction(ctx, OP_lea, oper1, oper2);
}

static void InsertZeroReg(Codegen_Context *ctx, Operand oper)
{
    oper.access_flags = AF_Write;
    InsertInstruction(ctx, OP_xor, oper, oper);
}

static void InsertZeroReg(Codegen_Context *ctx, Reg reg)
{
    InsertZeroReg(ctx, RegOperand(reg, AF_Write));
}

static void InsertZeroReg(Codegen_Context *ctx, Amd64_Register reg)
{
    InsertZeroReg(ctx, MakeReg(reg));
}

static void InsertLabel(Codegen_Context *ctx, Name name)
{
    Operand oper = { };
    oper.label.name = name;
    InsertInstruction(ctx, OP_LABEL, oper);
}

static void GenerateCompare(Codegen_Context *ctx, Ir_Instruction *ir_instr)
{
    Amd64_Opcode cmp_op;
    if (ir_instr->oper1.type->tag == TYP_f32)
        cmp_op = OP_comiss;
    else if (ir_instr->oper1.type->tag == TYP_f64)
        cmp_op = OP_comisd;
    else
        cmp_op = OP_cmp;

    InsertInstruction(ctx, cmp_op,
            IrOperand(&ir_instr->oper1, AF_Read),
            IrOperand(&ir_instr->oper2, AF_Read));

    Type *ltype = ir_instr->oper1.type;
    b32 signed_or_float = (TypeIsFloat(ltype) || TypeIsSigned(ltype));
    switch (ir_instr->opcode)
    {
    case IR_Eq:
        {
            InsertInstruction(ctx, OP_mov,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(false, AF_Read));
            InsertInstruction(ctx, OP_cmove,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(true, AF_Read));
            break;
        }
    case IR_Neq:
        {
            InsertInstruction(ctx, OP_mov,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(false, AF_Read));
            InsertInstruction(ctx, OP_cmovne,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(true, AF_Read));
            break;
        }
    case IR_Lt:
        {
            InsertInstruction(ctx, OP_mov,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(false, AF_Read));
            Amd64_Opcode mov_op = signed_or_float ? OP_cmovl : OP_cmovb;
            InsertInstruction(ctx, mov_op,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(true, AF_Read));
            break;
        }
    case IR_Leq:
        {
            InsertInstruction(ctx, OP_mov,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(false, AF_Read));
            Amd64_Opcode mov_op = signed_or_float ? OP_cmovle : OP_cmovbe;
            InsertInstruction(ctx, mov_op,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(true, AF_Read));
            break;
        }
    case IR_Gt:
        {
            InsertInstruction(ctx, OP_mov,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(false, AF_Read));
            Amd64_Opcode mov_op = signed_or_float ? OP_cmovg : OP_cmova;
            InsertInstruction(ctx, mov_op,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(true, AF_Read));
            break;
        }
    case IR_Geq:
        {
            InsertInstruction(ctx, OP_mov,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(false, AF_Read));
            Amd64_Opcode mov_op = signed_or_float ? OP_cmovge : OP_cmovae;
            InsertInstruction(ctx, mov_op,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(true, AF_Read));
            break;
        }
        default:
            INVALID_CODE_PATH;
    }
}

static void GenerateArithmetic(Codegen_Context *ctx, Ir_Instruction *ir_instr)
{
    Type *ltype = ir_instr->oper1.type;
    b32 is_float = TypeIsFloat(ltype);
    b32 is_signed = TypeIsSigned(ltype);
    switch (ir_instr->opcode)
    {
    //case IR_Eq: case IR_Neq: case IR_Lt: case IR_Leq: case IR_Gt: case IR_Geq:
    default: INVALID_CODE_PATH;
    case IR_Add:
        {
            if (ir_instr->target != ir_instr->oper1)
                InsertLoad(ctx, &ir_instr->target, &ir_instr->oper1);
            if (is_float)
            {
                Amd64_Opcode add_op = (ltype->tag == TYP_f32) ? OP_addss : OP_addsd;
                InsertInstruction(ctx, add_op,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper2, AF_Read));
            }
            else
            {
                InsertInstruction(ctx, OP_add,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper2, AF_Read));
            }
        } break;
    case IR_Sub:
        {
            if (ir_instr->target != ir_instr->oper1)
                InsertLoad(ctx, &ir_instr->target, &ir_instr->oper1);
            if (is_float)
            {
                Amd64_Opcode sub_op = (ltype->tag == TYP_f32) ? OP_subss : OP_subsd;
                InsertInstruction(ctx, sub_op,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper2, AF_Read));
            }
            else
            {
                InsertInstruction(ctx, OP_sub,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper2, AF_Read));
            }
        } break;
    case IR_Mul:
        {
            if (is_float)
            {
                if (ir_instr->target != ir_instr->oper1)
                    InsertLoad(ctx, &ir_instr->target, &ir_instr->oper1);
                Amd64_Opcode mul_op = (ltype->tag == TYP_f32) ? OP_mulss : OP_mulsd;
                InsertInstruction(ctx, mul_op,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper2, AF_Read));
            }
            else if (is_signed)
            {
                if (ir_instr->target != ir_instr->oper1)
                    InsertLoad(ctx, &ir_instr->target, &ir_instr->oper1);
                InsertInstruction(ctx, OP_imul,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper2, AF_Read));
            }
            else // unsigned
            {
                InsertZeroReg(ctx, REG_rdx);
                InsertLoad(ctx, RegOperand(REG_rax, AF_Write), IrOperand(&ir_instr->oper1, AF_Read));
                InsertInstruction(ctx, OP_mul, IrOperand(&ir_instr->oper2, AF_Read));
                InsertLoad(ctx, IrOperand(&ir_instr->target, AF_Write), RegOperand(REG_rax, AF_Read));
            }
        } break;
    case IR_Div:
        {
            if (is_float)
            {
                if (ir_instr->target != ir_instr->oper1)
                    InsertLoad(ctx, &ir_instr->target, &ir_instr->oper1);
                Amd64_Opcode div_op = (ltype->tag == TYP_f32) ? OP_divss : OP_divsd;
                InsertInstruction(ctx, div_op,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper2, AF_Read));
            }
            else if (is_signed)
            {
                InsertZeroReg(ctx, REG_rdx);
                InsertLoad(ctx, RegOperand(REG_rax, AF_Write), IrOperand(&ir_instr->oper1, AF_Read));
                InsertInstruction(ctx, OP_idiv, IrOperand(&ir_instr->oper2, AF_Read));
                InsertLoad(ctx, IrOperand(&ir_instr->target, AF_Write), RegOperand(REG_rax, AF_Read));
            }
            else // unsigned
            {
                InsertZeroReg(ctx, REG_rdx);
                InsertLoad(ctx, RegOperand(REG_rax, AF_Write), IrOperand(&ir_instr->oper1, AF_Read));
                InsertInstruction(ctx, OP_div, IrOperand(&ir_instr->oper2, AF_Read));
                InsertLoad(ctx, IrOperand(&ir_instr->target, AF_Write), RegOperand(REG_rax, AF_Read));
            }
        } break;
    case IR_Mod:
        {
            if (is_float)
            {
                INVALID_CODE_PATH;
            }
            else if (is_signed)
            {
                InsertZeroReg(ctx, REG_rdx);
                InsertLoad(ctx, RegOperand(REG_rax, AF_Write), IrOperand(&ir_instr->oper1, AF_Read));
                InsertInstruction(ctx, OP_idiv, IrOperand(&ir_instr->oper2, AF_Read));
                InsertLoad(ctx, IrOperand(&ir_instr->target, AF_Write), RegOperand(REG_rdx, AF_Read));
            }
            else // unsigned
            {
                InsertZeroReg(ctx, REG_rdx);
                InsertLoad(ctx, RegOperand(REG_rax, AF_Write), IrOperand(&ir_instr->oper1, AF_Read));
                InsertInstruction(ctx, OP_div, IrOperand(&ir_instr->oper2, AF_Read));
                InsertLoad(ctx, IrOperand(&ir_instr->target, AF_Write), RegOperand(REG_rdx, AF_Read));
            }
        } break;

    case IR_And:
        InsertInstruction(ctx, OP_and,
                IrOperand(&ir_instr->oper1, AF_Write),
                IrOperand(&ir_instr->oper2, AF_Read));
        break;
    case IR_Or:
        InsertInstruction(ctx, OP_or,
                IrOperand(&ir_instr->oper1, AF_Write),
                IrOperand(&ir_instr->oper2, AF_Read));
        break;
    case IR_Xor:
        InsertInstruction(ctx, OP_xor,
                IrOperand(&ir_instr->oper1, AF_Write),
                IrOperand(&ir_instr->oper2, AF_Read));
        break;

    case IR_Neg:
        if (is_float)
        {
            if (ltype->tag == TYP_f32)
            {
                InsertLoad(ctx, IrOperand(&ir_instr->target, AF_Write), ImmOperand(0.0f, AF_Read));
                InsertInstruction(ctx, OP_subss,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper1, AF_Read));
            }
            else if (ltype->tag == TYP_f64)
            {
                InsertLoad(ctx, IrOperand(&ir_instr->target, AF_Write), ImmOperand(0.0, AF_Read));
                InsertInstruction(ctx, OP_subsd,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper1, AF_Read));
            }
        }
        else
        {
            InsertLoad(ctx, &ir_instr->target, &ir_instr->oper1);
            InsertInstruction(ctx, OP_neg, IrOperand(&ir_instr->target, AF_ReadWrite));
        }
    case IR_Not:
    case IR_Compl:
        InsertLoad(ctx, &ir_instr->target, &ir_instr->oper1);
        InsertInstruction(ctx, OP_not, IrOperand(&ir_instr->target, AF_ReadWrite));
    }
}

static void PushArgs(Codegen_Context *ctx,
        Ir_Routine *routine, Ir_Instruction *ir_instr)
{
    s64 arg_count = ctx->current_arg_count;
    s64 arg_index = arg_count - 1;
    s64 arg_reg_idx = arg_index;
    s64 float_arg_reg_idx = arg_index;

    s64 arg_stack_alloc = (arg_count > 4 ? arg_count : 4) * 8;
    InsertInstruction(ctx, OP_sub, RegOperand(REG_rsp, AF_Write), ImmOperand(arg_stack_alloc, AF_Read));

    ASSERT(ir_instr->oper2.oper_type == IR_OPER_Immediate);
    s64 arg_instr_idx = ir_instr->oper2.imm_s64;
    while (arg_instr_idx != -1)
    {
        //Ir_Instruction arg_instr = array::At(routine->instructions, arg_instr_idx);
        Ir_Instruction *arg_instr = routine->instructions.data + arg_instr_idx;
        ASSERT(arg_instr->opcode == IR_Arg);

        ctx->comment = &arg_instr->comment;

        Type *arg_type = arg_instr->target.type;
        if (TypeIsStruct(arg_type))
        {
            //PushStructArg();

            const Reg *arg_reg = nullptr;
            arg_reg = GetArgRegister(&ctx->reg_alloc, arg_reg_idx);
            float_arg_reg_idx--;
            arg_reg_idx--;
            if (arg_reg)
            {
                if (arg_type->size > 8)
                {
                    InsertLoadAddr(ctx,
                            RegOperand(*arg_reg, AF_Write),
                            IrOperand(&arg_instr->target, AF_Read));
                }
                else
                {
                    InsertLoad(ctx,
                            RegOperand(*arg_reg, AF_Write),
                            IrOperand(&arg_instr->target, AF_Read));
                }
            }
            else
            {
                if (arg_type->size > 8)
                {
                    InsertLoadAddr(ctx,
                            AddrOperand(REG_rsp, arg_index * 8, AF_Write),
                            IrOperand(&arg_instr->target, AF_Read));
                }
                else
                {
                    InsertLoad(ctx,
                            AddrOperand(REG_rsp, arg_index * 8, AF_Write),
                            IrAddrOperand(&arg_instr->target, 0, AF_Read));
                }
            }
        }
        else
        {
            const Reg *arg_reg = nullptr;
            if (TypeIsFloat(arg_type))
            {
                arg_reg = GetFloatArgRegister(&ctx->reg_alloc, float_arg_reg_idx);
                float_arg_reg_idx--;
                arg_reg_idx--;
            }
            else
            {
                arg_reg = GetArgRegister(&ctx->reg_alloc, arg_reg_idx);
                float_arg_reg_idx--;
                arg_reg_idx--;
            }
            if (arg_reg)
            {
                InsertLoad(ctx,
                        RegOperand(*arg_reg, AF_Write),
                        IrOperand(&arg_instr->target, AF_Read));
            }
        }

        arg_index--;

        ASSERT(arg_instr->oper1.oper_type == IR_OPER_Immediate);
        arg_instr_idx = arg_instr->oper1.imm_s64;
    }
}

static void GenerateCode(Codegen_Context *ctx,
        Ir_Routine *routine, Ir_Instruction *ir_instr)
{
    switch (ir_instr->opcode)
    {
        case IR_COUNT:
            INVALID_CODE_PATH;
            break;

        case IR_Add:
        case IR_Sub:
        case IR_Mul:
        case IR_Div:
        case IR_Mod:
        case IR_And:
        case IR_Or:
        case IR_Xor:
        case IR_Not:
        case IR_Neg:
        case IR_Compl:
            GenerateArithmetic(ctx, ir_instr);
            break;

        case IR_Eq:
        case IR_Neq:
        case IR_Lt:
        case IR_Leq:
        case IR_Gt:
        case IR_Geq:
            GenerateCompare(ctx, ir_instr);
            break;

        case IR_Deref:
            // mov target, [oper1]
            InsertLoad(ctx,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrAddrOperand(&ir_instr->oper1, 0, AF_Read));
            break;

        case IR_Addr:
            // lea target, [oper1]
            InsertLoadAddr(ctx,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrAddrOperand(&ir_instr->oper1, 0, AF_Read));
            break;

        case IR_Mov:
            // mov target, oper1
            InsertLoad(ctx, &ir_instr->target, &ir_instr->oper1);
            break;
        case IR_MovMember:
            {
                Type *oper_type = ir_instr->oper1.type;
                s64 member_index = ir_instr->oper2.imm_s64;
                if (TypeIsPointer(oper_type))
                {
                    s64 member_offset = GetStructMemberOffset(oper_type->base_type, member_index);
                    InsertLoad(ctx, &ir_instr->target, &ir_instr->oper1);
                    if (member_offset != 0)
                    {
                        InsertInstruction(ctx, OP_add,
                                IrOperand(&ir_instr->target, AF_Write),
                                ImmOperand((s64)member_offset, AF_Read));
                    }
                }
                else
                {
                    s64 member_offset = GetStructMemberOffset(oper_type, member_index);
                    InsertLoadAddr(ctx,
                            IrOperand(&ir_instr->target, AF_Write),
                            IrAddrOperand(&ir_instr->oper1, member_offset, AF_Read));
                }
            } break;
        case IR_MovElement:
            {
                //NOT_IMPLEMENTED("mov elem");
                // target <- base + index*size
                s64 size = GetAlignedElementSize(ir_instr->oper1.type);
                InsertLoadAddr(ctx,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrAddrOperand(&ir_instr->oper1, &ir_instr->oper2, size, 0, AF_Read));
            } break;

        case IR_Arg:
            ctx->current_arg_count++;
            break;

        case IR_Call:
            PushArgs(ctx, routine, ir_instr);
            InsertInstruction(ctx, OP_call, IrOperand(&ir_instr->oper1, AF_Read));
            if (ir_instr->target.oper_type != IR_OPER_None)
                InsertLoad(ctx,
                        IrOperand(&ir_instr->target, AF_Write),
                        RegOperand(REG_rax, AF_Read));
            ctx->current_arg_count = 0;
            break;
        case IR_CallForeign:
            PushArgs(ctx, routine, ir_instr);
            InsertInstruction(ctx, OP_call, IrOperand(&ir_instr->oper1, AF_Read));
            if (ir_instr->target.oper_type != IR_OPER_None)
                InsertLoad(ctx,
                        IrOperand(&ir_instr->target, AF_Write),
                        RegOperand(REG_rax, AF_Read));
            ctx->current_arg_count = 0;
            break;

        case IR_Jump:
            InsertInstruction(ctx, OP_jmp, IrOperand(&ir_instr->target, AF_Read));
            break;
        case IR_Jz:
            InsertInstruction(ctx, OP_cmp,
                    IrOperand(&ir_instr->oper1, AF_Read),
                    ImmOperand((s64)0, AF_Read));
            InsertInstruction(ctx, OP_je, IrOperand(&ir_instr->target, AF_Read));
            break;
        case IR_Jnz:
            InsertInstruction(ctx, OP_cmp,
                    IrOperand(&ir_instr->oper1, AF_Read),
                    ImmOperand((s64)0, AF_Read));
            InsertInstruction(ctx, OP_jne, IrOperand(&ir_instr->target, AF_Read));
            break;
        case IR_Return:
            if (ir_instr->target.oper_type != IR_OPER_None)
                InsertLoad(ctx,
                        RegOperand(REG_rax, AF_Write),
                        IrOperand(&ir_instr->target, AF_Read));
            InsertInstruction(ctx, OP_ret);
            break;

        case IR_S_TO_F32:
            InsertInstruction(ctx, OP_cvtsi2ss,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrOperand(&ir_instr->oper1, AF_Read));
            break;
        case IR_S_TO_F64:
            InsertInstruction(ctx, OP_cvtsi2sd,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrOperand(&ir_instr->oper1, AF_Read));
            break;
        case IR_F32_TO_S:
            InsertInstruction(ctx, OP_cvtss2si,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrOperand(&ir_instr->oper1, AF_Read));
            break;
        case IR_F64_TO_S:
            InsertInstruction(ctx, OP_cvtsd2si,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrOperand(&ir_instr->oper1, AF_Read));
            break;
        case IR_F32_TO_F64:
            InsertInstruction(ctx, OP_cvtss2sd,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrOperand(&ir_instr->oper1, AF_Read));
            break;
        case IR_F64_TO_F32:
            InsertInstruction(ctx, OP_cvtsd2ss,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrOperand(&ir_instr->oper1, AF_Read));
            break;
    }
}

static void AllocateRegister(Codegen_Context *ctx, Operand *oper)
{
    switch (oper->type)
    {
        case OT_None: break;
        case OT_Label: break;
        case OT_Temp: NOT_IMPLEMENTED("TEMP alloc"); break;
        case OT_Register:
        case OT_Addr:
        case OT_Immediate:
            break;
        case OT_IrOperand:
            {
                Ir_Operand *ir_oper = oper->ir_oper;
                switch (ir_oper->oper_type)
                {
                    case IR_OPER_None: INVALID_CODE_PATH; break;
                    case IR_OPER_Variable:
                        {
                            const Reg *mapped_reg = GetMappedRegister(&ctx->reg_alloc, ir_oper->var.name);
                            if (!mapped_reg)
                            {
                                Reg reg = GetFreeRegister(&ctx->reg_alloc);
                                MapRegister(&ctx->reg_alloc, ir_oper->var.name, reg);
                                *oper = RegOperand(reg, oper->access_flags);
                            }
                            else
                            {
                                *oper = RegOperand(*mapped_reg, oper->access_flags);
                            }
                        } break;
                    case IR_OPER_Temp:
                        {
                            const Reg *mapped_reg = GetMappedRegister(&ctx->reg_alloc, ir_oper->temp.name);
                            if (!mapped_reg)
                            {
                                Reg reg = GetFreeRegister(&ctx->reg_alloc);
                                MapRegister(&ctx->reg_alloc, ir_oper->temp.name, reg);
                                *oper = RegOperand(reg, oper->access_flags);
                            }
                            else
                            {
                                *oper = RegOperand(*mapped_reg, oper->access_flags);
                            }
                        } break;
                    case IR_OPER_Immediate:
                    case IR_OPER_Label:
                    case IR_OPER_Routine:
                    case IR_OPER_ForeignRoutine:
                        break;
                }
            } break;
        case OT_IrAddrOper:
            NOT_IMPLEMENTED("OT_IrAddrOper");
            break;
    }
}

static void AllocateRegisters(Codegen_Context *ctx, Ir_Routine *ir_routine)
{
    ClearRegAllocs(&ctx->reg_alloc);
    DirtyCalleeSaveRegs(&ctx->reg_alloc);
    for (s64 i = 0; i < ir_routine->arg_count; i++)
    {
        Ir_Operand *arg = &ir_routine->args[i];
        const Reg *arg_reg = GetArgRegister(&ctx->reg_alloc, i);
        if (arg_reg)
        {
            MapRegister(&ctx->reg_alloc, arg->var.name, *arg_reg);
            DirtyRegister(&ctx->reg_alloc, *arg_reg);
        }
    }
    for (s64 i = 0; i < ctx->current_routine->instructions.count; i++)
    {
        Instruction *instr = ctx->current_routine->instructions.data + i;
        AllocateRegister(ctx, &instr->oper1);
        AllocateRegister(ctx, &instr->oper2);
        AllocateRegister(ctx, &instr->oper3);
    }
}

static void GenerateCode(Codegen_Context *ctx, Ir_Routine *routine)
{
    InsertLabel(ctx, routine->name);
    for (s64 i = 0; i < routine->instructions.count; i++)
    {
        //Ir_Instruction ir_instr = array::At(routine->instructions, i);
        Ir_Instruction *ir_instr = routine->instructions.data + i;
        if (!ctx->comment)
            ctx->comment = &ir_instr->comment;
        GenerateCode(ctx, routine, ir_instr);
    }
    AllocateRegisters(ctx, routine);
}

void GenerateCode_Amd64(Codegen_Context *ctx, Ir_Routine_List routines)
{
    ctx->routine_count = routines.count;
    ctx->routines = PushArray<Routine>(&ctx->arena, routines.count);
    for (s64 i = 0; i < routines.count; i++)
    {
        ctx->current_routine = &ctx->routines[i];
        GenerateCode(ctx, array::At(routines, i));
    }
}

// Print code


static s64 PrintPadding(FILE *file, s64 len, s64 min_len)
{
    s64 wlen = min_len - len;
    while (len < min_len)
    {
        fputc(' ', file);
        len++;
    }
    return (wlen > 0) ? wlen : 0;
}

static s64 PrintString(FILE *file, String str, s64 max_len)
{
    bool ellipsis = false;
    if (str.size < max_len)
        max_len = str.size;
    else if (str.size > max_len)
        ellipsis = true;

    s64 len = 0;
    for (s64 i = 0; i < max_len - (ellipsis ? 3 : 0); i++)
    {
        char c = str.data[i];
        switch (c)
        {
            case '\t': len += 2; fputs("\\t", file); break;
            case '\n': len += 2; fputs("\\n", file); break;
            case '\r': len += 2; fputs("\\r", file); break;
            case '\f': len += 2; fputs("\\f", file); break;
            case '\v': len += 2; fputs("\\v", file); break;
            default:
                len += 1; fputc(c, file);
        }
    }
    if (ellipsis) len += fprintf(file, "...");
    return len;
}

static s64 PrintPtr(FILE *file, void *ptr)
{
    if (ptr)
        return fprintf(file, "%p", ptr);
    else
        return fprintf(file, "(null)");
}

static s64 PrintBool(FILE *file, bool value)
{
    return fprintf(file, "%s", (value ? "(true)" : "(false)"));
}

static s64 PrintIrImmediate(FILE *file, Ir_Operand oper)
{
    switch (oper.type->tag)
    {
        case TYP_none:
        case TYP_pending:
        case TYP_null:
        case TYP_void:

        case TYP_pointer:   return PrintPtr(file, oper.imm_ptr); break;
        case TYP_bool:      return PrintBool(file, oper.imm_bool); break;
        case TYP_char:      return fprintf(file, "'%c'", oper.imm_u8); break;
        case TYP_u8:        return fprintf(file, "%u", oper.imm_u8); break;
        case TYP_s8:        return fprintf(file, "%d", oper.imm_s8); break;
        case TYP_u16:       return fprintf(file, "%u", oper.imm_u16); break;
        case TYP_s16:       return fprintf(file, "%d", oper.imm_s16); break;
        case TYP_u32:       return fprintf(file, "%u", oper.imm_u32); break;
        case TYP_s32:       return fprintf(file, "%d", oper.imm_s32); break;
        case TYP_u64:       return fprintf(file, "%" PRIu64, oper.imm_u64); break;
        case TYP_s64:       return fprintf(file, "%" PRId64, oper.imm_s64); break;
        case TYP_f32:       return fprintf(file, "%ff", oper.imm_f32); break;
        case TYP_f64:       return fprintf(file, "%fd", oper.imm_f64); break;
        case TYP_string:
        {
            s64 len = 0;
            len += fprintf(file, "\"");
            len += PrintString(file, oper.imm_str, 20);
            len += fprintf(file, "\"");
            return len;
        }

        case TYP_Struct:
        case TYP_Function:
            INVALID_CODE_PATH;
            break;
    }
    return 0;
}

static s64 PrintIrLabel(FILE *file, Ir_Operand label_oper)
{
    return fprintf(file, "L:%" PRId64, label_oper.label->target_loc);
}

static s64 PrintIrOperand(IoFile *file, Ir_Operand oper)
{
    s64 len = 0;
    switch (oper.oper_type)
    {
        case IR_OPER_None:
            len += fprintf((FILE*)file, "_");
            break;
        case IR_OPER_Variable:
            len += fprintf((FILE*)file, "%%");
            len += PrintName(file, oper.var.name);
            break;
        case IR_OPER_Temp:
            //len += fprintf((FILE*)file, "@temp%" PRId64, oper.temp.temp_id);
            len += PrintName(file, oper.temp.name);
            break;
        case IR_OPER_Immediate:
            len += PrintIrImmediate((FILE*)file, oper);
            break;
        case IR_OPER_Label:
            len += PrintIrLabel((FILE*)file, oper);
            break;
        case IR_OPER_Routine:
        case IR_OPER_ForeignRoutine:
            len += fprintf((FILE*)file, "<");
            len += PrintName(file, oper.var.name);
            len += fprintf((FILE*)file, ">");
            break;
    }
    return len;
}


static s64 PrintOperand(IoFile *file, Operand oper, b32 first)
{
    if (oper.type == OT_None) return 0;

    s64 len = 0;
    if (!first)
        len += fprintf((FILE*)file, ", ");
    switch (oper.type)
    {
        case OT_None:
            break;
        case OT_Label:
            NOT_IMPLEMENTED("OT_Label");
            break;
        case OT_Temp:
            NOT_IMPLEMENTED("OT_Temp");
            break;
        case OT_Register:
            len += fprintf((FILE*)file, "%s", GetRegName(oper.reg));
            break;
        case OT_IrOperand:
            len += PrintIrOperand(file, *oper.ir_oper);
            break;
        case OT_Immediate:
            len += fprintf((FILE*)file, "%" PRIu64, oper.imm_u64);
            break;
        case OT_Addr:
            {
                Addr_Base_Index_Offs base_idx_offs = oper.base_index_offs;
                len += fprintf((FILE*)file, "[");
                if (base_idx_offs.base.reg_index != REG_NONE)
                {
                    len += fprintf((FILE*)file, "%s", GetRegName(base_idx_offs.base));
                }
                if (base_idx_offs.index.reg_index != REG_NONE)
                {
                    s32 scale = base_idx_offs.scale;
                    if (scale > 0)
                    {
                        len += fprintf((FILE*)file, "+");
                    }
                    else
                    {
                        len += fprintf((FILE*)file, "-");
                        scale = -scale;
                    }
                    len += fprintf((FILE*)file, "%s*%" PRId32,
                            GetRegName(base_idx_offs.index), scale);
                }
                if (base_idx_offs.offset != 0)
                {
                    if (base_idx_offs.offset > 0)
                        len += fprintf((FILE*)file, "+");
                    len += fprintf((FILE*)file, "%" PRId32, base_idx_offs.offset);
                }
                len += fprintf((FILE*)file, "]");
            } break;
        case OT_IrAddrOper:
            {
                Addr_Ir_Base_Index_Offs base_idx_offs = oper.ir_base_index_offs;
                len += fprintf((FILE*)file, "[");
                if (base_idx_offs.base)
                {
                    len += PrintIrOperand(file, *base_idx_offs.base);
                }
                if (base_idx_offs.index)
                {
                    s32 scale = base_idx_offs.scale;
                    if (scale > 0)
                    {
                        len += fprintf((FILE*)file, "+");
                    }
                    else
                    {
                        len += fprintf((FILE*)file, "-");
                        scale = -scale;
                    }
                    len += PrintIrOperand(file, *base_idx_offs.index);
                    len += fprintf((FILE*)file, "*%" PRId32, scale);
                }
                if (base_idx_offs.offset != 0)
                {
                    if (base_idx_offs.offset > 0)
                        len += fprintf((FILE*)file, "+");
                    len += fprintf((FILE*)file, "%" PRId32, base_idx_offs.offset);
                }
                len += fprintf((FILE*)file, "]");
            } break;
    }
    return len;
}

static s64 PrintLabel(IoFile *file, Operand label_oper)
{
    s64 len = 0;
    len += PrintString(file, label_oper.label.name.str);
    len += fprintf((FILE*)file, ":");
    return len;
}

static s64 PrintComment(FILE *file, Ir_Comment comment)
{
    s64 len = 0;
    if (comment.start)
    {
        len += fprintf(file, "\t; ");
        len += fwrite(comment.start, 1, comment.end - comment.start, file);
    }
    return len;
}

static s64 PrintOpcode(IoFile *file, Amd64_Opcode opcode)
{
    s64 len = 0;
    len += fprintf((FILE*)file, "%s", opcode_names[opcode]);
    len += PrintPadding((FILE*)file, len, 10);
    return len;
}

static void PrintInstruction(IoFile *file, const Instruction instr)
{
    s64 len = 0;
    if ((Amd64_Opcode)instr.opcode == OP_LABEL)
    {
        len += PrintLabel(file, instr.oper1);
    }
    else
    {
        len += fprintf((FILE*)file, "\t");
        len += PrintOpcode(file, (Amd64_Opcode)instr.opcode);
        len += fprintf((FILE*)file, "\t");
        len += PrintOperand(file, instr.oper1, true);
        len += PrintOperand(file, instr.oper2, false);
        len += PrintOperand(file, instr.oper3, false);
    }
    PrintPadding((FILE*)file, len, 40);
    PrintComment((FILE*)file, instr.comment);
    fprintf((FILE*)file, "\n");
}

void OutputCode_Amd64(Codegen_Context *ctx)
{
    fprintf((FILE*)ctx->code_out, "; -----\n");
    fprintf((FILE*)ctx->code_out, "; %s\n", GetTargetString(ctx->target));
    fprintf((FILE*)ctx->code_out, "; -----\n\n");

    for (s64 routine_idx = 0; routine_idx < ctx->routine_count; routine_idx++)
    {
        Routine *routine = &ctx->routines[routine_idx];
        for (s64 i = 0; i < routine->instructions.count; i++)
        {
            PrintInstruction(ctx->code_out, array::At(routine->instructions, i));
        }
    }
}

} // hplang
