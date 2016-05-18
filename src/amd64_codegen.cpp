
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


template <s64 N>
Reg MakeReg(const char (&name)[N])
{
    Reg result = { };
    String str = { };
    str.size = N - 1;
    str.data = const_cast<char*>(name);
    result.name = MakeName(str);
    return result;
}


static Reg win_caller_save[] = {
    MakeReg("rax"),
    MakeReg("rcx"),
    MakeReg("rdx"),
    MakeReg("r8"),
    MakeReg("r9"),
    MakeReg("r10"),
    MakeReg("r11"),
    MakeReg("xmm0"),
    MakeReg("xmm1"),
    MakeReg("xmm2"),
    MakeReg("xmm3"),
    MakeReg("xmm4"),
    MakeReg("xmm5"),
    MakeReg("xmm6"),
    MakeReg("xmm7"),
};
static Reg win_callee_save[] = {
    // MakeReg("rbp") // NOTE(henrik): This register is handled explicitly
    MakeReg("rbx"),
    MakeReg("rdi"),
    MakeReg("rsi"),
    MakeReg("r12"),
    MakeReg("r13"),
    MakeReg("r14"),
    MakeReg("r15"),
    MakeReg("xmm8"),
    MakeReg("xmm9"),
    MakeReg("xmm10"),
    MakeReg("xmm11"),
    MakeReg("xmm12"),
    MakeReg("xmm13"),
    MakeReg("xmm14"),
    MakeReg("xmm15"),
};

static Reg nix_caller_save[] = {
    MakeReg("rax"),
    MakeReg("rcx"),
    MakeReg("rdx"),
    MakeReg("rdi"),
    MakeReg("rsi"),
    MakeReg("r8"),
    MakeReg("r9"),
    MakeReg("r10"),
    MakeReg("r11"),
    MakeReg("xmm0"),
    MakeReg("xmm1"),
    MakeReg("xmm2"),
    MakeReg("xmm3"),
    MakeReg("xmm4"),
    MakeReg("xmm5"),
    MakeReg("xmm6"),
    MakeReg("xmm7"),
    MakeReg("xmm8"),
    MakeReg("xmm9"),
    MakeReg("xmm10"),
    MakeReg("xmm11"),
    MakeReg("xmm12"),
    MakeReg("xmm13"),
    MakeReg("xmm14"),
    MakeReg("xmm15"),
};
static Reg nix_callee_save[] = {
    // MakeReg("rbp") // NOTE(henrik): This register is handled explicitly
    MakeReg("rbx"),
    MakeReg("r12"),
    MakeReg("r13"),
    MakeReg("r14"),
    MakeReg("r15"),
};

void InitializeCodegen_Amd64(Codegen_Context *ctx, Codegen_Target cg_target)
{
    switch (cg_target)
    {
    case CGT_AMD64_Windows:
        InitRegAlloc(&ctx->reg_alloc,
                array_length(win_caller_save), win_caller_save,
                array_length(win_callee_save), win_callee_save);
        break;
    case CGT_AMD64_Unix:
        InitRegAlloc(&ctx->reg_alloc,
                array_length(nix_caller_save), nix_caller_save,
                array_length(nix_callee_save), nix_callee_save);
        break;
    }
}

Reg AllocateRegister(Reg_Alloc *reg_alloc)
{
    (void)reg_alloc;
    return reg_alloc->caller_save_regs[0];
}

static Operand NoneOperand()
{
    Operand result = { };
    result.type = OT_None;
    return result;
}

static Operand RegOperand(Reg reg)
{
    Operand result = { };
    result.type = OT_Register;
    result.reg = reg;
    return result;
}

static Operand ImmOperand(bool imm)
{
    Operand result = { };
    result.type = OT_Immediate;
    result.imm_bool = imm;
    return result;
}

static Operand ImmOperand(u64 imm)
{
    Operand result = { };
    result.type = OT_Immediate;
    result.imm_u64 = imm;
    return result;
}

static Operand ImmOperand(s64 imm)
{
    Operand result = { };
    result.type = OT_Immediate;
    result.imm_s64 = imm;
    return result;
}

static Operand ImmOperand(f32 imm)
{
    Operand result = { };
    result.type = OT_Immediate;
    result.imm_f32 = imm;
    return result;
}

