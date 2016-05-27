
#include "amd64_codegen.h"
#include "common.h"
#include "compiler.h"
#include "reg_alloc.h"
#include "symbols.h"
#include "hashtable.h"

#include <cstdio>
#include <cinttypes>

namespace hplang
{

// Disabled or non-valid instruction opcode
#define PASTE_OP_D(x)

// NOTE(henrik): Conditional move opcode cmovg is not valid when the operands
// are 64 bit wide. The condition "cmovg a, b" can be replaced with "cmovl b, a".
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
    PASTE_OP_D(cmovg)\
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
const char *reg_name_strings[] = {
    REGS
};
#undef PASTE_REG

#define PASTE_REG(r) {},
Name reg_names[] = {
    REGS
};
#undef PASTE_REG

static const char* GetRegNameStr(Reg reg)
{
    return reg_name_strings[reg.reg_index];
}

static const Name GetRegName(Reg reg)
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
    for (s64 i = 0; i < array_length(reg_names); i++)
    {
        reg_names[i] = PushName(&ctx->arena, reg_name_strings[i]);
    }
    switch (cg_target)
    {
    case CGT_COUNT:
        INVALID_CODE_PATH;
        break;

    case CGT_AMD64_Windows:
        InitRegAlloc(ctx->reg_alloc,
                16 + 16,
                array_length(general_regs), general_regs,
                array_length(float_regs), float_regs,
                array_length(win_arg_regs), win_arg_regs,
                array_length(win_float_arg_regs), win_float_arg_regs,
                array_length(win_caller_save), win_caller_save,
                array_length(win_callee_save), win_callee_save);
        break;
    case CGT_AMD64_Unix:
        InitRegAlloc(ctx->reg_alloc,
                16 + 16,
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
    result.type = Oper_Type::None;
    return result;
}

static Operand R_(Operand oper)
{
    oper.access_flags = AF_Read;
    return oper;
}

static Operand W_(Operand oper)
{
    oper.access_flags = AF_Write;
    return oper;
}

//static Operand RW_(Operand oper)
//{
//    oper.access_flags = AF_ReadWrite;
//    return oper;
//}

static Operand RegOperand(Reg reg, Oper_Data_Type data_type, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Register;
    result.access_flags = access_flags;
    result.data_type = data_type;
    result.reg = reg;
    return result;
}

static Operand RegOperand(Amd64_Register reg, Oper_Data_Type data_type, Oper_Access_Flags access_flags)
{
    return RegOperand(MakeReg(reg), data_type, access_flags);
}

static Operand FixedRegOperand(Codegen_Context *ctx, Reg reg, Oper_Data_Type data_type, Oper_Access_Flags access_flags)
{
    const s64 buf_size = 40;
    char buf[buf_size];
    s64 reg_name_len = snprintf(buf, buf_size, "%s@%" PRId64,
            GetRegNameStr(reg), ctx->fixed_reg_id);
    ctx->fixed_reg_id++;

    Operand result = { };
    result.type = Oper_Type::FixedRegister;
    result.access_flags = access_flags;
    result.data_type = data_type;
    result.fixed_reg.reg = reg;
    result.fixed_reg.name = PushName(&ctx->arena, buf, reg_name_len);
    return result;
}

static Operand FixedRegOperand(Codegen_Context *ctx, Amd64_Register reg, Oper_Access_Flags access_flags)
{
    return FixedRegOperand(ctx, MakeReg(reg), Oper_Data_Type::U64, access_flags);
}

static Operand TempOperand(Codegen_Context *ctx, Oper_Data_Type data_type, Oper_Access_Flags access_flags)
{
    const s64 buf_size = 40;
    char buf[buf_size];
    s64 temp_name_len = snprintf(buf, buf_size, "cg_temp@%" PRId64, ctx->temp_id);
    ctx->temp_id++;

    Operand result = { };
    result.type = Oper_Type::Temp;
    result.access_flags = access_flags;
    result.data_type = data_type;
    result.temp.name = PushName(&ctx->arena, buf, temp_name_len);
    return result;
}

static Operand TempFloat32Operand(Codegen_Context *ctx, Oper_Access_Flags access_flags)
{
    return TempOperand(ctx, Oper_Data_Type::F64, access_flags);
}

static Operand TempFloat64Operand(Codegen_Context *ctx, Oper_Access_Flags access_flags)
{
    return TempOperand(ctx, Oper_Data_Type::F32, access_flags);
}

//static Operand AddrOperand(Reg base, s64 offset, Oper_Access_Flags access_flags)
//{
//    Operand result = { };
//    result.type = Oper_Type::Addr;
//    result.access_flags = access_flags;
//    result.data_type = Oper_Data_Type::PTR;
//    result.base_index_offs.base = base;
//    result.base_index_offs.offset = offset;
//    return result;
//}
//
//static Operand AddrOperand(Amd64_Register base, s64 offset, Oper_Access_Flags access_flags)
//{
//    return AddrOperand(MakeReg(base), offset, access_flags);
//}
//
//static Operand AddrOperand(Reg base, Reg index, s64 scale, s64 offset, Oper_Access_Flags access_flags)
//{
//    Operand result = { };
//    result.type = Oper_Type::Addr;
//    result.access_flags = access_flags;
//    result.data_type = Oper_Data_Type::PTR;
//    result.base_index_offs.base = base;
//    result.base_index_offs.index = index;
//    result.base_index_offs.scale = scale;
//    result.base_index_offs.offset = offset;
//    return result;
//}

static Operand ImmOperand(void *imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Immediate;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::PTR;
    result.imm_ptr = imm;
    return result;
}

static Operand ImmOperand(bool imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Immediate;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::BOOL;
    result.imm_bool = imm;
    return result;
}

static Operand ImmOperand(u8 imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Immediate;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::U8;
    result.imm_u8 = imm;
    return result;
}

static Operand ImmOperand(s8 imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Immediate;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::S8;
    result.imm_s8 = imm;
    return result;
}

static Operand ImmOperand(u16 imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Immediate;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::U16;
    result.imm_u16 = imm;
    return result;
}

static Operand ImmOperand(s16 imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Immediate;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::S16;
    result.imm_s16 = imm;
    return result;
}

static Operand ImmOperand(u32 imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Immediate;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::U32;
    result.imm_u32 = imm;
    return result;
}

static Operand ImmOperand(s32 imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Immediate;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::S32;
    result.imm_s32 = imm;
    return result;
}

static Operand ImmOperand(u64 imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Immediate;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::U64;
    result.imm_u64 = imm;
    return result;
}

static Operand ImmOperand(s64 imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Immediate;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::S64;
    result.imm_s64 = imm;
    return result;
}

static Operand ImmOperand(f32 imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Immediate;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::F32;
    result.imm_f32 = imm;
    return result;
}

static Operand ImmOperand(f64 imm, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Immediate;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::F64;
    result.imm_f64 = imm;
    return result;
}

static Operand LabelOperand(Name name, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::Label;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::PTR;
    result.label.name = name;
    return result;
}

static Operand LabelOperand(Ir_Operand *ir_oper, Oper_Access_Flags access_flags)
{
    ASSERT(ir_oper->oper_type == IR_OPER_Label ||
            ir_oper->oper_type == IR_OPER_Routine ||
            ir_oper->oper_type == IR_OPER_ForeignRoutine);
    Operand result = { };
    result.type = Oper_Type::Label;
    result.access_flags = access_flags;
    result.data_type = Oper_Data_Type::PTR;
    switch (ir_oper->oper_type)
    {
    default: break;
    case IR_OPER_Label:
        result.label.name = ir_oper->label->name;
        break;
    case IR_OPER_Routine:
    case IR_OPER_ForeignRoutine:
        result.label.name = ir_oper->var.name;
        break;
    }
    ASSERT(result.label.name.str.size != 0);
    return result;
}

static Oper_Data_Type DataTypeFromType(Type *type)
{
    ASSERT(type != nullptr);
    switch (type->tag)
    {
        case TYP_none:
            INVALID_CODE_PATH;
            break;
        case TYP_pending:
            return DataTypeFromType(type->base_type);
        case TYP_null:
            return Oper_Data_Type::PTR;

        case TYP_void:
            INVALID_CODE_PATH;
            break;

        case TYP_pointer:
            return Oper_Data_Type::PTR;
        case TYP_bool:
            return Oper_Data_Type::BOOL;
        case TYP_char:
            return Oper_Data_Type::U8;
        case TYP_u8:
            return Oper_Data_Type::U8;
        case TYP_s8:
            return Oper_Data_Type::S8;
        case TYP_u16:
            return Oper_Data_Type::U16;
        case TYP_s16:
            return Oper_Data_Type::S16;
        case TYP_u32:
            return Oper_Data_Type::U32;
        case TYP_s32:
            return Oper_Data_Type::S32;
        case TYP_u64:
            return Oper_Data_Type::U64;
        case TYP_s64:
            return Oper_Data_Type::S64;
        case TYP_f32:
            return Oper_Data_Type::F32;
        case TYP_f64:
            return Oper_Data_Type::F64;
        case TYP_string:
            return Oper_Data_Type::PTR;
        case TYP_Function:
            return Oper_Data_Type::PTR;
        case TYP_Struct:
            return Oper_Data_Type::PTR;
    }
    return Oper_Data_Type::PTR;
}

static Operand IrOperand(Ir_Operand *ir_oper, Oper_Access_Flags access_flags)
{
    Operand result = { };
    if (ir_oper->oper_type == IR_OPER_Label)
    {
        return LabelOperand(ir_oper->label->name, access_flags);
    }
    else if (ir_oper->oper_type == IR_OPER_Immediate)
    {
        switch (ir_oper->type->tag)
        {
            case TYP_none:
            case TYP_pending:
            case TYP_null:
                INVALID_CODE_PATH;
                break;
            case TYP_pointer:
                return ImmOperand(ir_oper->imm_ptr, access_flags);
            case TYP_bool:
                return ImmOperand(ir_oper->imm_bool, access_flags);
            case TYP_char:
                return ImmOperand(ir_oper->imm_u8, access_flags);
            case TYP_u8:
                return ImmOperand(ir_oper->imm_u8, access_flags);
            case TYP_s8:
                return ImmOperand(ir_oper->imm_s8, access_flags);
            case TYP_u16:
                return ImmOperand(ir_oper->imm_u16, access_flags);
            case TYP_s16:
                return ImmOperand(ir_oper->imm_s16, access_flags);
            case TYP_u32:
                return ImmOperand(ir_oper->imm_u32, access_flags);
            case TYP_s32:
                return ImmOperand(ir_oper->imm_s32, access_flags);
            case TYP_u64:
                return ImmOperand(ir_oper->imm_u64, access_flags);
            case TYP_s64:
                return ImmOperand(ir_oper->imm_s64, access_flags);
            case TYP_f32:
                return ImmOperand(ir_oper->imm_f32, access_flags);
            case TYP_f64:
                return ImmOperand(ir_oper->imm_f64, access_flags);
            default:
                break;
        }
    }
    result.type = Oper_Type::IrOperand;
    result.access_flags = access_flags;
    result.data_type = DataTypeFromType(ir_oper->type);
    result.ir_oper = ir_oper;
    return result;
}

//static Operand IrAddrOperand(Ir_Operand *base, s64 offset, Oper_Access_Flags access_flags)
//{
//    Operand result = { };
//    result.type = Oper_Type::IrAddrOper;
//    result.access_flags = access_flags;
//    result.data_type = Oper_Data_Type::PTR;
//    result.ir_base_index_offs.base = base;
//    result.ir_base_index_offs.offset = offset;
//    return result;
//}

//static Operand IrAddrOperand(Ir_Operand *base, Ir_Operand *index, s64 scale, s64 offset, Oper_Access_Flags access_flags)
//{
//    Operand result = { };
//    result.type = Oper_Type::IrAddrOper;
//    result.access_flags = access_flags;
//    result.data_type = Oper_Data_Type::PTR;
//    result.ir_base_index_offs.base = base;
//    result.ir_base_index_offs.index = index;
//    result.ir_base_index_offs.scale = scale;
//    result.ir_base_index_offs.offset = offset;
//    return result;
//}

static Operand BaseOffsetOperand(Operand base, s64 offset, Oper_Access_Flags access_flags)
{
    ASSERT(base.scale_offset == 0);
    Operand result = base;
    result.access_flags = access_flags;
    result.addr_mode = Oper_Addr_Mode::BaseOffset;
    result.scale_offset = offset;
    return result;
}

static Operand BaseOffsetOperand(Amd64_Register base, s64 offset, Oper_Access_Flags access_flags)
{
    Operand base_reg = RegOperand(base, Oper_Data_Type::PTR, access_flags);
    return BaseOffsetOperand(base_reg, offset, access_flags);
}

static Operand BaseOffsetOperand(Ir_Operand *base, s64 offset, Oper_Access_Flags access_flags)
{
    Operand base_oper = IrOperand(base, access_flags);
    return BaseOffsetOperand(base_oper, offset, access_flags);
}

static Operand BaseIndexOffsetOperand(Operand base, s64 offset, Oper_Access_Flags access_flags)
{
    ASSERT(base.scale_offset == 0);
    Operand result = base;
    result.access_flags = access_flags;
    result.addr_mode = Oper_Addr_Mode::BaseIndexOffset;
    result.scale_offset = offset;
    return result;
}

//static Operand BaseIndexOffsetOperand(Amd64_Register base, s64 offset, Oper_Access_Flags access_flags)
//{
//    Operand base_reg = RegOperand(base, Oper_Data_Type::PTR, access_flags);
//    return BaseIndexOffsetOperand(base_reg, offset, access_flags);
//}

static Operand BaseIndexOffsetOperand(Ir_Operand *base, s64 offset, Oper_Access_Flags access_flags)
{
    Operand base_oper = IrOperand(base, access_flags);
    return BaseIndexOffsetOperand(base_oper, offset, access_flags);
}

static Operand IndexScaleOperand(Operand index, s64 scale, Oper_Access_Flags access_flags)
{
    ASSERT(index.scale_offset == 0);
    Operand result = index;
    result.access_flags = access_flags;
    result.addr_mode = Oper_Addr_Mode::IndexScale;
    result.scale_offset = scale;
    return result;
}

//static Operand IndexScaleOperand(Amd64_Register index, s64 scale, Oper_Access_Flags access_flags)
//{
//    Operand index_reg = RegOperand(index, Oper_Data_Type::PTR, access_flags);
//    return IndexScaleOperand(index_reg, scale, access_flags);
//}

static Operand IndexScaleOperand(Ir_Operand *index, s64 scale, Oper_Access_Flags access_flags)
{
    Operand index_oper = IrOperand(index, access_flags);
    return IndexScaleOperand(index_oper, scale, access_flags);
}


static inline Instruction* NewInstruction(
        Codegen_Context *ctx,
        Amd64_Opcode opcode,
        Operand oper1 = NoneOperand(),
        Operand oper2 = NoneOperand(),
        Operand oper3 = NoneOperand())
{
    Instruction *instr = PushStruct<Instruction>(&ctx->arena);
    instr->opcode = (Opcode)opcode;
    instr->oper1 = oper1;
    instr->oper2 = oper2;
    instr->oper3 = oper3;
    if (ctx->comment)
    {
        instr->comment = *ctx->comment;
        ctx->comment = nullptr;
    }
    Instr_Flags flags = { };
    switch (opcode)
    {
        default:
            flags = IF_FallsThrough; break;
        case OP_jmp:
            flags = IF_Branch; break;
        case OP_je:
            flags = IF_FallsThrough;
            flags = flags | IF_Branch; break;
        case OP_jne:
            flags = IF_FallsThrough;
            flags = flags | IF_Branch; break;
    }
    instr->flags = flags;
    return instr;
}

static Instruction* LoadFloat32Imm(Codegen_Context *ctx, Operand dest, f32 value)
{
    s64 buf_size = 40;
    char buf[buf_size];
    s64 name_len = snprintf(buf, buf_size, "f32@%" PRId64 "", ctx->float32_consts.count);
    Name label_name = PushName(&ctx->arena, buf, name_len);

    Float32_Const fconst = { };
    fconst.label_name = label_name;
    fconst.value = value;
    array::Push(ctx->float32_consts, fconst);

    Operand float_label = LabelOperand(label_name, AF_Read);
    float_label.addr_mode = Oper_Addr_Mode::BaseOffset;
    return NewInstruction(ctx, OP_movss, dest, float_label);
}

static Instruction* LoadFloat64Imm(Codegen_Context *ctx, Operand dest, f64 value)
{
    s64 buf_size = 40;
    char buf[buf_size];
    s64 name_len = snprintf(buf, buf_size, "f64@%" PRId64 "", ctx->float64_consts.count);
    Name label_name = PushName(&ctx->arena, buf, name_len);

    Float64_Const fconst = { };
    fconst.label_name = label_name;
    fconst.value = value;
    array::Push(ctx->float64_consts, fconst);

    Operand float_label = LabelOperand(label_name, AF_Read);
    float_label.addr_mode = Oper_Addr_Mode::BaseOffset;
    return NewInstruction(ctx, OP_movsd, dest, float_label);
}

static Operand NewStringImm(Codegen_Context *ctx, String value, Oper_Access_Flags access_flags)
{
    s64 buf_size = 40;
    char buf[buf_size];
    s64 name_len = snprintf(buf, buf_size, "str@%" PRId64 "", ctx->str_consts.count);
    Name label_name = PushName(&ctx->arena, buf, name_len);

    String_Const sconst = { };
    sconst.label_name = label_name;
    sconst.value = value;
    array::Push(ctx->str_consts, sconst);

    Operand str_label = LabelOperand(label_name, access_flags);
    return str_label;
}

static Operand StripImmediates(Codegen_Context *ctx,
        Instruction_List &instructions,
        s64 instr_index,
        Operand oper)
{
    if (oper.type == Oper_Type::IrOperand && oper.ir_oper->oper_type == IR_OPER_Immediate)
    {
        Oper_Access_Flags access_flags = oper.access_flags;
        Instruction *load_const = nullptr;
        if (oper.ir_oper->type->tag == TYP_f32)
        {
            Operand dest = TempFloat32Operand(ctx, AF_Write);
            load_const = LoadFloat32Imm(ctx, dest, oper.ir_oper->imm_f32);
            array::Insert(instructions, instr_index, load_const);
            dest.access_flags = access_flags;
            dest.addr_mode = oper.addr_mode;
            dest.scale_offset = oper.scale_offset;
            return dest;
        }
        else if (oper.ir_oper->type->tag == TYP_f64)
        {
            Operand dest = TempFloat64Operand(ctx, AF_Write);
            load_const = LoadFloat64Imm(ctx, dest, oper.ir_oper->imm_f64);
            array::Insert(instructions, instr_index, load_const);
            dest.access_flags = access_flags;
            dest.addr_mode = oper.addr_mode;
            dest.scale_offset = oper.scale_offset;
            return dest;
        }
        else if (TypeIsString(oper.ir_oper->type))
        {
            Operand dest = TempOperand(ctx, Oper_Data_Type::PTR, AF_Write);
            Operand str = NewStringImm(ctx, oper.ir_oper->imm_str, AF_Read);
            load_const = NewInstruction(ctx, OP_mov, dest, str);
            array::Insert(instructions, instr_index, load_const);
            dest.access_flags = access_flags;
            dest.addr_mode = oper.addr_mode;
            dest.scale_offset = oper.scale_offset;
            return dest;
        }
    }
    return oper;
}

static void PushInstruction(Codegen_Context *ctx,
        Instruction_List &instructions,
        Amd64_Opcode opcode,
        Operand oper1 = NoneOperand(),
        Operand oper2 = NoneOperand(),
        Operand oper3 = NoneOperand())
{
    oper1 = StripImmediates(ctx, instructions, instructions.count, oper1);
    oper2 = StripImmediates(ctx, instructions, instructions.count, oper2);
    oper3 = StripImmediates(ctx, instructions, instructions.count, oper3);
    Instruction *instr = NewInstruction(ctx, opcode, oper1, oper2, oper3);
    array::Push(instructions, instr);
}

static void PushInstruction(Codegen_Context *ctx,
        Amd64_Opcode opcode,
        Operand oper1 = NoneOperand(),
        Operand oper2 = NoneOperand(),
        Operand oper3 = NoneOperand())
{
    PushInstruction(ctx, ctx->current_routine->instructions,
            opcode, oper1, oper2, oper3);
}

static void InsertInstruction(Codegen_Context *ctx,
        Instruction_List &instructions,
        s64 &instr_index,
        Amd64_Opcode opcode,
        Operand oper1 = NoneOperand(),
        Operand oper2 = NoneOperand(),
        Operand oper3 = NoneOperand())
{
    oper1 = StripImmediates(ctx, instructions, instr_index, oper1);
    oper2 = StripImmediates(ctx, instructions, instr_index, oper2);
    oper3 = StripImmediates(ctx, instructions, instr_index, oper3);
    Instruction *instr = NewInstruction(ctx, opcode, oper1, oper2, oper3);
    array::Insert(instructions, instr_index, instr);
    instr_index++;
}

static void PushEpilogue(Codegen_Context *ctx,
        Amd64_Opcode opcode,
        Operand oper1 = NoneOperand(),
        Operand oper2 = NoneOperand(),
        Operand oper3 = NoneOperand())
{
    PushInstruction(ctx, ctx->current_routine->epilogue,
            opcode, oper1, oper2, oper3);
}

static void PushPrologue(Codegen_Context *ctx,
        Amd64_Opcode opcode,
        Operand oper1 = NoneOperand(),
        Operand oper2 = NoneOperand(),
        Operand oper3 = NoneOperand())
{
    PushInstruction(ctx, ctx->current_routine->prologue,
            opcode, oper1, oper2, oper3);
}

static void InsertLoad(Codegen_Context *ctx,
        Instruction_List &instructions, s64 &instr_index,
        Operand oper1, Operand oper2)
{
    bool load_f32 = (oper1.data_type == Oper_Data_Type::F32);
    bool load_f64 = (oper1.data_type == Oper_Data_Type::F64);
    if (!load_f32 && !load_f64)
    {
        load_f32 = (oper2.data_type == Oper_Data_Type::F32);
        load_f64 = (oper2.data_type == Oper_Data_Type::F64);
    }

    if (load_f32)
        InsertInstruction(ctx, instructions, instr_index, OP_movss, oper1, oper2);
    else if (load_f64)
        InsertInstruction(ctx, instructions, instr_index, OP_movsd, oper1, oper2);
    else
        InsertInstruction(ctx, instructions, instr_index, OP_mov, oper1, oper2);
}

static void PushLoad(Codegen_Context *ctx,
        Instruction_List &instructions,
        Operand oper1, Operand oper2)
{
    bool load_f32 = (oper1.data_type == Oper_Data_Type::F32);
    bool load_f64 = (oper1.data_type == Oper_Data_Type::F64);
    if (!load_f32 && !load_f64)
    {
        load_f32 = (oper2.data_type == Oper_Data_Type::F32);
        load_f64 = (oper2.data_type == Oper_Data_Type::F64);
    }

    if (load_f32)
        PushInstruction(ctx, instructions, OP_movss, oper1, oper2);
    else if (load_f64)
        PushInstruction(ctx, instructions, OP_movsd, oper1, oper2);
    else
        PushInstruction(ctx, instructions, OP_mov, oper1, oper2);
}

/*
static void PushLoad(Codegen_Context *ctx,
        Instruction_List &instructions,
        Operand oper1, Operand oper2, Operand oper3)
{
    bool load_f32 = (oper1.data_type == Oper_Data_Type::F32);
    bool load_f64 = (oper1.data_type == Oper_Data_Type::F64);
    if (!load_f32 && !load_f64)
    {
        load_f32 = (oper2.data_type == Oper_Data_Type::F32);
        load_f64 = (oper2.data_type == Oper_Data_Type::F64);
        if (!load_f32 && !load_f64)
        {
            load_f32 = (oper3.data_type == Oper_Data_Type::F32);
            load_f64 = (oper3.data_type == Oper_Data_Type::F64);
        }
    }

    if (load_f32)
        PushInstruction(ctx, instructions, OP_movss, oper1, oper2, oper3);
    else if (load_f64)
        PushInstruction(ctx, instructions, OP_movsd, oper1, oper2, oper3);
    else
        PushInstruction(ctx, instructions, OP_mov, oper1, oper2, oper3);
}
*/

static void PushLoad(Codegen_Context *ctx, Operand oper1, Operand oper2)
{
    PushLoad(ctx, ctx->current_routine->instructions, oper1, oper2);
}

static void PushLoad(Codegen_Context *ctx, Ir_Operand *ir_oper1, Ir_Operand *ir_oper2)
{
    PushLoad(ctx, IrOperand(ir_oper1, AF_Write), IrOperand(ir_oper2, AF_Read));
}

static void PushLoadAddr(Codegen_Context *ctx, Operand oper1, Operand oper2)
{
    PushInstruction(ctx, OP_lea, oper1, oper2);
}

static void PushLoadAddr(Codegen_Context *ctx, Operand oper1, Operand oper2, Operand oper3)
{
    PushInstruction(ctx, OP_lea, oper1, oper2, oper3);
}

static void PushZeroReg(Codegen_Context *ctx, Operand oper)
{
    oper.access_flags = AF_Write;
    PushInstruction(ctx, OP_xor, oper, oper);
}

static void PushLabel(Codegen_Context *ctx, Name name)
{
    Operand oper = { };
    oper.type = Oper_Type::Label;
    oper.label.name = name;
    PushInstruction(ctx, OP_LABEL, oper);
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

    PushInstruction(ctx, cmp_op,
            IrOperand(&ir_instr->oper1, AF_Read),
            IrOperand(&ir_instr->oper2, AF_Read));

    Type *ltype = ir_instr->oper1.type;
    b32 signed_or_float = (TypeIsFloat(ltype) || TypeIsSigned(ltype));
    switch (ir_instr->opcode)
    {
    case IR_Eq:
        {
            PushInstruction(ctx, OP_mov,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(false, AF_Read));
            Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
            PushInstruction(ctx, OP_mov, temp, ImmOperand(true, AF_Read));
            PushInstruction(ctx, OP_cmove,
                    IrOperand(&ir_instr->target, AF_Write),
                    R_(temp));
            break;
        }
    case IR_Neq:
        {
            PushInstruction(ctx, OP_mov,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(false, AF_Read));
            Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
            PushInstruction(ctx, OP_mov, temp, ImmOperand(true, AF_Read));
            PushInstruction(ctx, OP_cmovne,
                    IrOperand(&ir_instr->target, AF_Write),
                    R_(temp));
            break;
        }
    case IR_Lt:
        {
            PushInstruction(ctx, OP_mov,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(false, AF_Read));
            Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
            PushInstruction(ctx, OP_mov, temp, ImmOperand(true, AF_Read));
            Amd64_Opcode mov_op = signed_or_float ? OP_cmovl : OP_cmovb;
            PushInstruction(ctx, mov_op,
                    IrOperand(&ir_instr->target, AF_Write),
                    R_(temp));
            break;
        }
    case IR_Leq:
        {
            PushInstruction(ctx, OP_mov,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(false, AF_Read));
            Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
            PushInstruction(ctx, OP_mov, temp, ImmOperand(true, AF_Read));
            Amd64_Opcode mov_op = signed_or_float ? OP_cmovle : OP_cmovbe;
            PushInstruction(ctx, mov_op,
                    IrOperand(&ir_instr->target, AF_Write),
                    R_(temp));
            break;
        }
    case IR_Gt:
        {
            // cmovg is not valid opcode for 64 bit registers/memory operands
            // => reverse the result of cmovl.
            if (signed_or_float)
            {
                PushInstruction(ctx, OP_mov,
                        IrOperand(&ir_instr->target, AF_Write),
                        ImmOperand(true, AF_Read));
                Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
                PushInstruction(ctx, OP_mov, temp, ImmOperand(false, AF_Read));
                PushInstruction(ctx, OP_cmovl,
                        IrOperand(&ir_instr->target, AF_Write),
                        R_(temp));
            }
            else
            {
                PushInstruction(ctx, OP_mov,
                        IrOperand(&ir_instr->target, AF_Write),
                        ImmOperand(false, AF_Read));
                Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
                PushInstruction(ctx, OP_mov, temp, ImmOperand(true, AF_Read));
                PushInstruction(ctx, OP_cmova,
                        IrOperand(&ir_instr->target, AF_Write),
                        R_(temp));
            }
            break;
        }
    case IR_Geq:
        {
            PushInstruction(ctx, OP_mov,
                    IrOperand(&ir_instr->target, AF_Write),
                    ImmOperand(false, AF_Read));
            Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
            PushInstruction(ctx, OP_mov, temp, ImmOperand(true, AF_Read));
            Amd64_Opcode mov_op = signed_or_float ? OP_cmovge : OP_cmovae;
            PushInstruction(ctx, mov_op,
                    IrOperand(&ir_instr->target, AF_Write),
                    R_(temp));
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
    default: INVALID_CODE_PATH;
    case IR_Add:
        {
            if (ir_instr->target != ir_instr->oper1)
                PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
            if (is_float)
            {
                Amd64_Opcode add_op = (ltype->tag == TYP_f32) ? OP_addss : OP_addsd;
                PushInstruction(ctx, add_op,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper2, AF_Read));
            }
            else
            {
                PushInstruction(ctx, OP_add,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper2, AF_Read));
            }
        } break;
    case IR_Sub:
        {
            if (ir_instr->target != ir_instr->oper1)
                PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
            if (is_float)
            {
                Amd64_Opcode sub_op = (ltype->tag == TYP_f32) ? OP_subss : OP_subsd;
                PushInstruction(ctx, sub_op,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper2, AF_Read));
            }
            else
            {
                PushInstruction(ctx, OP_sub,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper2, AF_Read));
            }
        } break;
    case IR_Mul:
        {
            if (is_float)
            {
                if (ir_instr->target != ir_instr->oper1)
                    PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
                Amd64_Opcode mul_op = (ltype->tag == TYP_f32) ? OP_mulss : OP_mulsd;
                PushInstruction(ctx, mul_op,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper2, AF_Read));
            }
            else if (is_signed)
            {
                if (ir_instr->target != ir_instr->oper1)
                    PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
                Operand temp = TempOperand(ctx, Oper_Data_Type::S64, AF_Write);
                PushLoad(ctx, temp, IrOperand(&ir_instr->oper2, AF_Read));
                PushInstruction(ctx, OP_imul,
                        IrOperand(&ir_instr->target, AF_Write),
                        R_(temp));
            }
            else // unsigned
            {
                Operand rax = FixedRegOperand(ctx, REG_rax, AF_Read);
                Operand rdx = FixedRegOperand(ctx, REG_rdx, AF_Read);
                Operand temp = TempOperand(ctx, Oper_Data_Type::S64, AF_Write);
                PushZeroReg(ctx, rdx);
                PushLoad(ctx, W_(rax), IrOperand(&ir_instr->oper1, AF_Read));
                PushLoad(ctx, temp, IrOperand(&ir_instr->oper2, AF_Read));
                PushInstruction(ctx, OP_mul, R_(temp)); // writes to rdx
                PushLoad(ctx, IrOperand(&ir_instr->target, AF_Write), R_(rax));
            }
        } break;
    case IR_Div:
        {
            if (is_float)
            {
                if (ir_instr->target != ir_instr->oper1)
                    PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
                Amd64_Opcode div_op = (ltype->tag == TYP_f32) ? OP_divss : OP_divsd;
                PushInstruction(ctx, div_op,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper2, AF_Read));
            }
            else if (is_signed)
            {
                Operand rax = FixedRegOperand(ctx, REG_rax, AF_Read);
                Operand rdx = FixedRegOperand(ctx, REG_rdx, AF_Read);
                Operand temp = TempOperand(ctx, Oper_Data_Type::S64, AF_Write);
                PushZeroReg(ctx, rdx);
                PushLoad(ctx, W_(rax), IrOperand(&ir_instr->oper1, AF_Read));
                PushLoad(ctx, temp, IrOperand(&ir_instr->oper2, AF_Read));
                PushInstruction(ctx, OP_idiv, R_(temp)); // writes to rdx
                PushLoad(ctx, IrOperand(&ir_instr->target, AF_Write), R_(rax));
            }
            else // unsigned
            {
                Operand rax = FixedRegOperand(ctx, REG_rax, AF_Read);
                Operand rdx = FixedRegOperand(ctx, REG_rdx, AF_Read);
                Operand temp = TempOperand(ctx, Oper_Data_Type::S64, AF_Write);
                PushZeroReg(ctx, rdx);
                PushLoad(ctx, W_(rax), IrOperand(&ir_instr->oper1, AF_Read));
                PushLoad(ctx, temp, IrOperand(&ir_instr->oper2, AF_Read));
                PushInstruction(ctx, OP_div, R_(temp)); // writes to rdx
                PushLoad(ctx, IrOperand(&ir_instr->target, AF_Write), R_(rax));
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
                Operand rax = FixedRegOperand(ctx, REG_rax, AF_Read);
                Operand rdx = FixedRegOperand(ctx, REG_rdx, AF_Read);
                Operand temp = TempOperand(ctx, Oper_Data_Type::S64, AF_Write);
                PushZeroReg(ctx, rdx);
                PushLoad(ctx, W_(rax), IrOperand(&ir_instr->oper1, AF_Read));
                PushLoad(ctx, temp, IrOperand(&ir_instr->oper2, AF_Read));
                PushInstruction(ctx, OP_idiv, R_(temp)); // writes to rdx
                PushLoad(ctx, IrOperand(&ir_instr->target, AF_Write), R_(rdx));
            }
            else // unsigned
            {
                Operand rax = FixedRegOperand(ctx, REG_rax, AF_Read);
                Operand rdx = FixedRegOperand(ctx, REG_rdx, AF_Read);
                Operand temp = TempOperand(ctx, Oper_Data_Type::S64, AF_Write);
                PushZeroReg(ctx, rdx);
                PushLoad(ctx, W_(rax), IrOperand(&ir_instr->oper1, AF_Read));
                PushLoad(ctx, temp, IrOperand(&ir_instr->oper2, AF_Read));
                PushInstruction(ctx, OP_div, R_(temp)); // writes to rdx
                PushLoad(ctx, IrOperand(&ir_instr->target, AF_Write), R_(rdx));
            }
        } break;

    case IR_And:
        PushInstruction(ctx, OP_and,
                IrOperand(&ir_instr->oper1, AF_Write),
                IrOperand(&ir_instr->oper2, AF_Read));
        break;
    case IR_Or:
        PushInstruction(ctx, OP_or,
                IrOperand(&ir_instr->oper1, AF_Write),
                IrOperand(&ir_instr->oper2, AF_Read));
        break;
    case IR_Xor:
        PushInstruction(ctx, OP_xor,
                IrOperand(&ir_instr->oper1, AF_Write),
                IrOperand(&ir_instr->oper2, AF_Read));
        break;

    case IR_Neg:
        if (is_float)
        {
            if (ltype->tag == TYP_f32)
            {
                PushLoad(ctx, IrOperand(&ir_instr->target, AF_Write), ImmOperand(0.0f, AF_Read));
                PushInstruction(ctx, OP_subss,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper1, AF_Read));
            }
            else if (ltype->tag == TYP_f64)
            {
                PushLoad(ctx, IrOperand(&ir_instr->target, AF_Write), ImmOperand(0.0, AF_Read));
                PushInstruction(ctx, OP_subsd,
                        IrOperand(&ir_instr->target, AF_Write),
                        IrOperand(&ir_instr->oper1, AF_Read));
            }
        }
        else
        {
            PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
            PushInstruction(ctx, OP_neg, IrOperand(&ir_instr->target, AF_ReadWrite));
        }
        break;
    case IR_Not:
    case IR_Compl:
        PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
        PushInstruction(ctx, OP_not, IrOperand(&ir_instr->target, AF_ReadWrite));
        break;
    }
}

