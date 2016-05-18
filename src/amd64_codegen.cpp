
#include "amd64_codegen.h"
#include "common.h"

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
    \
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

static Operand OperandFromIrOper(Ir_Operand ir_oper)
{
    Operand result = { };
    result.type = OT_IrOperand;
    result.ir_oper = ir_oper;
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

static void InsertLoad(Codegen_Context *ctx, Operand oper1, Operand oper2)
{
    InsertInstruction(ctx, OP_mov, oper1, oper2);
}

static void InsertLoad(Codegen_Context *ctx, Ir_Operand ir_oper1, Ir_Operand ir_oper2)
{
    InsertLoad(ctx, OperandFromIrOper(ir_oper1), OperandFromIrOper(ir_oper2));
}

static void InsertLabel(Codegen_Context *ctx, Name name)
{
    Operand oper = { };
    oper.label.name = name;
    InsertInstruction(ctx, OP_LABEL, oper);
}


static void GenerateCode(Codegen_Context *ctx, Ir_Instruction ir_instr)
{
    switch (ir_instr.opcode)
    {
        case IR_Add:
            if (ir_instr.target != ir_instr.oper1)
                InsertLoad(ctx, ir_instr.target, ir_instr.oper1);
            InsertInstruction(ctx, OP_add,
                    OperandFromIrOper(ir_instr.target),
                    OperandFromIrOper(ir_instr.oper2));
            break;
        case IR_Sub:
            if (ir_instr.target != ir_instr.oper1)
                InsertLoad(ctx, ir_instr.target, ir_instr.oper1);
            InsertInstruction(ctx, OP_sub,
                    OperandFromIrOper(ir_instr.target),
                    OperandFromIrOper(ir_instr.oper2));
            break;
        case IR_Mov:
            InsertLoad(ctx, ir_instr.target, ir_instr.oper1);
            break;

        case IR_Call:
            InsertInstruction(ctx, OP_call, OperandFromIrOper(ir_instr.oper1));
            InsertLoad(ctx, OperandFromIrOper(ir_instr.target), RegOperand(MakeReg("rax")));
            break;
        case IR_Jump:
            InsertInstruction(ctx, OP_jmp, OperandFromIrOper(ir_instr.target));
            break;
        case IR_Jz:
            InsertInstruction(ctx, OP_cmp, OperandFromIrOper(ir_instr.oper1), ImmOperand((s64)0));
            InsertInstruction(ctx, OP_je, OperandFromIrOper(ir_instr.target));
            break;
        case IR_Return:
            InsertInstruction(ctx, OP_ret);
            break;

        case IR_S_TO_F32:
            InsertInstruction(ctx, OP_cvtsi2ss,
                    OperandFromIrOper(ir_instr.target),
                    OperandFromIrOper(ir_instr.oper1));
            break;
        case IR_S_TO_F64:
            InsertInstruction(ctx, OP_cvtsi2sd,
                    OperandFromIrOper(ir_instr.target),
                    OperandFromIrOper(ir_instr.oper1));
            break;
        case IR_F32_TO_S:
            InsertInstruction(ctx, OP_cvtss2si,
                    OperandFromIrOper(ir_instr.target),
                    OperandFromIrOper(ir_instr.oper1));
            break;
        case IR_F64_TO_S:
            InsertInstruction(ctx, OP_cvtsd2si,
                    OperandFromIrOper(ir_instr.target),
                    OperandFromIrOper(ir_instr.oper1));
            break;
        case IR_F32_TO_F64:
            InsertInstruction(ctx, OP_cvtss2sd,
                    OperandFromIrOper(ir_instr.target),
                    OperandFromIrOper(ir_instr.oper1));
            break;
        case IR_F64_TO_F32:
            InsertInstruction(ctx, OP_cvtsd2ss,
                    OperandFromIrOper(ir_instr.target),
                    OperandFromIrOper(ir_instr.oper1));
            break;
        default:
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