static Operand ImmOperand(f64 imm)
{
    Operand result = { };
    result.type = OT_Immediate;
    result.imm_f64 = imm;
    return result;
}

static Operand IrOperand(Ir_Operand ir_oper)
{
    Operand result = { };
    result.type = OT_IrOperand;
    result.ir_oper = ir_oper;
    return result;
}

static Operand IrAddrOperand(Ir_Operand ir_oper, s64 offset)
{
    Operand result = { };
    result.type = OT_IrAddrOper;
    result.ir_base_offs.base = ir_oper;
    result.ir_base_offs.offset = offset;
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
    array::Push(ctx->instructions, instr);
}

static void InsertInstruction(Codegen_Context *ctx,
        Amd64_Opcode opcode, Ir_Operand ir_oper1, Ir_Operand ir_oper2)
{
    InsertInstruction(ctx, opcode,
            IrOperand(ir_oper1), IrOperand(ir_oper2));
}

static void InsertLoad(Codegen_Context *ctx, Operand oper1, Operand oper2)
{
    InsertInstruction(ctx, OP_mov, oper1, oper2);
}

static void InsertLoad(Codegen_Context *ctx, Ir_Operand ir_oper1, Ir_Operand ir_oper2)
{
    InsertLoad(ctx, IrOperand(ir_oper1), IrOperand(ir_oper2));
}

static void InsertLoadAddr(Codegen_Context *ctx, Operand oper1, Operand oper2)
{
    InsertInstruction(ctx, OP_lea, oper1, oper2);
}

static void InsertZeroReg(Codegen_Context *ctx, Operand oper)
{
    InsertInstruction(ctx, OP_xor, oper, oper);
}

static void InsertZeroReg(Codegen_Context *ctx, Reg reg)
{
    InsertZeroReg(ctx, RegOperand(reg));
}

static void InsertLabel(Codegen_Context *ctx, Name name)
{
    Operand oper = { };
    oper.label.name = name;
    InsertInstruction(ctx, OP_LABEL, oper);
}

static void GenerateCompare(Codegen_Context *ctx, Ir_Instruction ir_instr)
{
    Amd64_Opcode cmp_op;
    if (ir_instr.oper1.ir_type == IR_TYP_f32)
        cmp_op = OP_comiss;
    else if (ir_instr.oper1.ir_type == IR_TYP_f64)
        cmp_op = OP_comisd;
    else
        cmp_op = OP_cmp;

    InsertInstruction(ctx, cmp_op, IrOperand(ir_instr.oper1), IrOperand(ir_instr.oper2));

    Type *ltype = ir_instr.oper1.type;
    b32 signed_or_float = (TypeIsFloat(ltype) || TypeIsSigned(ltype));
    switch (ir_instr.opcode)
    {
    case IR_Eq:
        {
            InsertInstruction(ctx, OP_mov, IrOperand(ir_instr.target), ImmOperand(false));
            InsertInstruction(ctx, OP_cmove, IrOperand(ir_instr.target), ImmOperand(true));
            break;
        }
    case IR_Neq:
        {
            InsertInstruction(ctx, OP_mov, IrOperand(ir_instr.target), ImmOperand(false));
            InsertInstruction(ctx, OP_cmovne, IrOperand(ir_instr.target), ImmOperand(true));
            break;
        }
    case IR_Lt:
        {
            InsertInstruction(ctx, OP_mov, IrOperand(ir_instr.target), ImmOperand(false));
            Amd64_Opcode mov_op = signed_or_float ? OP_cmovl : OP_cmovb;
            InsertInstruction(ctx, mov_op, IrOperand(ir_instr.target), ImmOperand(true));
            break;
        }
    case IR_Leq:
        {
            InsertInstruction(ctx, OP_mov, IrOperand(ir_instr.target), ImmOperand(false));
            Amd64_Opcode mov_op = signed_or_float ? OP_cmovle : OP_cmovbe;
            InsertInstruction(ctx, mov_op, IrOperand(ir_instr.target), ImmOperand(true));
            break;
        }
    case IR_Gt:
        {
            InsertInstruction(ctx, OP_mov, IrOperand(ir_instr.target), ImmOperand(false));
            Amd64_Opcode mov_op = signed_or_float ? OP_cmovg : OP_cmova;
            InsertInstruction(ctx, mov_op, IrOperand(ir_instr.target), ImmOperand(true));
            break;
        }
    case IR_Geq:
        {
            InsertInstruction(ctx, OP_mov, IrOperand(ir_instr.target), ImmOperand(false));
            Amd64_Opcode mov_op = signed_or_float ? OP_cmovge : OP_cmovae;
            InsertInstruction(ctx, mov_op, IrOperand(ir_instr.target), ImmOperand(true));
            break;
        }
        default:
            INVALID_CODE_PATH;
    }
}