static s64 GetLocalOffset(Codegen_Context *ctx, Name name)
{
    Routine *routine = ctx->current_routine;
    Local_Offset *offs = hashtable::Lookup(routine->local_offsets, name);
    if (offs) return offs->offset;

    offs = PushStruct<Local_Offset>(&ctx->arena);

    //routine->locals_size += GetAlignedSize(ir_oper->type);
    routine->locals_size += 8;
    routine->locals_size = Align(routine->locals_size, 8);
    offs->name = name;
    offs->offset = -routine->locals_size;

    hashtable::Put(routine->local_offsets, name, offs);
    return offs->offset;
}

#if 1
static b32 GetLocalOffset(Codegen_Context *ctx, Name name, s64 *offset)
{
    Routine *routine = ctx->current_routine;
    Local_Offset *offs = hashtable::Lookup(routine->local_offsets, name);
    if (offs)
    {
        *offset = offs->offset;
        return true;
    }
    return false;
}
#endif

static Operand GetAddress(Codegen_Context *ctx, Ir_Operand *ir_oper)
{
    // TODO(henrik): Differentiate between IR immediate and IR string constant.
    //ASSERT(ir_oper->oper_type != IR_OPER_Immediate);
    if (ir_oper->oper_type == IR_OPER_Immediate)
    {
        if (TypeIsString(ir_oper->type))
            return IrOperand(ir_oper, AF_ReadWrite);
        INVALID_CODE_PATH;
    }
    Name name;
    if (ir_oper->oper_type == IR_OPER_Variable)
        name = ir_oper->var.name;
    else if (ir_oper->oper_type == IR_OPER_Temp)
        name = ir_oper->var.name;
    else
        INVALID_CODE_PATH;
    s64 local_offs = GetLocalOffset(ctx, name);
    return BaseOffsetOperand(REG_rbp, local_offs, AF_ReadWrite);
}