static void GenerateArithmetic(Codegen_Context *ctx, Ir_Instruction ir_instr)
{
    Type *ltype = ir_instr.oper1.type;
    b32 is_float = TypeIsFloat(ltype);
    b32 is_signed = TypeIsSigned(ltype);
    switch (ir_instr.opcode)
    {
    //case IR_Eq: case IR_Neq: case IR_Lt: case IR_Leq: case IR_Gt: case IR_Geq:
    default: INVALID_CODE_PATH;
    case IR_Add:
        {
            if (ir_instr.target != ir_instr.oper1)
                InsertLoad(ctx, ir_instr.target, ir_instr.oper1);
            if (is_float)
            {
                Amd64_Opcode add_op = (ltype->tag == TYP_f32) ? OP_addss : OP_addsd;
                InsertInstruction(ctx, add_op, ir_instr.target, ir_instr.oper2);
            }
            else
            {
                InsertInstruction(ctx, OP_add, ir_instr.target, ir_instr.oper2);
            }
        } break;
    case IR_Sub:
        {
            if (ir_instr.target != ir_instr.oper1)
                InsertLoad(ctx, ir_instr.target, ir_instr.oper1);
            if (is_float)
            {
                Amd64_Opcode sub_op = (ltype->tag == TYP_f32) ? OP_subss : OP_subsd;
                InsertInstruction(ctx, sub_op, ir_instr.target, ir_instr.oper2);
            }
            else
            {
                InsertInstruction(ctx, OP_sub, ir_instr.target, ir_instr.oper2);
            }
        } break;
    case IR_Mul:
        {
            if (is_float)
            {
                if (ir_instr.target != ir_instr.oper1)
                    InsertLoad(ctx, ir_instr.target, ir_instr.oper1);
                Amd64_Opcode mul_op = (ltype->tag == TYP_f32) ? OP_mulss : OP_mulsd;
                InsertInstruction(ctx, mul_op, ir_instr.target, ir_instr.oper2);
            }
            else if (is_signed)
            {
                if (ir_instr.target != ir_instr.oper1)
                    InsertLoad(ctx, ir_instr.target, ir_instr.oper1);
                InsertInstruction(ctx, OP_imul, ir_instr.target, ir_instr.oper2);
            }
            else // unsigned
            {
                Reg rax = MakeReg("rax");
                Reg rdx = MakeReg("rdx");
                InsertZeroReg(ctx, rdx);
                InsertLoad(ctx, RegOperand(rax), IrOperand(ir_instr.oper1));
                InsertInstruction(ctx, OP_mul, IrOperand(ir_instr.oper2));
                InsertLoad(ctx, IrOperand(ir_instr.target), RegOperand(rax));
            }
        } break;
    case IR_Div:
        {
            if (is_float)
            {
                if (ir_instr.target != ir_instr.oper1)
                    InsertLoad(ctx, ir_instr.target, ir_instr.oper1);
                Amd64_Opcode div_op = (ltype->tag == TYP_f32) ? OP_divss : OP_divsd;
                InsertInstruction(ctx, div_op, ir_instr.target, ir_instr.oper2);
            }
            else if (is_signed)
            {
                Reg rax = MakeReg("rax");
                Reg rdx = MakeReg("rdx");
                InsertZeroReg(ctx, rdx);
                InsertLoad(ctx, RegOperand(rax), IrOperand(ir_instr.oper1));
                InsertInstruction(ctx, OP_idiv, IrOperand(ir_instr.oper2));
                InsertLoad(ctx, IrOperand(ir_instr.target), RegOperand(rax));
            }
            else // unsigned
            {
                Reg rax = MakeReg("rax");
                Reg rdx = MakeReg("rdx");
                InsertZeroReg(ctx, rdx);
                InsertLoad(ctx, RegOperand(rax), IrOperand(ir_instr.oper1));
                InsertInstruction(ctx, OP_div, IrOperand(ir_instr.oper2));
                InsertLoad(ctx, IrOperand(ir_instr.target), RegOperand(rax));
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
                Reg rax = MakeReg("rax");
                Reg rdx = MakeReg("rdx");
                InsertZeroReg(ctx, rdx);
                InsertLoad(ctx, RegOperand(rax), IrOperand(ir_instr.oper1));
                InsertInstruction(ctx, OP_idiv, IrOperand(ir_instr.oper2));
                InsertLoad(ctx, IrOperand(ir_instr.target), RegOperand(rdx));
            }
            else // unsigned
            {
                Reg rax = MakeReg("rax");
                Reg rdx = MakeReg("rdx");
                InsertZeroReg(ctx, rdx);
                InsertLoad(ctx, RegOperand(rax), IrOperand(ir_instr.oper1));
                InsertInstruction(ctx, OP_div, IrOperand(ir_instr.oper2));
                InsertLoad(ctx, IrOperand(ir_instr.target), RegOperand(rdx));
            }
        } break;

    case IR_And:
        InsertInstruction(ctx, OP_and, ir_instr.oper1, ir_instr.oper2);
        break;
    case IR_Or:
        InsertInstruction(ctx, OP_or, ir_instr.oper1, ir_instr.oper2);
        break;
    case IR_Xor:
        InsertInstruction(ctx, OP_xor, ir_instr.oper1, ir_instr.oper2);
        break;

    case IR_Neg:
        if (is_float)
        {
            if (ltype->tag == TYP_f32)
            {
                InsertLoad(ctx, IrOperand(ir_instr.target), ImmOperand(0.0f));
                InsertInstruction(ctx, OP_subss, ir_instr.target, ir_instr.oper1);
            }
            else if (ltype->tag == TYP_f64)
            {
                InsertLoad(ctx, IrOperand(ir_instr.target), ImmOperand(0.0));
                InsertInstruction(ctx, OP_subsd, ir_instr.target, ir_instr.oper1);
            }
        }
        else
        {
            InsertLoad(ctx, ir_instr.target, ir_instr.oper1);
            InsertInstruction(ctx, OP_neg, IrOperand(ir_instr.target));
        }
    case IR_Not:
    case IR_Compl:
        InsertLoad(ctx, ir_instr.target, ir_instr.oper1);
        InsertInstruction(ctx, OP_not, IrOperand(ir_instr.target));
    }
}