static s64 PushArgs(Codegen_Context *ctx,
        Ir_Routine *ir_routine, Ir_Instruction *ir_instr)
{
    s64 arg_count = ctx->current_arg_count;
    s64 arg_index = arg_count - 1;
    s64 arg_reg_idx = arg_index;
    s64 float_arg_reg_idx = arg_index;

    s64 arg_stack_alloc = (arg_count > 4 ? arg_count : 4) * 8;
    PushInstruction(ctx, OP_sub,
            RegOperand(REG_rsp, Oper_Data_Type::U64, AF_Write),
            ImmOperand(arg_stack_alloc, AF_Read));

    ASSERT(ir_instr->oper2.oper_type == IR_OPER_Immediate);
    s64 arg_instr_idx = ir_instr->oper2.imm_s64;
    while (arg_instr_idx != -1)
    {
        Ir_Instruction *arg_instr = &ir_routine->instructions[arg_instr_idx];
        ASSERT(arg_instr->opcode == IR_Arg);

        ctx->comment = &arg_instr->comment;

        Type *arg_type = arg_instr->target.type;
        if (TypeIsStruct(arg_type))
        {
            //PushStructArg();

            const Reg *arg_reg = nullptr;
            arg_reg = GetArgRegister(ctx->reg_alloc, arg_reg_idx);
            float_arg_reg_idx--;
            arg_reg_idx--;
            if (arg_reg)
            {
                if (arg_type->size > 8)
                {
                    PushLoad(ctx,
                            FixedRegOperand(ctx, *arg_reg, Oper_Data_Type::PTR, AF_Write),
                            R_(GetAddress(ctx, &arg_instr->target)));
                }
                else
                {
                    PushLoad(ctx,
                            FixedRegOperand(ctx, *arg_reg, Oper_Data_Type::U64, AF_Write),
                            IrOperand(&arg_instr->target, AF_Read));
                }
            }
            else
            {
                if (arg_type->size > 8)
                {
                    PushLoadAddr(ctx,
                            BaseOffsetOperand(REG_rsp, arg_index * 8, AF_Write),
                            R_(GetAddress(ctx, &arg_instr->target)));
                }
                else
                {
                    Operand temp = TempOperand(ctx, Oper_Data_Type::PTR, AF_Write);
                    PushLoad(ctx,
                            temp,
                            BaseOffsetOperand(&arg_instr->target, 0, AF_Read));
                    PushLoad(ctx,
                            BaseOffsetOperand(REG_rsp, arg_index * 8, AF_Write),
                            R_(temp));
                }
            }
        }
        else
        {
            const Reg *arg_reg = nullptr;
            Oper_Data_Type data_type = DataTypeFromType(arg_type);
            if (TypeIsFloat(arg_type))
            {
                arg_reg = GetFloatArgRegister(ctx->reg_alloc, float_arg_reg_idx);
                float_arg_reg_idx--;
                arg_reg_idx--;
            }
            else
            {
                arg_reg = GetArgRegister(ctx->reg_alloc, arg_reg_idx);
                float_arg_reg_idx--;
                arg_reg_idx--;
            }
            if (arg_reg)
            {
                PushLoad(ctx,
                        FixedRegOperand(ctx, *arg_reg, data_type, AF_Write),
                        IrOperand(&arg_instr->target, AF_Read));
            }
        }

        arg_index--;

        ASSERT(arg_instr->oper1.oper_type == IR_OPER_Immediate);
        arg_instr_idx = arg_instr->oper1.imm_s64;
    }

    return arg_stack_alloc;
}

static void GenerateCode(Codegen_Context *ctx,
        Ir_Routine *routine, Ir_Instruction *ir_instr)
{
    switch (ir_instr->opcode)
    {
        case IR_COUNT:
            INVALID_CODE_PATH;
            break;

        case IR_Label:
            PushLabel(ctx, ir_instr->target.label->name);
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
            {
                PushLoad(ctx,
                        IrOperand(&ir_instr->target, AF_Write),
                        BaseOffsetOperand(&ir_instr->oper1, 0, AF_Read));
            } break;

        case IR_Addr:
            {
                Operand addr_oper = GetAddress(ctx, &ir_instr->oper1);
                PushLoadAddr(ctx,
                        IrOperand(&ir_instr->target, AF_Write),
                        R_(addr_oper));
            } break;

        case IR_Mov:
            PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
            break;
        case IR_MovMember:
            {
                Type *oper_type = ir_instr->oper1.type;
                s64 member_index = ir_instr->oper2.imm_s64;
                if (TypeIsPointer(oper_type))
                {
                    s64 member_offset = GetStructMemberOffset(oper_type->base_type, member_index);
                    PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
                    if (member_offset != 0)
                    {
                        PushInstruction(ctx, OP_add,
                                IrOperand(&ir_instr->target, AF_Write),
                                ImmOperand((s64)member_offset, AF_Read));
                    }
                }
                else
                {
                    s64 member_offset = GetStructMemberOffset(oper_type, member_index);
                    PushLoad(ctx,
                            IrOperand(&ir_instr->target, AF_Write),
                            BaseOffsetOperand(&ir_instr->oper1, member_offset, AF_Read));
                }
            } break;
        case IR_MovElement:
            {
                // target <- base + index*size
                s64 size = GetAlignedElementSize(ir_instr->oper1.type);
                PushLoadAddr(ctx,
                        IrOperand(&ir_instr->target, AF_Write),
                        BaseIndexOffsetOperand(&ir_instr->oper1, 0, AF_Read),
                        IndexScaleOperand(&ir_instr->oper2, size, AF_Read));
            } break;

        case IR_Arg:
            ctx->current_arg_count++;
            break;

        case IR_Call:
            {
                s64 arg_stack_alloc = PushArgs(ctx, routine, ir_instr);
                PushInstruction(ctx, OP_call, IrOperand(&ir_instr->oper1, AF_Read));
                PushInstruction(ctx, OP_add,
                        RegOperand(REG_rsp, Oper_Data_Type::U64, AF_Write),
                        ImmOperand(arg_stack_alloc, AF_Read));
                if (ir_instr->target.oper_type != IR_OPER_None)
                {
                    Oper_Data_Type data_type = DataTypeFromType(ir_instr->target.type);
                    if (TypeIsFloat(ir_instr->target.type))
                    {
                        PushLoad(ctx,
                            IrOperand(&ir_instr->target, AF_Write),
                            RegOperand(REG_xmm0, data_type, AF_Read));
                    }
                    else
                    {
                        PushLoad(ctx,
                            IrOperand(&ir_instr->target, AF_Write),
                            RegOperand(REG_rax, data_type, AF_Read));
                    }
                }
                ctx->current_arg_count = 0;
            } break;
        case IR_CallForeign:
            {
                s64 arg_stack_alloc = PushArgs(ctx, routine, ir_instr);
                PushInstruction(ctx, OP_call, LabelOperand(&ir_instr->oper1, AF_Read));
                PushInstruction(ctx, OP_add,
                        RegOperand(REG_rsp, Oper_Data_Type::U64, AF_Write),
                        ImmOperand(arg_stack_alloc, AF_Read));
                if (ir_instr->target.oper_type != IR_OPER_None)
                {
                    Oper_Data_Type data_type = DataTypeFromType(ir_instr->target.type);
                    if (TypeIsFloat(ir_instr->target.type))
                    {
                        PushLoad(ctx,
                            IrOperand(&ir_instr->target, AF_Write),
                            RegOperand(REG_xmm0, data_type, AF_Read));
                    }
                    else
                    {
                        PushLoad(ctx,
                            IrOperand(&ir_instr->target, AF_Write),
                            RegOperand(REG_rax, data_type, AF_Read));
                    }
                }
                ctx->current_arg_count = 0;
            } break;

        case IR_Jump:
            PushInstruction(ctx, OP_jmp, LabelOperand(&ir_instr->target, AF_Read));
            break;
        case IR_Jz:
            PushInstruction(ctx, OP_cmp,
                    IrOperand(&ir_instr->oper1, AF_Read),
                    ImmOperand((s64)0, AF_Read));
            PushInstruction(ctx, OP_je, LabelOperand(&ir_instr->target, AF_Read));
            break;
        case IR_Jnz:
            PushInstruction(ctx, OP_cmp,
                    IrOperand(&ir_instr->oper1, AF_Read),
                    ImmOperand((s64)0, AF_Read));
            PushInstruction(ctx, OP_jne, LabelOperand(&ir_instr->target, AF_Read));
            break;
        case IR_Return:
            if (ir_instr->target.oper_type != IR_OPER_None)
            {
                Oper_Data_Type data_type = DataTypeFromType(ir_instr->target.type);
                if (TypeIsFloat(ir_instr->target.type))
                {
                    PushLoad(ctx,
                        RegOperand(REG_xmm0, data_type, AF_Write),
                        IrOperand(&ir_instr->target, AF_Read));
                }
                else
                {
                    PushLoad(ctx,
                        RegOperand(REG_rax, data_type, AF_Write),
                        IrOperand(&ir_instr->target, AF_Read));
                }
            }
            PushInstruction(ctx, OP_jmp,
                    LabelOperand(ctx->current_routine->return_label.name, AF_Read));
            break;

        case IR_S_TO_F32:
            PushInstruction(ctx, OP_cvtsi2ss,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrOperand(&ir_instr->oper1, AF_Read));
            break;
        case IR_S_TO_F64:
            PushInstruction(ctx, OP_cvtsi2sd,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrOperand(&ir_instr->oper1, AF_Read));
            break;
        case IR_F32_TO_S:
            PushInstruction(ctx, OP_cvtss2si,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrOperand(&ir_instr->oper1, AF_Read));
            break;
        case IR_F64_TO_S:
            PushInstruction(ctx, OP_cvtsd2si,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrOperand(&ir_instr->oper1, AF_Read));
            break;
        case IR_F32_TO_F64:
            PushInstruction(ctx, OP_cvtss2sd,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrOperand(&ir_instr->oper1, AF_Read));
            break;
        case IR_F64_TO_F32:
            PushInstruction(ctx, OP_cvtsd2ss,
                    IrOperand(&ir_instr->target, AF_Write),
                    IrOperand(&ir_instr->oper1, AF_Read));
            break;
    }
}