static void GenerateCode(Codegen_Context *ctx, Ir_Instruction ir_instr)
{
    switch (ir_instr.opcode)
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
            InsertLoad(ctx, IrOperand(ir_instr.target), IrAddrOperand(ir_instr.oper1, 0));
            break;

        case IR_Addr:
            NOT_IMPLEMENTED("IR addr");
            break;

        case IR_Param:
            NOT_IMPLEMENTED("IR param");
            break;

        case IR_Mov:
            // mov target, oper1
            InsertLoad(ctx, ir_instr.target, ir_instr.oper1);
            break;
        case IR_MovMember:
            {
                s64 offset_of_member = GetStructMemberOffset(ir_instr.oper1.type, ir_instr.oper2.imm_s64);
                InsertLoadAddr(ctx, IrOperand(ir_instr.target), IrAddrOperand(ir_instr.oper1, offset_of_member));
            } break;

        case IR_Call:
            InsertInstruction(ctx, OP_call, IrOperand(ir_instr.oper1));
            InsertLoad(ctx, IrOperand(ir_instr.target), RegOperand(MakeReg("rax")));
            break;
        case IR_CallForeign:
            NOT_IMPLEMENTED("IR call foreign");
            InsertInstruction(ctx, OP_call, IrOperand(ir_instr.oper1));
            InsertLoad(ctx, IrOperand(ir_instr.target), RegOperand(MakeReg("rax")));
            break;

        case IR_Jump:
            InsertInstruction(ctx, OP_jmp, IrOperand(ir_instr.target));
            break;
        case IR_Jz:
            InsertInstruction(ctx, OP_cmp, IrOperand(ir_instr.oper1), ImmOperand((s64)0));
            InsertInstruction(ctx, OP_je, IrOperand(ir_instr.target));
            break;
        case IR_Jnz:
            InsertInstruction(ctx, OP_cmp, IrOperand(ir_instr.oper1), ImmOperand((s64)0));
            InsertInstruction(ctx, OP_jne, IrOperand(ir_instr.target));
            break;
        case IR_Return:
            InsertLoad(ctx, RegOperand(MakeReg("rax")), IrOperand(ir_instr.target));
            InsertInstruction(ctx, OP_ret);
            break;

        case IR_S_TO_F32:
            InsertInstruction(ctx, OP_cvtsi2ss, ir_instr.target, ir_instr.oper1);
            break;
        case IR_S_TO_F64:
            InsertInstruction(ctx, OP_cvtsi2sd, ir_instr.target, ir_instr.oper1);
            break;
        case IR_F32_TO_S:
            InsertInstruction(ctx, OP_cvtss2si, ir_instr.target, ir_instr.oper1);
            break;
        case IR_F64_TO_S:
            InsertInstruction(ctx, OP_cvtsd2si, ir_instr.target, ir_instr.oper1);
            break;
        case IR_F32_TO_F64:
            InsertInstruction(ctx, OP_cvtss2sd, ir_instr.target, ir_instr.oper1);
            break;
        case IR_F64_TO_F32:
            InsertInstruction(ctx, OP_cvtsd2ss, ir_instr.target, ir_instr.oper1);
            break;
    }
}