//static s64 GetLocalOffset(Codegen_Context *ctx, Ir_Operand *ir_oper)
//{
//    ASSERT(ir_oper->oper_type == IR_OPER_Variable ||
//           ir_oper->oper_type == IR_OPER_Temp);
//    Routine *routine = ctx->current_routine;
//    Name name;
//    if (ir_oper->oper_type == IR_OPER_Variable)
//        name = ir_oper->var.name;
//    else
//        name = ir_oper->temp.name;
//    Local_Offset *offs = hashtable::Lookup(routine->local_offsets, name);
//    if (offs) return offs->offset;
//
//    offs = PushStruct<Local_Offset>(&ctx->arena);
//
//    //routine->locals_size += GetAlignedSize(ir_oper->type);
//    routine->locals_size += GetSize(ir_oper->type);
//    routine->locals_size = Align(routine->locals_size, GetAlign(ir_oper->type));
//    offs->name = name;
//    offs->offset = routine->locals_size;
//
//    hashtable::Put(routine->local_offsets, name, offs);
//    return offs->offset;
//}

/*
static void SaveDirtyRegister(Codegen_Context *ctx, s64 instr_index, Name name, Reg reg, Oper_Data_Type data_type)
{
    if (UndirtyRegister(ctx->reg_alloc, reg))
    {
        s64 local_offs = GetLocalOffset(ctx, name);
        InsertLoad(ctx, ctx->current_routine->instructions, instr_index,
                BaseOffsetOperand(REG_rbp, local_offs, AF_Write),
                RegOperand(reg, data_type, AF_Read));
        if (UndirtyCalleeSave(ctx->reg_alloc, reg))
        {
            PushLoad(ctx, ctx->current_routine->epilogue,
                    RegOperand(reg, data_type, AF_Write),
                    BaseOffsetOperand(REG_rbp, local_offs, AF_Read));
        }
    }
}
*/

#if 0
static void SaveDirtyRegister(Codegen_Context *ctx, s64 &instr_index, Reg reg)
{
    if (UndirtyRegister(ctx->reg_alloc, reg))
    {
        const Reg_Var *old_var = GetMappedVar(ctx->reg_alloc, reg);
        Name name;
        Oper_Data_Type data_type;
        if (!old_var)
        {
            if (!IsCalleeSave(ctx->reg_alloc, reg))
                return;
            name = GetRegName(reg);
            if (IsFloatRegister(ctx->reg_alloc, reg))
                data_type = Oper_Data_Type::F64;
            else
                data_type = Oper_Data_Type::U64;
        }
        else
        {
            name = old_var->var_name;
            data_type = old_var->data_type;
            UnmapRegister(ctx->reg_alloc, reg);
        }

        s64 local_offs = GetLocalOffset(ctx, name);
        if (UndirtyCalleeSave(ctx->reg_alloc, reg))
        {
            PushLoad(ctx, ctx->current_routine->callee_save_spills,
                    BaseOffsetOperand(REG_rbp, local_offs, AF_Write),
                    RegOperand(reg, data_type, AF_Read));
            PushLoad(ctx, ctx->current_routine->epilogue,
                    RegOperand(reg, data_type, AF_Write),
                    BaseOffsetOperand(REG_rbp, local_offs, AF_Read));
        }
        else
        {
            InsertLoad(ctx, ctx->current_routine->instructions, instr_index,
                    BaseOffsetOperand(REG_rbp, local_offs, AF_Write),
                    RegOperand(reg, data_type, AF_Read));
        }
    }
}

static void AllocateRegister(Codegen_Context *ctx, s64 &instr_index, Operand *oper, Name name, const Reg *fixed_reg)
{
    Reg_Alloc *reg_alloc = ctx->reg_alloc;
    const Reg *mapped_reg = GetMappedRegister(reg_alloc, name);
    Reg reg;
    if (!mapped_reg)
    {
        reg = (fixed_reg) ? *fixed_reg : GetFreeRegister(reg_alloc, oper->data_type);
        SaveDirtyRegister(ctx, instr_index, reg);
        MapRegister(reg_alloc, name, reg, oper->data_type);
        mapped_reg = &reg;
    }
    Operand reg_oper = RegOperand(*mapped_reg, oper->data_type, oper->access_flags);
    reg_oper.addr_mode = oper->addr_mode;
    reg_oper.scale_offset = oper->scale_offset;
    *oper = reg_oper;

    s64 local_offs;
    if (GetLocalOffset(ctx, name, &local_offs))
    {
        InsertLoad(ctx, ctx->current_routine->instructions, instr_index,
                reg_oper,
                BaseOffsetOperand(REG_rbp, local_offs, AF_Read));
    }
}

static void AllocateRegister(Codegen_Context *ctx, s64 &instr_index, Operand *oper)
{
    switch (oper->type)
    {
        case Oper_Type::None: break;
        case Oper_Type::Label: break;
        case Oper_Type::Register:
            break;
        case Oper_Type::Immediate:
            break;
        case Oper_Type::Temp:
            {
                AllocateRegister(ctx, instr_index, oper, oper->temp.name, nullptr);
            } break;
        case Oper_Type::FixedRegister:
            {
                AllocateRegister(ctx, instr_index, oper, oper->fixed_reg.name, &oper->fixed_reg.reg);
            } break;
        case Oper_Type::IrOperand:
            {
                Ir_Operand *ir_oper = oper->ir_oper;
                switch (ir_oper->oper_type)
                {
                    case IR_OPER_None: INVALID_CODE_PATH; break;
                    case IR_OPER_Variable:
                        {
                            AllocateRegister(ctx, instr_index, oper, ir_oper->var.name, nullptr);
                        } break;
                    case IR_OPER_Temp:
                        {
                            AllocateRegister(ctx, instr_index, oper, ir_oper->temp.name, nullptr);
                        } break;
                    case IR_OPER_Immediate:
                    case IR_OPER_Label:
                    case IR_OPER_Routine:
                    case IR_OPER_ForeignRoutine:
                        break;
                }
            } break;
    }
}

/*
static void SaveDirtyOperand(Codegen_Context *ctx, s64 instr_index, Operand *oper)
{
    if ((oper->access_flags & AF_Write) == 0)
        return;

    Reg_Alloc *reg_alloc = ctx->reg_alloc;
    switch (oper->type)
    {
        case Oper_Type::None: break;
        case Oper_Type::Label: break;
        case Oper_Type::AddrLabel: break;
        case Oper_Type::Register:
            break;
        case Oper_Type::Temp:
            {
                //const Reg *mapped_reg = GetMappedRegister(reg_alloc, oper->temp.name);
                //if (!mapped_reg) return;
                //SaveDirtyRegister(ctx, instr_index, oper->temp.name, *mapped_reg);
            } break;
        case Oper_Type::Immediate:
            break;
        case Oper_Type::FixedRegister:
            {
                //const Reg *mapped_reg = GetMappedRegister(reg_alloc, oper->fixed_reg.name);
                //if (!mapped_reg) return;
                //SaveDirtyRegister(ctx, instr_index, oper->fixed_reg.name, *mapped_reg);
                //
                //const Reg_Var *mapped_var = GetMappedVar(reg_alloc, oper->fixed_reg.reg);
                //if (!mapped_var) return;
                //SaveDirtyRegister(ctx, instr_index, mapped_var->var_name, oper->fixed_reg.reg, mapped_var->data_type);
                //
                //SaveDirtyRegister(ctx, instr_index, oper->fixed_reg.reg);
            } break;
        case Oper_Type::IrOperand:
            {
                Ir_Operand *ir_oper = oper->ir_oper;
                switch (ir_oper->oper_type)
                {
                    case IR_OPER_None: INVALID_CODE_PATH; break;
                    case IR_OPER_Variable:
                        {
                            //const Reg *mapped_reg = GetMappedRegister(reg_alloc, ir_oper->var.name);
                            //if (!mapped_reg) return;
                            //SaveDirtyRegister(ctx, instr_index, ir_oper->var.name, *mapped_reg, oper->data_type);
                        } break;
                    case IR_OPER_Temp:
                        {
                            //const Reg *mapped_reg = GetMappedRegister(reg_alloc, ir_oper->temp.name);
                            //if (!mapped_reg) return;
                            //SaveDirtyRegister(ctx, instr_index, ir_oper->temp.name, *mapped_reg, oper->data_type);
                        } break;
                    case IR_OPER_Immediate:
                    case IR_OPER_Label:
                    case IR_OPER_Routine:
                    case IR_OPER_ForeignRoutine:
                        break;
                }
            } break;
    }
}
*/

static void DirtyReg(Codegen_Context *ctx, Operand oper)
{
    if ((oper.access_flags & AF_Write) == 0)
        return;
    if (oper.addr_mode == Oper_Addr_Mode::Direct &&
        oper.type == Oper_Type::Register)
    {
        DirtyRegister(ctx->reg_alloc, oper.reg);
    }
}

static void ClearRegs(Codegen_Context *ctx, s64 &instr_index)
{
    Reg_Alloc *reg_alloc = ctx->reg_alloc;
    for (s64 i = 0; i < reg_alloc->general_reg_count; i++)
    {
        Reg reg = reg_alloc->general_regs[i];
        const Reg_Var *reg_var = GetMappedVar(reg_alloc, reg);
        if (!reg_var) continue;
        if (IsCalleeSave(reg_alloc, reg)) continue;
        SaveDirtyRegister(ctx, instr_index, reg);
    }
    for (s64 i = 0; i < reg_alloc->float_reg_count; i++)
    {
        Reg reg = reg_alloc->float_regs[i];
        const Reg_Var *reg_var = GetMappedVar(reg_alloc, reg);
        if (!reg_var) continue;
        if (IsCalleeSave(reg_alloc, reg)) continue;
        SaveDirtyRegister(ctx, instr_index, reg);
    }
    ClearRegAllocs(reg_alloc);
}

static b32 IsTerminator(Instruction *instr)
{
    switch ((Amd64_Opcode)instr->opcode)
    {
    case OP_jmp:
    case OP_je:
    case OP_jne:
        return true;
    default:
        break;
    }
    return false;
}

static void AllocateRegisters(Codegen_Context *ctx, Ir_Routine *ir_routine)
{
    Reg_Alloc *reg_alloc = ctx->reg_alloc;
    ClearRegAllocs(reg_alloc);
    DirtyCalleeSaveRegs(reg_alloc);
    for (s64 i = 0; i < ir_routine->arg_count; i++)
    {
        Ir_Operand *arg = &ir_routine->args[i];
        const Reg *arg_reg = TypeIsFloat(arg->type)
            ? GetFloatArgRegister(reg_alloc, i)
            : GetArgRegister(reg_alloc, i);
        if (arg_reg)
        {
            Oper_Data_Type data_type = DataTypeFromType(arg->type);
            MapRegister(reg_alloc, arg->var.name, *arg_reg, data_type);
            DirtyRegister(reg_alloc, *arg_reg);
        }
    }
    Routine *routine = ctx->current_routine;
    for (s64 i = 0; i < routine->instructions.count; i++)
    {
        Instruction *instr = routine->instructions[i];
        if ((Amd64_Opcode)instr->opcode == OP_LABEL)
        {
            ClearRegs(ctx, i);
            continue;
        }
        //SaveDirtyOperand(ctx, i, &instr->oper1);
        //SaveDirtyOperand(ctx, i, &instr->oper2);
        //SaveDirtyOperand(ctx, i, &instr->oper3);
        AllocateRegister(ctx, i, &instr->oper1);
        AllocateRegister(ctx, i, &instr->oper2);
        AllocateRegister(ctx, i, &instr->oper3);

        DirtyReg(ctx, instr->oper1);
        DirtyReg(ctx, instr->oper2);
        DirtyReg(ctx, instr->oper3);

        if (IsTerminator(instr))
            ClearRegs(ctx, i);
    }
}
#endif


static void CollectLabelInstructions(Codegen_Context *ctx, Routine *routine)
{
    for (s64 i = 0; i < routine->instructions.count; i++)
    {
        Instruction *instr = routine->instructions[i];
        if ((Amd64_Opcode)instr->opcode == OP_LABEL)
        {
            Instruction *next_instr = nullptr;
            s64 next_i = -1;
            for (s64 n = i + 1; n < routine->instructions.count; n++)
            {
                Instruction *next = routine->instructions[n];
                if ((Amd64_Opcode)next->opcode != OP_LABEL)
                {
                    next_instr = next;
                    next_i = n;
                    break;
                }
            }

            Label label = instr->oper1.label;
            Label_Instr *label_instr = PushStruct<Label_Instr>(&ctx->arena);
            label_instr->name = label.name;
            label_instr->instr = next_instr;
            label_instr->instr_index = next_i;
            hashtable::Put(routine->labels, label.name, label_instr);
        }
    }
}

#if 1
struct Name_Data_Type
{
    Name name;
    Reg fixed_reg;
    Oper_Data_Type data_type;
};
struct Live_Sets
{
    Array<Name_Data_Type> live_in;
    Array<Name_Data_Type> live_out;
};

b32 SetAdd(Array<Name_Data_Type> &set, Name name, Oper_Data_Type data_type, Reg fixed_reg = { })
{
    for (s64 i = 0; i < set.count; i++)
    {
        if (set[i].name == name) return false;
    }
    Name_Data_Type nd = { };
    nd.name = name;
    nd.fixed_reg = fixed_reg;
    nd.data_type = data_type;
    array::Push(set, nd);
    return true;
}

b32 SetUnion(Array<Name_Data_Type> &a, const Array<Name_Data_Type> &b)
{
    b32 any_new = false;
    for (s64 i = 0; i < b.count; i++)
    {
        Name_Data_Type nd = b[i];
        any_new |= SetAdd(a, nd.name, nd.data_type, nd.fixed_reg);
    }
    return any_new;
}

b32 SetUnion(Array<Name_Data_Type> &a, const Array<Name_Data_Type> &b, Name *exclude_names, s64 exclude_count)
{
    b32 any_new = false;
    for (s64 i = 0; i < b.count; i++)
    {
        Name name = b[i].name;
        b32 excluded = false;
        for (s64 e = 0; e < exclude_count; e++)
        {
            if (name == exclude_names[e])
            {
                excluded = true;
                break;
            }
        }
        if (!excluded)
        {
            Name_Data_Type nd = b[i];
            any_new |= SetAdd(a, nd.name, nd.data_type, nd.fixed_reg);
        }
    }
    return any_new;
}

static Name GetOperName(Operand oper)
{
    Name name = { };
    switch (oper.type)
    {
        default: break;
        case Oper_Type::FixedRegister:  name = oper.fixed_reg.name; break;
        case Oper_Type::Temp:           name = oper.temp.name; break;
        case Oper_Type::IrOperand:
            {
                if (oper.ir_oper->oper_type == IR_OPER_Variable)
                    name = oper.ir_oper->var.name;
                else if (oper.ir_oper->oper_type == IR_OPER_Temp)
                    name = oper.ir_oper->temp.name;
            }
            break;
    }
    return name;
}

static Name GetOperName(Operand oper, Reg *fixed_reg)
{
    Name name = { };
    switch (oper.type)
    {
        default: break;
        case Oper_Type::FixedRegister:
            name = oper.fixed_reg.name;
            *fixed_reg = oper.fixed_reg.reg;
            break;
        case Oper_Type::Temp:
            name = oper.temp.name;
            break;
        case Oper_Type::IrOperand:
            {
                if (oper.ir_oper->oper_type == IR_OPER_Variable)
                    name = oper.ir_oper->var.name;
                else if (oper.ir_oper->oper_type == IR_OPER_Temp)
                    name = oper.ir_oper->temp.name;
            } break;
    }
    return name;
}

static b32 AddOper(Array<Name_Data_Type> &set, Operand oper, Oper_Access_Flags access_flags)
{
    if ((oper.access_flags & access_flags) != 0)
    {
        Reg fixed_reg = { };
        Name name = GetOperName(oper, &fixed_reg);
        if (name.str.size != 0)
            return SetAdd(set, name, oper.data_type, fixed_reg);
    }
    return false;
}

struct Live_Interval
{
    s32 start;
    s32 end;
    Name name;
    Reg reg;
    Oper_Data_Type data_type;
    b32 is_fixed;
};

static void PrintInstruction(IoFile *file, const Instruction *instr);

void ComputeLiveness(Codegen_Context *ctx, Ir_Routine *ir_routine,
        Routine *routine, Array<Live_Interval*> &live_intervals)
{
    Instruction_List &instructions = routine->instructions;

    Array<Live_Sets> live_sets = { };
    array::Resize(live_sets, instructions.count);

    Live_Sets &entry = live_sets[0];
    for (s64 i = 0; i < ir_routine->arg_count; i++)
    {
        Ir_Operand *arg = &ir_routine->args[i];
        Oper_Data_Type data_type = DataTypeFromType(arg->type);
        const Reg *arg_reg = GetArgRegister(ctx->reg_alloc, data_type, i);
        if (arg_reg)
        {
            SetAdd(entry.live_out, arg->var.name, data_type, *arg_reg);
        }
    }

    bool changed = true;
    while (changed)
    {
        changed = false;
        //for (s64 i = 0; i < instructions.count; i++)
        for (s64 i = instructions.count - 1; i >= 0; i--)
        {
            Instruction *instr = instructions[i];
            Instruction *next_instr = nullptr;
            s64 next_i = -1;
            for (s64 n = i + 1; n < instructions.count; n++)
            {
                Instruction *next = instructions[n];
                if ((Amd64_Opcode)next->opcode != OP_LABEL)
                {
                    next_instr = next;
                    next_i = n;
                    break;
                }
            }

            changed |= AddOper(live_sets[i].live_in, instr->oper1, AF_Read);
            changed |= AddOper(live_sets[i].live_in, instr->oper2, AF_Read);
            changed |= AddOper(live_sets[i].live_in, instr->oper3, AF_Read);

            Name writes[3] = { };
            if ((instr->oper1.access_flags & AF_Write) != 0)
                writes[0] = GetOperName(instr->oper1);
            if ((instr->oper2.access_flags & AF_Write) != 0)
                writes[1] = GetOperName(instr->oper2);
            if ((instr->oper3.access_flags & AF_Write) != 0)
                writes[2] = GetOperName(instr->oper3);

            changed |= SetUnion(live_sets[i].live_in, live_sets[i].live_out, writes, 3);

#if 1
            changed |= AddOper(live_sets[i].live_out, instr->oper1, AF_Write);
            changed |= AddOper(live_sets[i].live_out, instr->oper2, AF_Write);
            changed |= AddOper(live_sets[i].live_out, instr->oper3, AF_Write);
#endif

            if (((instr->flags & IF_FallsThrough) != 0) && next_instr)
            {
                changed |= SetUnion(live_sets[i].live_out, live_sets[next_i].live_in);
            }
            if ((instr->flags & IF_Branch) != 0)
            {
                ASSERT(instr->oper1.type == Oper_Type::Label);
                Name label_name = instr->oper1.label.name;
                const Label_Instr *li = hashtable::Lookup(routine->labels, label_name);
                ASSERT(li != nullptr);
                if (li->instr)
                {
                    changed |= SetUnion(live_sets[i].live_out, live_sets[li->instr_index].live_in);
                }
            }
        }
    }

    // Reduce liveness information to coarse live intervals
    for (s64 instr_i = 0; instr_i < live_sets.count; instr_i++)
    {
        Live_Sets sets = live_sets[instr_i];
        for (s64 i = 0; i < sets.live_out.count; i++)
        {
            Name_Data_Type name_dt = sets.live_out[i];
            Live_Interval *prev_interval = hashtable::Lookup(live_intervals, name_dt.name);
            if (prev_interval)
            {
                prev_interval->start = (instr_i < prev_interval->start)
                    ? instr_i : prev_interval->start;
                prev_interval->end = (instr_i > prev_interval->end)
                    ? instr_i : prev_interval->end;
                continue;
            }
            Live_Interval *interval = PushStruct<Live_Interval>(&ctx->arena);
            interval->start = instr_i;
            interval->end = instr_i;
            interval->name = name_dt.name;
            interval->reg = name_dt.fixed_reg;
            interval->data_type = name_dt.data_type;
            interval->is_fixed = (name_dt.fixed_reg.reg_index != REG_NONE);
            hashtable::Put(live_intervals, name_dt.name, interval);
        }
        for (s64 i = 0; i < sets.live_in.count; i++)
        {
            Name_Data_Type name_dt = sets.live_in[i];
            Live_Interval *prev_interval = hashtable::Lookup(live_intervals, name_dt.name);
            if (prev_interval)
            {
                prev_interval->start = (instr_i < prev_interval->start)
                    ? instr_i : prev_interval->start;
                prev_interval->end = (instr_i > prev_interval->end)
                    ? instr_i : prev_interval->end;
                continue;
            }
            Live_Interval *interval = PushStruct<Live_Interval>(&ctx->arena);
            interval->start = instr_i;
            interval->end = instr_i;
            interval->name = name_dt.name;
            interval->reg = name_dt.fixed_reg;
            interval->data_type = name_dt.data_type;
            interval->is_fixed = (name_dt.fixed_reg.reg_index != REG_NONE);
            hashtable::Put(live_intervals, name_dt.name, interval);
        }
    }

#if 1
    fprintf(stderr, "\n--Live in/out-- ");
    PrintName((IoFile*)stderr, routine->name);
    fprintf(stderr, "\n");
    for (s64 instr_i = 0; instr_i < live_sets.count; instr_i++)
    {
        Live_Sets sets = live_sets[instr_i];
        //fprintf(stderr, "instr %" PRId64 ":\n", instr_i);
        fprintf(stderr, "instr %" PRId64 ": ", instr_i);
        PrintInstruction((IoFile*)stderr, instructions[instr_i]);
        fprintf(stderr, "   in: ");
        for (s64 i = 0; i < sets.live_in.count; i++)
        {
            Name name = sets.live_in[i].name;
            PrintName((IoFile*)stderr, name);
            fprintf(stderr, ", ");
        }
        fprintf(stderr, "\n  out: ");
        for (s64 i = 0; i < sets.live_out.count; i++)
        {
            Name name = sets.live_out[i].name;
            PrintName((IoFile*)stderr, name);
            fprintf(stderr, ", ");
        }
        fprintf(stderr, "\n");
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "--Live in/out end--\n");
#endif

#if 1
    fprintf(stderr, "\n--Live intervals-- ");
    PrintName((IoFile*)stderr, routine->name);
    fprintf(stderr, "\n");
    for (s64 i = 0; i < live_intervals.count; i++)
    {
        Live_Interval *interval = live_intervals[i];
        if (!interval) continue;

        PrintName((IoFile*)stderr, interval->name);
        fprintf(stderr, ": \t[%d,%d]\n",
                interval->start, interval->end);
    }
    fprintf(stderr, "--Live intervals end--\n");
#endif

    for (s64 i = 0; i < live_sets.count; i++)
    {
        array::Free(live_sets[i].live_in);
        array::Free(live_sets[i].live_out);
    }
    array::Free(live_sets);
}
#endif