static void GenerateCode(Codegen_Context *ctx, Ir_Routine *routine)
{
    InsertLabel(ctx, routine->name);
    for (s64 i = 0; i < routine->instructions.count; i++)
    {
        Ir_Instruction ir_instr = array::At(routine->instructions, i);
        GenerateCode(ctx, ir_instr);
    }
}

void GenerateCode_Amd64(Codegen_Context *ctx, Ir_Routine_List routines)
{
    //for (s64 i = 0; i < routines.count; i++)
    //{
    //    fprintf((FILE*)ctx->code_out, "global ");
    //    PrintName(ctx->code_out, array::At(routines, i)->name);
    //    fprintf((FILE*)ctx->code_out, "\n");
    //}
    //fprintf((FILE*)ctx->code_out, "\nsection .text\n\n");
    for (s64 i = 0; i < routines.count; i++)
    {
        GenerateCode(ctx, array::At(routines, i));
    }
}

// Print code

static void PrintOpcode(Codegen_Context *ctx, Amd64_Opcode opcode)
{
    fprintf((FILE*)ctx->code_out, "%s", opcode_names[opcode]);
}

static void PrintPtr(FILE *file, void *ptr)
{
    if (ptr)
        fprintf(file, "%p", ptr);
    else
        fprintf(file, "(null)");
}

static void PrintBool(FILE *file, bool value)
{
    fprintf(file, "%s", (value ? "(true)" : "(false)"));
}

static void PrintIrImmediate(FILE *file, Ir_Operand oper)
{
    switch (oper.ir_type)
    {
        case IR_TYP_ptr:    PrintPtr(file, oper.imm_ptr); break;
        case IR_TYP_bool:   PrintBool(file, oper.imm_bool); break;
        case IR_TYP_u8:     fprintf(file, "%u", oper.imm_u8); break;
        case IR_TYP_s8:     fprintf(file, "%d", oper.imm_s8); break;
        case IR_TYP_u16:    fprintf(file, "%u", oper.imm_u16); break;
        case IR_TYP_s16:    fprintf(file, "%d", oper.imm_s16); break;
        case IR_TYP_u32:    fprintf(file, "%u", oper.imm_u32); break;
        case IR_TYP_s32:    fprintf(file, "%d", oper.imm_s32); break;
        case IR_TYP_u64:    fprintf(file, "%" PRIu64, oper.imm_u64); break;
        case IR_TYP_s64:    fprintf(file, "%" PRId64, oper.imm_s64); break;
        case IR_TYP_f32:    fprintf(file, "%ff", oper.imm_f32); break;
        case IR_TYP_f64:    fprintf(file, "%fd", oper.imm_f64); break;
        case IR_TYP_str:
            fprintf(file, "\"");
            PrintString((IoFile*)file, oper.imm_str);
            fprintf(file, "\"");
            break;

        case IR_TYP_struct:
        case IR_TYP_routine:
            INVALID_CODE_PATH;
            break;
    }
}