static void ExpireOldIntervals(Codegen_Context *ctx, Array<Live_Interval> &active, Live_Interval interval)
{
    for (s64 i = 0; i < active.count; i++)
    {
        Live_Interval active_interval = active[i];
        if (active_interval.end >= interval.start)
            return;
        array::Erase(active, i);
        if (IsFloatRegister(ctx->reg_alloc, active_interval.reg))
            array::Push(ctx->reg_alloc->free_float_regs, active_interval.reg);
        else
            array::Push(ctx->reg_alloc->free_regs, active_interval.reg);
    }
}

static void RemoveFromFreeRegs(Array<Reg> &free_regs, Reg reg)
{
    for (s64 i = 0; i < free_regs.count; i++)
    {
        if (free_regs[i] == reg)
        {
            array::EraseBySwap(free_regs, i);
            return;
        }
    }
}

static void RemoveFromFreeRegs(Codegen_Context *ctx, Reg reg)
{
    if (IsFloatRegister(ctx->reg_alloc, reg))
        RemoveFromFreeRegs(ctx->reg_alloc->free_float_regs, reg);
    else
        RemoveFromFreeRegs(ctx->reg_alloc->free_regs, reg);
}

static void AddToActive(Array<Live_Interval> &active, Live_Interval interval)
{
    s64 index = 0;
    for (; index < active.count; index++)
    {
        ASSERT(active[index].reg != interval.reg);
        if (interval.end < active[index].end)
            break;
    }
    array::Insert(active, index, interval);
}

struct Spill_Info
{
    Live_Interval spill;
    s32 instr_index;
    b32 is_spill;
};
Array<Spill_Info> spills = { };

static void InsertSpills(Codegen_Context *ctx)
{
    s64 idx_offset = 0;
    Routine *routine = ctx->current_routine;
    for (s64 i = 0; i < spills.count; i++)
    {
        Spill_Info spill_info = spills[i];
        s64 index = spill_info.instr_index + idx_offset;

        s64 offs = GetLocalOffset(ctx, spill_info.spill.name);
        if (spill_info.is_spill)
        {
            InsertLoad(ctx, routine->instructions, index,
                    BaseOffsetOperand(REG_rbp, offs, AF_Write),
                    RegOperand(spill_info.spill.reg, spill_info.spill.data_type, AF_Read));
        }
        else
        {
            InsertLoad(ctx, routine->instructions, index,
                    RegOperand(spill_info.spill.reg, spill_info.spill.data_type, AF_Write),
                    BaseOffsetOperand(REG_rbp, offs, AF_Read));
        }
        idx_offset++;
    }
}

static void Spill(Live_Interval spill, s64 instr_index)
{
    Spill_Info spill_info = { };
    spill_info.spill = spill;
    spill_info.instr_index = instr_index;
    spill_info.is_spill = true;
    array::Push(spills, spill_info);
}

static void Unspill(Live_Interval spill, s64 instr_index)
{
    Spill_Info spill_info = { };
    spill_info.spill = spill;
    spill_info.instr_index = instr_index;
    spill_info.is_spill = false;
    array::Push(spills, spill_info);
}

static void SpillAtInterval(Codegen_Context *ctx, Array<Live_Interval> &active, Live_Interval interval)
{
    s64 spill_i = active.count-1;
    Live_Interval spill = active[spill_i];
    if (spill.end > interval.end)
    {
        Spill(spill, interval.start);

        interval.reg = spill.reg;
        GetLocalOffset(ctx, spill.name);
        array::Erase(active, spill_i);
        AddToActive(active, interval);
    }
    else
    {
        Spill(spill, interval.start);

        GetLocalOffset(ctx, interval.name);
    }
}

static void SpillFixedRegAtInterval(Codegen_Context *ctx, Array<Live_Interval> &active, Live_Interval interval)
{
    s64 spill_i = -1;
    for (s64 i = 0; i < active.count; i++)
    {
        if (active[i].reg == interval.reg)
        {
            spill_i = i;
            break;
        }
    }
    if (spill_i == -1)
    {
        RemoveFromFreeRegs(ctx, interval.reg);
        AddToActive(active, interval);
    }
    else
    {
        Live_Interval spill = active[spill_i];
        Spill(spill, interval.start);

        fprintf(stderr, "Spilled ");
        PrintName((IoFile*)stderr, spill.name);
        fprintf(stderr, "\n");

        GetLocalOffset(ctx, spill.name);
        array::Erase(active, spill_i);
        AddToActive(active, interval);
    }
}

static b32 SetRegOperand(Array<Live_Interval> active, Operand *oper, Name oper_name)
{
    if (oper_name.str.size == 0)
        return true;

    for (s64 i = 0; i < active.count; i++)
    {
        if (active[i].name == oper_name)
        {
            Operand reg_oper = RegOperand(active[i].reg, oper->data_type, oper->access_flags);
            reg_oper.addr_mode = oper->addr_mode;
            reg_oper.scale_offset = oper->scale_offset;
            *oper = reg_oper;
            return true;
        }
    }
    return false;
}

static b32 SetOperand(Codegen_Context *ctx,
        Array<Live_Interval> active, Array<Live_Interval> active_f,
        Operand *oper, s64 instr_index)
{
    Name oper_name = GetOperName(*oper);
    if (SetRegOperand(active, oper, oper_name)) return true;
    if (SetRegOperand(active_f, oper, oper_name)) return true;

    (void)instr_index;
    s64 offs;
    if (GetLocalOffset(ctx, oper_name, &offs))
    {
        *oper = BaseOffsetOperand(REG_rbp, offs, oper->access_flags);
        fprintf(stderr, "Local offset %" PRId64 " for ", offs);
        PrintName((IoFile*)stderr, oper_name);
        fprintf(stderr, " at %" PRId64 "\n", instr_index);
    }
    else
    {
        fprintf(stderr, "No local offset for ");
        PrintName((IoFile*)stderr, oper_name);
        fprintf(stderr, " at %" PRId64 "\n", instr_index);
        //INVALID_CODE_PATH;
    }
    return false;
}

static void SpillCallerSaves(Reg_Alloc *reg_alloc, Array<Live_Interval> active, s64 instr_index)
{
    for (s64 i = 0; i < active.count; i++)
    {
        if (!active[i].is_fixed && IsCallerSave(reg_alloc, active[i].reg))
        {
            Spill(active[i], instr_index);
        }
    }
}

static void UnspillCallerSaves(Reg_Alloc *reg_alloc, Array<Live_Interval> active, s64 instr_index)
{
    for (s64 i = 0; i < active.count; i++)
    {
        if (!active[i].is_fixed && IsCallerSave(reg_alloc, active[i].reg))
        {
            Unspill(active[i], instr_index);
        }
    }
}

static void ScanInstructions(Codegen_Context *ctx, Routine *routine,
        Array<Live_Interval> active, Array<Live_Interval> active_f,
        s64 interval_start, s64 next_interval_start)
{
    Reg_Alloc *reg_alloc = ctx->reg_alloc;
    for (s64 instr_i = interval_start; instr_i <= next_interval_start; instr_i++)
    {
        fprintf(stderr, "scanning instr %" PRId64 "\n", instr_i);
        Instruction *instr = routine->instructions[instr_i];
        if ((Amd64_Opcode)instr->opcode == OP_call)
        {
            SpillCallerSaves(reg_alloc, active, instr_i);
            SpillCallerSaves(reg_alloc, active_f, instr_i);
            UnspillCallerSaves(reg_alloc, active, instr_i + 1);
            UnspillCallerSaves(reg_alloc, active_f, instr_i + 1);
        }
        SetOperand(ctx, active, active_f, &instr->oper1, instr_i);
        SetOperand(ctx, active, active_f, &instr->oper2, instr_i);
        SetOperand(ctx, active, active_f, &instr->oper3, instr_i);
    }
}

static void LinearScanRegAllocation(Codegen_Context *ctx, Array<Live_Interval> live_intervals)
{
    array::Clear(spills);

    Reg_Alloc *reg_alloc = ctx->reg_alloc;
    ClearRegAllocs(reg_alloc);
    //for (s64 i = 0; i < reg_alloc->general_reg_count; i++)
    //    array::Push(reg_alloc->free_regs, reg_alloc->general_regs[i]);
    //for (s64 i = 0; i < reg_alloc->float_reg_count; i++)
    //    array::Push(reg_alloc->free_float_regs, reg_alloc->float_regs[i]);
    //for (s64 i = 0; i < ir_routine->arg_count; i++)
    //{
    //    Ir_Operand *arg = ir_routine->args[i];
    //    Oper_Data_Type data_type = DataTypeFromType(arg->type);
    //    const Reg *arg_reg = GetArgRegister(reg_alloc, data_type, i);
    //}

    Routine *routine = ctx->current_routine;

    s32 last_interval_start = 0;
    s32 last_interval_end = 0;
    Array<Live_Interval> active = { };
    Array<Live_Interval> active_f = { };
    for (s64 i = 0; i < live_intervals.count; i++)
    {
        Live_Interval interval = live_intervals[i];
        s64 next_interval_start = 0; //interval.start;
        if (i + 1 < live_intervals.count)
            next_interval_start = live_intervals[i + 1].start;

        ExpireOldIntervals(ctx, active, interval);
        ExpireOldIntervals(ctx, active_f, interval);
        if (interval.data_type == Oper_Data_Type::F32 ||
            interval.data_type == Oper_Data_Type::F64)
        {
            if (interval.reg.reg_index != REG_NONE)
            {
                SpillFixedRegAtInterval(ctx, active_f, interval);
            }
            else if (active_f.count == reg_alloc->float_reg_count)
            {
                SpillAtInterval(ctx, active_f, interval);
            }
            else
            {
                Reg reg = GetFreeRegister(reg_alloc, interval.data_type);
                interval.reg = reg;
                AddToActive(active_f, interval);
            }
        }
        else
        {
            if (interval.reg.reg_index != REG_NONE)
            {
                SpillFixedRegAtInterval(ctx, active, interval);
            }
            else if (active.count == reg_alloc->general_reg_count)
            {
                SpillAtInterval(ctx, active, interval);
            }
            else
            {
                Reg reg = GetFreeRegister(reg_alloc, interval.data_type);
                interval.reg = reg;
                AddToActive(active, interval);
            }
        }

        ScanInstructions(ctx, routine, active, active_f,
                interval.start, next_interval_start);

        last_interval_start = interval.start;
        if (interval.end > last_interval_end)
            last_interval_end = interval.end;
    }

    ScanInstructions(ctx, routine, active, active_f,
            last_interval_start, last_interval_end);

    //ScanInstructions(ctx, routine, active, active_f,
    //        last_interval_start, last_interval_end);

    InsertSpills(ctx);
}