static void PrintIrLabel(FILE *file, Ir_Operand label_oper)
{
    fprintf(file, "L:%" PRId64, label_oper.label->target_loc);
}

static void PrintIrOperand(IoFile *file, Ir_Operand oper)
{
    switch (oper.oper_type)
    {
        case IR_OPER_None:
            fprintf((FILE*)file, "_");
            break;
        case IR_OPER_Variable:
            fprintf((FILE*)file, "{");
            PrintName(file, oper.var.name);
            fprintf((FILE*)file, "}");
            break;
        case IR_OPER_Temp:
            fprintf((FILE*)file, "@temp%" PRId64, oper.temp.temp_id);
            break;
        case IR_OPER_Immediate:
            PrintIrImmediate((FILE*)file, oper);
            break;
        case IR_OPER_Label:
            PrintIrLabel((FILE*)file, oper);
            break;
        case IR_OPER_Routine:
        case IR_OPER_ForeignRoutine:
            fprintf((FILE*)file, "<");
            //len += PrintName(file, oper.routine->name, 15);
            PrintName(file, oper.var.name);
            fprintf((FILE*)file, ">");
            break;
    }
}


static void PrintOperand(Codegen_Context *ctx, Operand oper, b32 first)
{
    if (oper.type == OT_None) return;

    if (!first)
        fprintf((FILE*)ctx->code_out, ", ");
    switch (oper.type)
    {
        case OT_None:
            break;
        case OT_Label:
            break;
        case OT_Register:
            PrintName(ctx->code_out, oper.reg.name);
            break;
        case OT_IrOperand:
            PrintIrOperand(ctx->code_out, oper.ir_oper);
            break;
        case OT_Immediate:
            fprintf((FILE*)ctx->code_out, "%" PRIu64, oper.imm_u64);
            break;
        case OT_Addr:
            fprintf((FILE*)ctx->code_out, "[");
            PrintName(ctx->code_out, oper.base_offs.base.name);
            if (oper.base_offs.offset > 0)
                fprintf((FILE*)ctx->code_out, "+");
            fprintf((FILE*)ctx->code_out, "%" PRId64, oper.base_offs.offset);
            fprintf((FILE*)ctx->code_out, "]");
            break;
        case OT_IrAddrOper:
            fprintf((FILE*)ctx->code_out, "[");
            PrintIrOperand(ctx->code_out, oper.ir_base_offs.base);
            if (oper.base_offs.offset > 0)
                fprintf((FILE*)ctx->code_out, "+");
            fprintf((FILE*)ctx->code_out, "%" PRId64, oper.ir_base_offs.offset);
            fprintf((FILE*)ctx->code_out, "]");
            break;
    }
}

static void PrintLabel(IoFile *file, Operand label_oper)
{
    PrintString(file, label_oper.label.name.str);
    fprintf((FILE*)file, ":");
}

static void PrintInstruction(Codegen_Context *ctx, const Instruction instr)
{
    if ((Amd64_Opcode)instr.opcode == OP_LABEL)
    {
        PrintLabel(ctx->code_out, instr.oper1);
    }
    else
    {
        fprintf((FILE*)ctx->code_out, "\t");
        PrintOpcode(ctx, (Amd64_Opcode)instr.opcode);
        fprintf((FILE*)ctx->code_out, "\t");
        PrintOperand(ctx, instr.oper1, true);
        PrintOperand(ctx, instr.oper2, false);
        PrintOperand(ctx, instr.oper3, false);
    }
    fprintf((FILE*)ctx->code_out, "\n");
}

void OutputCode_Amd64(Codegen_Context *ctx)
{
    for (s64 i = 0; i < ctx->instructions.count; i++)
    {
        PrintInstruction(ctx, array::At(ctx->instructions, i));
    }
}

} // hplang