static void GenerateCode(Codegen_Context *ctx, Ir_Routine *ir_routine)
{
    Routine *routine = ctx->current_routine;

    bool toplevel = (ir_routine->name.str.size == 0);
    if (toplevel)
    {
        routine->name = PushName(&ctx->arena, "init_");
    }
    else
    {
        routine->name = ir_routine->name;
    }

    PushPrologue(ctx, OP_push, RegOperand(REG_rbp, Oper_Data_Type::U64, AF_Read));
    PushPrologue(ctx, OP_mov,
            RegOperand(REG_rbp, Oper_Data_Type::U64, AF_Write),
            RegOperand(REG_rsp, Oper_Data_Type::U64, AF_Read));

    routine->return_label.name = PushName(&ctx->arena, ".ret_label");

    for (s64 i = 0; i < ir_routine->instructions.count; i++)
    {
        Ir_Instruction *ir_instr = &ir_routine->instructions[i];
        if (!ctx->comment)
            ctx->comment = &ir_instr->comment;
        GenerateCode(ctx, ir_routine, ir_instr);
    }
    if (toplevel)
    {
        Operand main_label = LabelOperand(ctx->comp_ctx->env.main_func_name, AF_Read);
        PushInstruction(ctx, OP_call, main_label);
    }
    PushInstruction(ctx, OP_LABEL, LabelOperand(routine->return_label.name, AF_Read));

    CollectLabelInstructions(ctx, routine);

    Array<Live_Interval*> live_interval_set = { };
    ComputeLiveness(ctx, ir_routine, routine, live_interval_set);

    Array<Live_Interval> live_intervals = { };
    for (s64 i = 0; i < live_interval_set.count; i++)
    {
        Live_Interval *interval = live_interval_set[i];
        if (!interval) continue;

        s64 index = 0;
        for (index = 0; index < live_intervals.count; index++)
        {
            if (interval->start < live_intervals[index].start)
                break;
        }
        array::Insert(live_intervals, index, *interval);
    }

    //fprintf(stderr, "liv set: %" PRId64 "\n", live_interval_set.count);
    //for (s64 i = 0; i < live_intervals.count; i++)
    //{
    //    Live_Interval interval = live_intervals[i];
    //    fprintf(stderr, "[%d,%d]\n", interval.start, interval.end);
    //}

    array::Free(live_interval_set);

    LinearScanRegAllocation(ctx, live_intervals);
    //AllocateRegisters(ctx, ir_routine);

    array::Free(live_intervals);

    s64 locals_size = ctx->current_routine->locals_size;
    if (locals_size > 0)
    {
        locals_size = Align(locals_size, 16);
        Operand locals_size_oper = ImmOperand(locals_size, AF_Read);

        PushPrologue(ctx, OP_sub, RegOperand(REG_rsp, Oper_Data_Type::U64, AF_Write), locals_size_oper);
        //PushEpilogue(ctx, OP_add, RegOperand(REG_rsp, AF_Write), locals_size_oper);
        PushEpilogue(ctx, OP_mov,
                RegOperand(REG_rsp, Oper_Data_Type::U64, AF_Write),
                RegOperand(REG_rbp, Oper_Data_Type::U64, AF_Read));
    }
    PushEpilogue(ctx, OP_pop, RegOperand(REG_rbp, Oper_Data_Type::U64, AF_Write));
    PushEpilogue(ctx, OP_ret);
}

void GenerateCode_Amd64(Codegen_Context *ctx, Ir_Routine_List ir_routines)
{
    ctx->routine_count = ir_routines.count;
    ctx->routines = PushArray<Routine>(&ctx->arena, ir_routines.count);
    for (s64 i = 0; i < ir_routines.count; i++)
    {
        ctx->current_routine = &ctx->routines[i];
        GenerateCode(ctx, array::At(ir_routines, i));
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
            //len += fprintf((FILE*)file, "%%");
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
            len += PrintName(file, oper.var.name);
            break;
    }
    return len;
}

static s64 PrintOperandV(IoFile *file, Operand oper)
{
    s64 len = 0;
    switch (oper.type)
    {
        case Oper_Type::None:
            break;
        case Oper_Type::Label:
            len += PrintName(file, oper.label.name);
            break;
        case Oper_Type::Temp:
            len += PrintName(file, oper.temp.name);
            break;
        case Oper_Type::Register:
            len += PrintName(file, GetRegName(oper.reg));
            break;
        case Oper_Type::FixedRegister:
            len += PrintName(file, GetRegName(oper.fixed_reg.reg));
            break;
        case Oper_Type::IrOperand:
            len += PrintIrOperand(file, *oper.ir_oper);
            break;
        case Oper_Type::Immediate:
            len += fprintf((FILE*)file, "%" PRIu64, oper.imm_u64);
            break;
    }
    return len;
}

static s64 PrintOperand(IoFile *file, Operand oper, const Operand *next_oper, b32 first)
{
    if (oper.type == Oper_Type::None) return 0;
    if (oper.addr_mode == Oper_Addr_Mode::IndexScale) return 0;

    s64 len = 0;
    if (!first)
        len += fprintf((FILE*)file, ", ");

    if (oper.addr_mode == Oper_Addr_Mode::BaseOffset ||
        oper.addr_mode == Oper_Addr_Mode::BaseIndexOffset)
    {
        len += fprintf((FILE*)file, "[");
        len += PrintOperandV(file, oper);
    }
    else if (oper.addr_mode == Oper_Addr_Mode::Direct)
    {
        len += PrintOperandV(file, oper);
    }

    if (oper.addr_mode == Oper_Addr_Mode::BaseIndexOffset)
    {
        ASSERT(next_oper);
        ASSERT(next_oper->addr_mode == Oper_Addr_Mode::IndexScale);
        ASSERT(next_oper->scale_offset != 0);

        s32 scale = next_oper->scale_offset;
        if (scale > 0)
        {
            len += fprintf((FILE*)file, "+");
        }
        else
        {
            len += fprintf((FILE*)file, "-");
            scale = -scale;
        }
        len += PrintOperandV(file, *next_oper);
        len += fprintf((FILE*)file, "*%" PRId32, scale);
    }
    if (oper.addr_mode == Oper_Addr_Mode::BaseOffset ||
        oper.addr_mode == Oper_Addr_Mode::BaseIndexOffset)
    {
        if (oper.scale_offset != 0)
        {
            if (oper.scale_offset > 0)
                len += fprintf((FILE*)file, "+");
            len += fprintf((FILE*)file, "%" PRId32, oper.scale_offset);
        }
        len += fprintf((FILE*)file, "]");
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

static s64 PrintComment(FILE *file, s64 line_len, Ir_Comment comment)
{
    s64 len = 0;
    if (comment.start)
    {
        PrintPadding((FILE*)file, line_len, 40);
        len += fprintf(file, "\t; ");
        len += fwrite(comment.start, 1, comment.end - comment.start, file);
    }
    return len;
}

static s64 PrintOpcode(IoFile *file, Amd64_Opcode opcode)
{
    s64 len = 0;
    len += fprintf((FILE*)file, "%s", opcode_names[opcode]);
    return len;
}

static void PrintInstruction(IoFile *file, const Instruction *instr)
{
    s64 len = 0;
    if ((Amd64_Opcode)instr->opcode == OP_LABEL)
    {
        len += PrintLabel(file, instr->oper1);
    }
    else
    {
        //len += fprintf((FILE*)file, "\t");
        len += PrintPadding((FILE*)file, len, 4);
        len += PrintOpcode(file, (Amd64_Opcode)instr->opcode);

        if (instr->oper1.type != Oper_Type::None)
        {
            len += PrintPadding((FILE*)file, len, 16);
            //len += fprintf((FILE*)file, "\t");
            len += PrintOperand(file, instr->oper1, &instr->oper2, true);
            len += PrintOperand(file, instr->oper2, &instr->oper3, false);
            len += PrintOperand(file, instr->oper3, nullptr, false);
        }
    }
    PrintComment((FILE*)file, len, instr->comment);
    fprintf((FILE*)file, "\n");
}

void PrintInstructions(IoFile *file, Instruction_List &instructions)
{
    for (s64 i = 0; i < instructions.count; i++)
    {
        PrintInstruction(file, array::At(instructions, i));
    }
}

void OutputCode_Amd64(Codegen_Context *ctx)
{
    IoFile *file = ctx->code_out;
    FILE *f = (FILE*)file;

    String filename = ctx->comp_ctx->modules[0]->module_file->filename;
    fprintf(f, "; -----\n");
    fprintf(f, "; Source file: "); PrintString(file, filename); fprintf(f, "\n");
    fprintf(f, "; Target:      %s\n", GetTargetString(ctx->target));
    fprintf(f, "; -----\n\n");

    fprintf(f, "bits 64\n\n");
    for (s64 routine_idx = 0; routine_idx < ctx->foreign_routine_count; routine_idx++)
    {
        Name foreign_routine = ctx->foreign_routines[routine_idx];
        fprintf(f, "extern ");
        PrintName(file, foreign_routine);
        fprintf(f, "\n");
    }
    fprintf(f, "\n");
    for (s64 routine_idx = 0; routine_idx < ctx->routine_count; routine_idx++)
    {
        Routine *routine = &ctx->routines[routine_idx];
        fprintf(f, "global ");
        PrintName(file, routine->name);
        fprintf(f, "\n");
    }

    fprintf(f, "\nsection .text\n\n");

    for (s64 routine_idx = 0; routine_idx < ctx->routine_count; routine_idx++)
    {
        Routine *routine = &ctx->routines[routine_idx];
        PrintName(file, routine->name);
        fprintf(f, ":\n");

        fprintf(f, "; prologue\n");
        PrintInstructions(file, routine->prologue);
        fprintf(f, "; callee save spills\n");
        PrintInstructions(file, routine->callee_save_spills);
        fprintf(f, "; routine body\n");
        PrintInstructions(file, routine->instructions);
        fprintf(f, "; epilogue\n");
        PrintInstructions(file, routine->epilogue);
        fprintf(f, "; -----\n\n");
    }

    fprintf(f, "\nsection .data\n");

    // globals

    // constants

    if (ctx->float32_consts.count)
    {
        fprintf(f, "\nalign 4\n");
        for (s64 i = 0; i < ctx->float32_consts.count; i++)
        {
            Float32_Const fconst = ctx->float32_consts[i];
            PrintName(file, fconst.label_name);
            fprintf(f, ":\tdd\t%" PRIu32 "\n", fconst.uvalue);
        }
    }
    if (ctx->float64_consts.count)
    {
        fprintf(f, "\nalign 8\n");
        for (s64 i = 0; i < ctx->float64_consts.count; i++)
        {
            Float64_Const fconst = ctx->float64_consts[i];
            PrintName(file, fconst.label_name);
            fprintf(f, ":\tdq\t%" PRIu64 "\n", fconst.uvalue);
        }
    }

    if (ctx->str_consts.count)
    {
        Array<String> str_data = { };

        fprintf(f, "\nalign 8\n");
        for (s64 i = 0; i < ctx->str_consts.count; i++)
        {
            String_Const sconst = ctx->str_consts[i];
            PrintName(file, sconst.label_name);
            fprintf(f, ":\n\tdq\t%" PRIu64 "\n", sconst.value.size);
            fprintf(f, "\tdq\tstr_data@%" PRId64 "\n", i);
            array::Push(str_data, sconst.value);
        }

        fprintf(f, "\nalign 1\n");
        for (s64 i = 0; i < str_data.count; i++)
        {
            String str = str_data[i];
            fprintf(f, "str_data@%" PRId64 ":\n", i);
            fprintf(f, "\tdb\t");
            if (str.size > 0)
            {
                s32 c = str.data[0];
                fprintf(f, "%d", c);
                for (s64 si = 1; si < str.size; si++)
                {
                    s32 c = str.data[si];
                    fprintf(f, ",%d", c);
                }
                fprintf(f, ",0");
            }
            else
            {
                fprintf(f, "0");
            }
            fprintf(f, "\n");
        }

        array::Free(str_data);
    }
}

} // hplang
