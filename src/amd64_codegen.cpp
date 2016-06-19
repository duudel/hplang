
#include "amd64_codegen.h"
#include "common.h"
#include "compiler.h"
#include "reg_alloc.h"
#include "symbols.h"
#include "hashtable.h"

#include <cstdio>
#include <cinttypes>

// The register allocation is implemented with a linear scan register
// allocator.  The algorithm was first described by [1] and a more optimal
// interval splitting algorithm was given by [2].
//
// The implementation here follows [1], but could be extended to support more
// optimal interval split positining with the use position structure as
// described by [2].
//
// [1]  Massimiliano Poletto and Vivek Sarkar, 1998.
//      Linear Scan Register Allocation.
//      http://web.cs.ucla.edu/~palsberg/course/cs132/linearscan.pdf
//
// [2]  Christian Wimmer and Haspeter Mössenböck, 2004.
//      Optimized Interval Splitting in a Linear Scan Register Allocator.
//      https://www.usenix.org/legacy/events/vee05/full_papers/p132-wimmer.pdf

namespace hplang
{

#define RA_DEBUG(ctx, x) \
        if ((ctx)->comp_ctx->options.debug_reg_alloc) x

enum { Opcode_Mod_Shift = 7 };

enum Opcode_Mod
{
    NO_MOD  = 0,
    O1_REG  = 0x01,
    O1_MEM  = 0x02,
    O1_RM   = O1_REG | O1_MEM,
    O1_IMM  = 0x04,
    //O1_W8   = 0x08,
    //O1_W16  = 0x10,
    //O1_W32  = 0x20,
    //O1_W64  = 0x40,

    O2_REG = (O1_REG << Opcode_Mod_Shift),
    O2_MEM = (O1_MEM << Opcode_Mod_Shift),
    O2_RM = O2_REG | O2_MEM,
    O2_IMM = (O1_IMM << Opcode_Mod_Shift),

    O3_REG = (O2_REG << Opcode_Mod_Shift),
    O3_MEM = (O2_MEM << Opcode_Mod_Shift),
    O3_RM = O3_REG | O3_MEM,
    O3_IMM = (O2_IMM << Opcode_Mod_Shift),
};

// Disabled or non-valid instruction opcode
#define PASTE_OP_D(x, mods)

// NOTE(henrik): Do we want (comiss and comisd) or (ucomiss and ucomisd)?

// NOTE(henrik): Conditional move opcode cmovg is not valid when the operands
// are 64 bit wide. The condition "cmovg a, b" can be replaced with "cmovl b, a".
#define OPCODES\
    PASTE_OP(LABEL,     NO_MOD)\
    PASTE_OP(SPILL,     NO_MOD)\
    \
    PASTE_OP(nop,       NO_MOD)\
    \
    PASTE_OP(call,      NO_MOD)\
    PASTE_OP(ret,       NO_MOD)\
    PASTE_OP(jmp,       NO_MOD)\
    PASTE_OP(je,        NO_MOD)\
    PASTE_OP(jne,       NO_MOD)\
    PASTE_OP(jb,        NO_MOD)\
    PASTE_OP(jbe,       NO_MOD)\
    PASTE_OP(ja,        NO_MOD)\
    PASTE_OP(jae,       NO_MOD)\
    PASTE_OP(jl,        NO_MOD)\
    PASTE_OP(jle,       NO_MOD)\
    PASTE_OP(jg,        NO_MOD)\
    PASTE_OP(jge,       NO_MOD)\
    \
    PASTE_OP(cmp,       O1_REG | O2_REG | O2_IMM)\
    PASTE_OP(comiss,    O1_REG | O2_RM)\
    PASTE_OP(comisd,    O1_REG | O2_RM)\
    \
    PASTE_OP(lea,       O1_REG | O2_MEM)\
    PASTE_OP(mov,       O1_RM | O2_RM | O2_IMM)\
    PASTE_OP(movsx,     O1_RM | O2_RM | O2_IMM)\
    PASTE_OP(movzx,     O1_RM | O2_RM | O2_IMM)\
    PASTE_OP(movss,     O1_RM | O2_RM)\
    PASTE_OP(movsd,     O1_RM | O2_RM)\
    \
    PASTE_OP(cmove,     O1_REG | O2_RM)\
    PASTE_OP(cmovne,    O1_REG | O2_RM)\
    PASTE_OP(cmova,     O1_REG | O2_RM)\
    PASTE_OP(cmovae,    O1_REG | O2_RM)\
    PASTE_OP(cmovb,     O1_REG | O2_RM)\
    PASTE_OP(cmovbe,    O1_REG | O2_RM)\
    PASTE_OP(cmovl,     O1_REG | O2_RM)\
    PASTE_OP(cmovle,    O1_REG | O2_RM)\
    PASTE_OP_D(cmovg,   O1_REG | O2_RM)\
    PASTE_OP(cmovge,    O1_REG | O2_RM)\
    \
    PASTE_OP(cqo,       NO_MOD)\
    \
    PASTE_OP(add,       O1_REG | O2_RM | O2_IMM)\
    PASTE_OP(sub,       O1_REG | O2_RM | O2_IMM)\
    PASTE_OP(mul,       O1_RM)\
    PASTE_OP(imul,      O1_REG | O2_RM)\
    PASTE_OP(div,       O1_RM)\
    PASTE_OP(idiv,      O1_RM)\
    PASTE_OP(and,       O1_REG | O2_RM | O2_IMM)\
    PASTE_OP(or,        O1_REG | O2_RM | O2_IMM)\
    PASTE_OP(xor,       O1_REG | O2_RM | O2_IMM)\
    PASTE_OP(neg,       O1_REG)\
    PASTE_OP(not,       O1_REG)\
    PASTE_OP(sal,       O1_REG | O2_REG | O2_IMM)\
    PASTE_OP(shl,       O1_REG | O2_REG | O2_IMM)\
    PASTE_OP(sar,       O1_REG | O2_REG | O2_IMM)\
    PASTE_OP(shr,       O1_REG | O2_REG | O2_IMM)\
    \
    PASTE_OP(addss,     O1_REG | O2_REG)\
    PASTE_OP(subss,     O1_REG | O2_REG)\
    PASTE_OP(mulss,     O1_REG | O2_REG)\
    PASTE_OP(divss,     O1_REG | O2_REG)\
    PASTE_OP(addsd,     O1_REG | O2_REG)\
    PASTE_OP(subsd,     O1_REG | O2_REG)\
    PASTE_OP(mulsd,     O1_REG | O2_REG)\
    PASTE_OP(divsd,     O1_REG | O2_REG)\
    \
    PASTE_OP(sqrtss,    O1_REG | O2_REG)\
    PASTE_OP(sqrtsd,    O1_REG | O2_REG)\
    \
    PASTE_OP(push,      O1_REG)\
    PASTE_OP(pop,       O1_REG)\
    \
    PASTE_OP(cvtsi2ss,  O1_REG | O2_REG)\
    PASTE_OP(cvtsi2sd,  O1_REG | O2_REG)\
    PASTE_OP(cvtss2si,  O1_REG | O2_REG)\
    PASTE_OP(cvtsd2si,  O1_REG | O2_REG)\
    PASTE_OP(cvtss2sd,  O1_REG | O2_REG)\
    PASTE_OP(cvtsd2ss,  O1_REG | O2_REG)\


#define PASTE_OP(x, mods) OP_##x,
enum Amd64_Opcode
{
    OPCODES
};
#undef PASTE_OP

#define PASTE_OP(x, mods) mods,
static u32 opcode_flags[] = {
    OPCODES
};
#undef PASTE_OP


#define PASTE_OP(x, mods) #x,
static const char *opcode_names[] = {
    OPCODES
};
#undef PASTE_OP

/* AMD64 registers
 * rip and mmx registers not listed.
 */
#define REGS\
    PASTE_REG(NONE, NONE, NONE, NONE)\
    PASTE_REG(rax, eax, ax, al)\
    PASTE_REG(rbx, ebx, bx, bl)\
    PASTE_REG(rcx, ecx, cx, cl)\
    PASTE_REG(rdx, edx, dx, dl)\
    PASTE_REG(rbp, ebp, bp, bpl)\
    PASTE_REG(rsi, esi, si, sil)\
    PASTE_REG(rdi, edi, di, dil)\
    PASTE_REG(rsp, esp, sp, spl)\
    PASTE_REG(r8, r8d, r8w, r8b)\
    PASTE_REG(r9, r9d, r9w, r9b)\
    PASTE_REG(r10, r10d, r10w, r10b)\
    PASTE_REG(r11, r11d, r11w, r11b)\
    PASTE_REG(r12, r12d, r12w, r12b)\
    PASTE_REG(r13, r13d, r13w, r13b)\
    PASTE_REG(r14, r14d, r14w, r14b)\
    PASTE_REG(r15, r15d, r15w, r15b)\
    \
    PASTE_REG(xmm0, xmm0, xmm0, xmm0)\
    PASTE_REG(xmm1, xmm1, xmm1, xmm1)\
    PASTE_REG(xmm2, xmm2, xmm2, xmm2)\
    PASTE_REG(xmm3, xmm3, xmm3, xmm3)\
    PASTE_REG(xmm4, xmm4, xmm4, xmm4)\
    PASTE_REG(xmm5, xmm5, xmm5, xmm5)\
    PASTE_REG(xmm6, xmm6, xmm6, xmm6)\
    PASTE_REG(xmm7, xmm7, xmm7, xmm7)\
    PASTE_REG(xmm8, xmm8, xmm8, xmm8)\
    PASTE_REG(xmm9, xmm9, xmm9, xmm9)\
    PASTE_REG(xmm10, xmm10, xmm10, xmm10)\
    PASTE_REG(xmm11, xmm11, xmm11, xmm11)\
    PASTE_REG(xmm12, xmm12, xmm12, xmm12)\
    PASTE_REG(xmm13, xmm13, xmm13, xmm13)\
    PASTE_REG(xmm14, xmm14, xmm14, xmm14)\
    PASTE_REG(xmm15, xmm15, xmm15, xmm15)\

#define PASTE_REG(r8, r4, r2, r1) REG_##r8,
enum Amd64_Register
{
    REGS
    REG_COUNT
};
#undef PASTE_REG

#define PASTE_REG(r8, r4, r2, r1) #r8,
static const char *reg_name_strings_8b[] = {
    REGS
};
#undef PASTE_REG

#define PASTE_REG(r8, r4, r2, r1) #r4,
static const char *reg_name_strings_4b[] = {
    REGS
};
#undef PASTE_REG

#define PASTE_REG(r8, r4, r2, r1) #r2,
static const char *reg_name_strings_2b[] = {
    REGS
};
#undef PASTE_REG

#define PASTE_REG(r8, r4, r2, r1) #r1,
static const char *reg_name_strings_1b[] = {
    REGS
};
#undef PASTE_REG


#define PASTE_REG(r8, r4, r2, r1) #r8 "@@save",
static const char *reg_save_name_strings[] = {
    REGS
};
#undef PASTE_REG

#define PASTE_REG(r8, r4, r2, r1) {},
static Name reg_save_names[] = {
    REGS
};
#undef PASTE_REG

static const char* GetRegNameStr(Reg reg)
{
    return reg_name_strings_8b[reg.reg_index];
}

static const char* GetRegNameStr(Reg reg, Oper_Data_Type data_type)
{
    switch (data_type)
    {
    case Oper_Data_Type::BOOL:
    case Oper_Data_Type::S8:
    case Oper_Data_Type::U8:
        return reg_name_strings_1b[reg.reg_index];
    case Oper_Data_Type::S16:
    case Oper_Data_Type::U16:
        return reg_name_strings_2b[reg.reg_index];
    case Oper_Data_Type::S32:
    case Oper_Data_Type::U32:
        return reg_name_strings_4b[reg.reg_index];
    case Oper_Data_Type::S64:
    case Oper_Data_Type::U64:
    case Oper_Data_Type::PTR:
        return reg_name_strings_8b[reg.reg_index];
    case Oper_Data_Type::F32:
    case Oper_Data_Type::F64:
        return reg_name_strings_8b[reg.reg_index];
    }
    INVALID_CODE_PATH;
    return nullptr;
}

Reg MakeReg(Amd64_Register r)
{
    Reg reg;
    reg.reg_index = r;
    return reg;
}


// Windows AMD64 ABI register usage
static Reg_Info win_reg_info[] = {
    { REG_NONE,    -1,  RF_None | RF_NonAllocable },
    { REG_rax,      0,  RF_CallerSave | RF_Return },
    { REG_rbx,     -1,  RF_None },
    { REG_rcx,      0,  RF_CallerSave | RF_Arg },
    { REG_rdx,      1,  RF_CallerSave | RF_Arg },
    { REG_rbp,     -1,  RF_NonAllocable },
    { REG_rsi,     -1,  RF_None },
    { REG_rdi,     -1,  RF_None },
    { REG_rsp,     -1,  RF_NonAllocable },
    { REG_r8,       2,  RF_CallerSave | RF_Arg },
    { REG_r9,       3,  RF_CallerSave | RF_Arg },
    { REG_r10,     -1,  RF_CallerSave },
    { REG_r11,     -1,  RF_CallerSave },
    { REG_r12,     -1,  RF_None },
    { REG_r13,     -1,  RF_None },
    { REG_r14,     -1,  RF_None },
    { REG_r15,     -1,  RF_None },

    { REG_xmm0,     0,   RF_CallerSave | RF_Arg | RF_Return | RF_Float },
    { REG_xmm1,     1,   RF_CallerSave | RF_Arg | RF_Float },
    { REG_xmm2,     2,   RF_CallerSave | RF_Arg | RF_Float },
    { REG_xmm3,     3,   RF_CallerSave | RF_Arg | RF_Float },
    { REG_xmm4,    -1,   RF_CallerSave | RF_Float },
    { REG_xmm5,    -1,   RF_CallerSave | RF_Float },
    { REG_xmm6,    -1,   RF_CallerSave | RF_Float },
    { REG_xmm7,    -1,   RF_CallerSave | RF_Float },
    { REG_xmm8,    -1,   RF_Float },
    { REG_xmm9,    -1,   RF_Float },
    { REG_xmm10,   -1,   RF_Float },
    { REG_xmm11,   -1,   RF_Float },
    { REG_xmm12,   -1,   RF_Float },
    { REG_xmm13,   -1,   RF_Float },
    { REG_xmm14,   -1,   RF_Float },
    { REG_xmm15,   -1,   RF_Float },
};

// Unix System V ABI register usage
static Reg_Info nix_reg_info[] = {
    { REG_NONE,    -1,  RF_None | RF_NonAllocable },
    { REG_rax,      0,  RF_CallerSave | RF_Return },
    { REG_rbx,     -1,  RF_None },
    { REG_rcx,      3,  RF_CallerSave | RF_Arg },
    // NOTE(henrik): cannot use rdx as the second return register until
    // Reg_Info can store both arg and return register index.
    { REG_rdx,      2,  RF_CallerSave | RF_Arg }, //| RF_Return },
    { REG_rbp,     -1,  RF_NonAllocable },
    { REG_rsi,      1,  RF_CallerSave | RF_Arg },
    { REG_rdi,      0,  RF_CallerSave | RF_Arg },
    { REG_rsp,     -1,  RF_NonAllocable },
    { REG_r8,       4,  RF_CallerSave | RF_Arg },
    { REG_r9,       5,  RF_CallerSave | RF_Arg },
    { REG_r10,     -1,  RF_CallerSave },
    { REG_r11,     -1,  RF_CallerSave },
    { REG_r12,     -1,  RF_None },
    { REG_r13,     -1,  RF_None },
    { REG_r14,     -1,  RF_None },
    { REG_r15,     -1,  RF_None },

    { REG_xmm0,     0,   RF_CallerSave | RF_Arg | RF_Return | RF_Float },
    { REG_xmm1,     1,   RF_CallerSave | RF_Arg | RF_Return | RF_Float },
    { REG_xmm2,     2,   RF_CallerSave | RF_Arg | RF_Float },
    { REG_xmm3,     3,   RF_CallerSave | RF_Arg | RF_Float },
    { REG_xmm4,     4,   RF_CallerSave | RF_Arg | RF_Float },
    { REG_xmm5,     5,   RF_CallerSave | RF_Arg | RF_Float },
    { REG_xmm6,     6,   RF_CallerSave | RF_Arg | RF_Float },
    { REG_xmm7,     7,   RF_CallerSave | RF_Arg | RF_Float },
    { REG_xmm8,    -1,   RF_CallerSave | RF_Float },
    { REG_xmm9,    -1,   RF_CallerSave | RF_Float },
    { REG_xmm10,   -1,   RF_CallerSave | RF_Float },
    { REG_xmm11,   -1,   RF_CallerSave | RF_Float },
    { REG_xmm12,   -1,   RF_CallerSave | RF_Float },
    { REG_xmm13,   -1,   RF_CallerSave | RF_Float },
    { REG_xmm14,   -1,   RF_CallerSave | RF_Float },
    { REG_xmm15,   -1,   RF_CallerSave | RF_Float },
};


void InitializeCodegen_Amd64(Codegen_Context *ctx, Codegen_Target cg_target)
{
    // TODO(henrik): Fix this! Do not do this. Are the names even needed?
    for (s64 i = 0; i < array_length(reg_save_names); i++)
    {
        reg_save_names[i] = PushName(&ctx->arena, reg_save_name_strings[i]);
    }
    ctx->return_label_name = PushName(&ctx->arena, ".ret_label");
    switch (cg_target)
    {
    case CGT_COUNT:
        INVALID_CODE_PATH;
        break;

    case CGT_AMD64_Windows:
        InitRegAlloc(ctx->reg_alloc,
                array_length(win_reg_info),
                win_reg_info,
                true,                       // Is argument register index shared between general and float registers.
                4);                         // Count of argument registers that need shadow space backing.
        break;
    case CGT_AMD64_Unix:
        InitRegAlloc(ctx->reg_alloc,
                array_length(nix_reg_info),
                nix_reg_info,
                false,
                0);
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

static Operand RW_(Operand oper)
{
    oper.access_flags = AF_ReadWrite;
    return oper;
}

static Operand S_(Operand oper)
{
    oper.access_flags |= AF_Shadow;
    return oper;
}

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

static Operand FixedRegOperand(Codegen_Context *ctx, Amd64_Register reg, Oper_Data_Type data_type, Oper_Access_Flags access_flags)
{
    return FixedRegOperand(ctx, MakeReg(reg), data_type, access_flags);
}

static Operand VirtualRegOperand(Name name, Oper_Data_Type data_type, Oper_Access_Flags access_flags)
{
    Operand result = { };
    result.type = Oper_Type::VirtualRegister;
    result.access_flags = access_flags;
    result.data_type = data_type;
    result.virtual_reg.name = name;
    return result;
}

static Operand TempOperand(Codegen_Context *ctx, Oper_Data_Type data_type, Oper_Access_Flags access_flags)
{
    const s64 buf_size = 40;
    char buf[buf_size];
    s64 temp_name_len = snprintf(buf, buf_size, "cg_temp@%" PRId64, ctx->temp_id);
    ctx->temp_id++;

    Name name = PushName(&ctx->arena, buf, temp_name_len);
    return VirtualRegOperand(name, data_type, access_flags);
}

static Operand TempFloat32Operand(Codegen_Context *ctx, Oper_Access_Flags access_flags)
{
    return TempOperand(ctx, Oper_Data_Type::F32, access_flags);
}

static Operand TempFloat64Operand(Codegen_Context *ctx, Oper_Access_Flags access_flags)
{
    return TempOperand(ctx, Oper_Data_Type::F64, access_flags);
}

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

static Operand StringConst(Codegen_Context *ctx, String value, Oper_Access_Flags access_flags)
{
    for (s64 i = 0; i < ctx->str_consts.count; i++)
    {
        String_Const str_const = ctx->str_consts[i];
        if (str_const.value == value)
        {
            Operand str_label = LabelOperand(str_const.label_name, access_flags);
            str_label.addr_mode = Oper_Addr_Mode::BaseOffset;
            return str_label;
        }
    }
    s64 buf_size = 40;
    char buf[buf_size];
    s64 name_len = snprintf(buf, buf_size, "str@%" PRId64 "", ctx->str_consts.count);
    Name label_name = PushName(&ctx->arena, buf, name_len);

    String_Const str_const = { };
    str_const.label_name = label_name;
    str_const.value = value;
    array::Push(ctx->str_consts, str_const);

    Operand str_label = LabelOperand(label_name, access_flags);
    str_label.addr_mode = Oper_Addr_Mode::BaseOffset;
    return str_label;
}

static Operand IrOperand(Codegen_Context *ctx,
        Ir_Operand *ir_oper, Oper_Access_Flags access_flags)
{
    Operand result = { };
    Oper_Data_Type data_type = DataTypeFromType(ir_oper->type);
    switch (ir_oper->oper_type)
    {
        case IR_OPER_None:
            INVALID_CODE_PATH;
            break;
        case IR_OPER_Label:
            return LabelOperand(ir_oper->label->name, access_flags);
        case IR_OPER_Routine:
        case IR_OPER_ForeignRoutine:
            return LabelOperand(ir_oper->var.name, access_flags);
        case IR_OPER_Variable:
            return VirtualRegOperand(ir_oper->var.name, data_type, access_flags);
        case IR_OPER_Temp:
            return VirtualRegOperand(ir_oper->temp.name, data_type, access_flags);
        case IR_OPER_GlobalVariable:
            {
                Operand var = LabelOperand(ir_oper->var.name, access_flags);
                var.data_type = data_type;
                var.addr_mode = Oper_Addr_Mode::BaseOffset;
                return var;
            }
        case IR_OPER_Immediate:
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
                case TYP_string:
                    return StringConst(ctx, ir_oper->imm_str, access_flags);
                default:
                    break;
            }
        }
    }
    INVALID_CODE_PATH;
    return result;
}

static Operand BaseOffsetOperand(Operand base, s64 offset, Oper_Access_Flags access_flags)
{
    ASSERT(base.scale_offset == 0);
    Operand result = base;
    result.access_flags = access_flags;
    result.addr_mode = Oper_Addr_Mode::BaseOffset;
    result.scale_offset = offset;
    return result;
}

static Operand BaseOffsetOperand(Amd64_Register base, s64 offset,
        Oper_Data_Type data_type, Oper_Access_Flags access_flags)
{
    Operand base_reg = RegOperand(base, data_type, access_flags);
    return BaseOffsetOperand(base_reg, offset, access_flags);
}

static Operand BaseOffsetOperand(Codegen_Context *ctx,
        Ir_Operand *base, s64 offset, Oper_Data_Type data_type, Oper_Access_Flags access_flags)
{
    Operand base_oper = IrOperand(ctx, base, access_flags);
    base_oper.data_type = data_type;
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

static Operand BaseIndexOffsetOperand(Codegen_Context *ctx,
        Ir_Operand *base, s64 offset, Oper_Data_Type data_type, Oper_Access_Flags access_flags)
{
    Operand base_oper = IrOperand(ctx, base, access_flags);
    base_oper.data_type = data_type;
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

static Operand IndexScaleOperand(Codegen_Context *ctx,
        Ir_Operand *index, s64 scale, Oper_Access_Flags access_flags)
{
    Operand index_oper = IrOperand(ctx, index, access_flags);
    return IndexScaleOperand(index_oper, scale, access_flags);
}

static inline void MakeNop(Instruction *instr)
{
    instr->opcode = (Opcode)OP_nop;
    instr->oper1 = NoneOperand();
    instr->oper2 = NoneOperand();
    instr->oper3 = NoneOperand();
}

static inline Instruction* NewInstruction(
        Codegen_Context *ctx,
        Amd64_Opcode opcode,
        Operand oper1 = NoneOperand(),
        Operand oper2 = NoneOperand(),
        Operand oper3 = NoneOperand())
{
    Instruction *instr = PushStruct<Instruction>(&ctx->arena);
    *instr = { };
    instr->opcode = (Opcode)opcode;
    instr->oper1 = oper1;
    instr->oper2 = oper2;
    instr->oper3 = oper3;
    if (opcode == OP_mov)
    {
        ASSERT(oper1.type != Oper_Type::None);
        ASSERT((oper1.access_flags & AF_Shadow) == 0);
        ASSERT(oper2.type != Oper_Type::None);
        ASSERT((oper2.access_flags & AF_Shadow) == 0);
    }
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
        case OP_jne:
        case OP_jl:
        case OP_jle:
        case OP_jg:
        case OP_jge:
        case OP_jb:
        case OP_jbe:
        case OP_ja:
        case OP_jae:
            flags = IF_FallsThrough;
            flags = flags | IF_Branch; break;
    }
    instr->flags = flags;
    return instr;
}

static Instruction* LoadFloat32Imm(Codegen_Context *ctx, Operand dest, f32 value)
{
    // TODO(henrik): Use hashtable for constant floats and strings
    for (s64 i = 0; i < ctx->float32_consts.count; i++)
    {
        Float32_Const fconst = ctx->float32_consts[i];
        if (fconst.value == value)
        {
            Operand float_label = LabelOperand(fconst.label_name, AF_Read);
            float_label.data_type = Oper_Data_Type::F32;
            float_label.addr_mode = Oper_Addr_Mode::BaseOffset;
            return NewInstruction(ctx, OP_movss, dest, float_label);
        }
    }

    s64 buf_size = 40;
    char buf[buf_size];
    s64 name_len = snprintf(buf, buf_size, "f32@%" PRId64 "", ctx->float32_consts.count);
    Name label_name = PushName(&ctx->arena, buf, name_len);

    Float32_Const fconst = { };
    fconst.label_name = label_name;
    fconst.value = value;
    array::Push(ctx->float32_consts, fconst);

    Operand float_label = LabelOperand(label_name, AF_Read);
    float_label.data_type = Oper_Data_Type::F32;
    float_label.addr_mode = Oper_Addr_Mode::BaseOffset;
    return NewInstruction(ctx, OP_movss, dest, float_label);
}

static Instruction* LoadFloat64Imm(Codegen_Context *ctx, Operand dest, f64 value)
{
    for (s64 i = 0; i < ctx->float64_consts.count; i++)
    {
        Float64_Const fconst = ctx->float64_consts[i];
        if (fconst.value == value)
        {
            Operand float_label = LabelOperand(fconst.label_name, AF_Read);
            float_label.data_type = Oper_Data_Type::F64;
            float_label.addr_mode = Oper_Addr_Mode::BaseOffset;
            return NewInstruction(ctx, OP_movsd, dest, float_label);
        }
    }

    s64 buf_size = 40;
    char buf[buf_size];
    s64 name_len = snprintf(buf, buf_size, "f64@%" PRId64 "", ctx->float64_consts.count);
    Name label_name = PushName(&ctx->arena, buf, name_len);

    Float64_Const fconst = { };
    fconst.label_name = label_name;
    fconst.value = value;
    array::Push(ctx->float64_consts, fconst);

    Operand float_label = LabelOperand(label_name, AF_Read);
    float_label.data_type = Oper_Data_Type::F64;
    float_label.addr_mode = Oper_Addr_Mode::BaseOffset;
    return NewInstruction(ctx, OP_movsd, dest, float_label);
}

static Amd64_Opcode MoveOp(Oper_Data_Type data_type)
{
    if (data_type == Oper_Data_Type::F32) return OP_movss;
    if (data_type == Oper_Data_Type::F64) return OP_movsd;
    return OP_mov;
}

static Operand LoadImmediates(Codegen_Context *ctx,
        Amd64_Opcode opcode,
        s64 oper_idx,
        Instruction_List &instructions,
        s64 instr_index,
        Operand oper,
        bool o1_mem = false)
{
    if ((oper.access_flags & AF_Shadow) != 0) return oper;

    u32 opflags = opcode_flags[opcode];
    u8 opshift = oper_idx * Opcode_Mod_Shift;
    if (oper.type == Oper_Type::Immediate)
    {
        if (oper.data_type == Oper_Data_Type::F32)
        {
            Operand dest = TempFloat32Operand(ctx, AF_Write);
            Instruction *load_const = LoadFloat32Imm(ctx, dest, oper.imm_f32);
            array::Insert(instructions, instr_index, load_const);

            dest.access_flags = oper.access_flags;
            dest.addr_mode = oper.addr_mode;
            dest.scale_offset = oper.scale_offset;
            return dest;
        }
        else if (oper.data_type == Oper_Data_Type::F64)
        {
            Operand dest = TempFloat64Operand(ctx, AF_Write);
            Instruction *load_const = LoadFloat64Imm(ctx, dest, oper.imm_f64);
            array::Insert(instructions, instr_index, load_const);

            dest.access_flags = oper.access_flags;
            dest.addr_mode = oper.addr_mode;
            dest.scale_offset = oper.scale_offset;
            return dest;
        }
        else if ((opflags & (O1_IMM << opshift)) == 0)
        {
            Operand temp = TempOperand(ctx, oper.data_type, AF_Write);
            Oper_Addr_Mode addr_mode = oper.addr_mode;
            oper.addr_mode = Oper_Addr_Mode::Direct;
            Instruction *load = NewInstruction(ctx, MoveOp(oper.data_type), temp, oper);
            array::Insert(instructions, instr_index, load);

            temp.access_flags = oper.access_flags;
            temp.addr_mode = addr_mode;
            temp.scale_offset = oper.scale_offset;
            return temp;
        }
    }
    else if (oper.addr_mode == Oper_Addr_Mode::BaseOffset
            //|| oper.addr_mode == Oper_Addr_Mode::BaseIndexOffset
            )
    {
        if ((opflags & (O1_MEM << opshift)) == 0 ||
            (oper_idx > 0 && o1_mem))
        {
            Operand temp = TempOperand(ctx, oper.data_type, AF_Write);
            Instruction *load = NewInstruction(ctx, MoveOp(oper.data_type), temp, oper);
            array::Insert(instructions, instr_index, load);

            temp.access_flags = oper.access_flags;
            return temp;
        }
    }
    /*else if (oper.addr_mode == Oper_Addr_Mode::BaseIndexOffset)
    {
        if (oper.type == Oper_Type::Label)
        {
            s32 scale_offset = oper.scale_offset;
            Oper_Data_Type data_type = oper.data_type;
            Operand temp = TempOperand(ctx, Oper_Data_Type::PTR, AF_Write);
            oper.scale_offset = 0;
            oper.addr_mode = Oper_Addr_Mode::BaseOffset;
            Instruction *load = NewInstruction(ctx, OP_lea, temp, R_(oper));
            array::Insert(instructions, instr_index, load);

            temp.data_type = data_type; //Oper_Data_Type::PTR;
            temp.access_flags = oper.access_flags;
            temp.addr_mode = Oper_Addr_Mode::BaseIndexOffset;
            //temp.scale_offset = oper.scale_offset;
            temp.scale_offset = scale_offset;
            return temp;
        }
    }*/
    /*else if (oper.addr_mode == Oper_Addr_Mode::IndexScale)
    {
        if ((opflags & (O1_MEM << opshift)) == 0 ||
            (oper_idx > 1 && o1_mem))
        {
            Operand temp = TempOperand(ctx, oper.data_type, AF_Write);
            Instruction *load = NewInstruction(ctx, MoveOp(oper.data_type), temp, oper);
            array::Insert(instructions, instr_index, load);

            temp.access_flags = oper.access_flags;
            temp.addr_mode = oper.addr_mode;
            temp.scale_offset = oper.scale_offset;
            return temp;
        }
    }*/
    return oper;
}

static Instruction* PushInstruction(Codegen_Context *ctx,
        Instruction_List &instructions,
        Amd64_Opcode opcode,
        Operand oper1 = NoneOperand(),
        Operand oper2 = NoneOperand(),
        Operand oper3 = NoneOperand())
{
    oper1 = LoadImmediates(ctx, opcode, 0, instructions, instructions.count, oper1);
    bool o1_mem = (oper1.addr_mode == Oper_Addr_Mode::BaseOffset);
    oper2 = LoadImmediates(ctx, opcode, 1, instructions, instructions.count, oper2, o1_mem);
    oper3 = LoadImmediates(ctx, opcode, 2, instructions, instructions.count, oper3, o1_mem);
    Instruction *instr = NewInstruction(ctx, opcode, oper1, oper2, oper3);
    array::Push(instructions, instr);
    return instr;
}

static Instruction* PushInstruction(Codegen_Context *ctx,
        Amd64_Opcode opcode,
        Operand oper1 = NoneOperand(),
        Operand oper2 = NoneOperand(),
        Operand oper3 = NoneOperand())
{
    return PushInstruction(ctx, ctx->current_routine->instructions,
            opcode, oper1, oper2, oper3);
}

static Instruction* InsertInstruction(Codegen_Context *ctx,
        Instruction_List &instructions,
        s64 &instr_index,
        Amd64_Opcode opcode,
        Operand oper1 = NoneOperand(),
        Operand oper2 = NoneOperand(),
        Operand oper3 = NoneOperand())
{
    oper1 = LoadImmediates(ctx, opcode, 0, instructions, instr_index, oper1);
    bool o1_mem = (oper1.addr_mode == Oper_Addr_Mode::BaseOffset);
    oper2 = LoadImmediates(ctx, opcode, 1, instructions, instr_index, oper2, o1_mem);
    oper3 = LoadImmediates(ctx, opcode, 2, instructions, instr_index, oper3, o1_mem);
    Instruction *instr = NewInstruction(ctx, opcode, oper1, oper2, oper3);
    array::Insert(instructions, instr_index, instr);
    instr_index++;
    return instr;
}

static Instruction* PushEpilogue(Codegen_Context *ctx,
        Amd64_Opcode opcode,
        Operand oper1 = NoneOperand(),
        Operand oper2 = NoneOperand(),
        Operand oper3 = NoneOperand())
{
    return PushInstruction(ctx, ctx->current_routine->epilogue,
            opcode, oper1, oper2, oper3);
}

static Instruction* PushPrologue(Codegen_Context *ctx,
        Amd64_Opcode opcode,
        Operand oper1 = NoneOperand(),
        Operand oper2 = NoneOperand(),
        Operand oper3 = NoneOperand())
{
    return PushInstruction(ctx, ctx->current_routine->prologue,
            opcode, oper1, oper2, oper3);
}

static Instruction* InsertLoad(Codegen_Context *ctx,
        Instruction_List &instructions, s64 &instr_index,
        Operand oper1, Operand oper2)
{
    ASSERT(oper1.data_type == oper2.data_type);
    return InsertInstruction(ctx, instructions, instr_index, MoveOp(oper1.data_type), oper1, oper2);
}

static Instruction* PushLoad(Codegen_Context *ctx,
        Instruction_List &instructions,
        Operand oper1, Operand oper2, Operand oper3 = NoneOperand())
{
    ASSERT(oper1.data_type == oper2.data_type);
    return PushInstruction(ctx, instructions, MoveOp(oper1.data_type), oper1, oper2, oper3);
}

static Instruction* PushLoad(Codegen_Context *ctx, Operand oper1, Operand oper2, Operand oper3 = NoneOperand())
{
    return PushLoad(ctx, ctx->current_routine->instructions, oper1, oper2, oper3);
}

static void PushLoad(Codegen_Context *ctx, Ir_Operand *ir_oper1, Ir_Operand *ir_oper2)
{
    PushLoad(ctx, IrOperand(ctx, ir_oper1, AF_Write), IrOperand(ctx, ir_oper2, AF_Read));
}

static Instruction* PushLoadAddr(Codegen_Context *ctx, Operand oper1, Operand oper2)
{
    return PushInstruction(ctx, OP_lea, oper1, oper2);
}

static Instruction* PushLoadAddr(Codegen_Context *ctx, Operand oper1, Operand oper2, Operand oper3)
{
    return PushInstruction(ctx, OP_lea, oper1, oper2, oper3);
}

static void PushZeroReg(Codegen_Context *ctx, Operand oper)
{
    oper.access_flags = AF_Write;
    PushInstruction(ctx, OP_xor, W_(oper), W_(oper));
}

static void PushLabel(Codegen_Context *ctx, Name name)
{
    Operand oper = { };
    oper.type = Oper_Type::Label;
    oper.label.name = name;
    PushInstruction(ctx, OP_LABEL, oper);
}

static void GenerateCompare(Codegen_Context *ctx,
        Ir_Instruction *ir_instr, Ir_Instruction *ir_next_instr,
        bool *skip_next)
{
    Amd64_Opcode cmp_op;
    if (ir_instr->oper1.type->tag == TYP_f32)
        cmp_op = OP_comiss;
    else if (ir_instr->oper1.type->tag == TYP_f64)
        cmp_op = OP_comisd;
    else
        cmp_op = OP_cmp;

    PushInstruction(ctx, cmp_op,
            IrOperand(ctx, &ir_instr->oper1, AF_Read),
            IrOperand(ctx, &ir_instr->oper2, AF_Read));

    Type *ltype = ir_instr->oper1.type;
    b32 is_signed = TypeIsSigned(ltype);

    if (ir_next_instr)
    {
        Amd64_Opcode op = OP_nop;
        switch (ir_next_instr->opcode)
        {
        case IR_Jnz:
            switch (ir_instr->opcode)
            {
                case IR_Eq:     op = OP_je; break;
                case IR_Neq:    op = OP_jne; break;
                case IR_Lt:     op = is_signed ? OP_jl  : OP_jb; break;
                case IR_Leq:    op = is_signed ? OP_jle : OP_jbe; break;
                case IR_Gt:     op = is_signed ? OP_jg  : OP_ja; break;
                case IR_Geq:    op = is_signed ? OP_jge : OP_jae; break;
                default:
                    break;
            }
            break;
        case IR_Jz:
            switch (ir_instr->opcode)
            {
                case IR_Eq:     op = OP_jne; break;
                case IR_Neq:    op = OP_je; break;
                case IR_Lt:     op = is_signed ? OP_jge : OP_jae; break;
                case IR_Leq:    op = is_signed ? OP_jg  : OP_ja; break;
                case IR_Gt:     op = is_signed ? OP_jle : OP_jbe; break;
                case IR_Geq:    op = is_signed ? OP_jl : OP_jb; break;
                default:
                    break;
            }
            break;
        default:
            break;
        }
        if (op != OP_nop)
        {
            Operand target = LabelOperand(&ir_next_instr->target, AF_Read);
            PushInstruction(ctx, op, target);
            *skip_next = true;
            return;
        }
    }

    Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
    ASSERT(target.data_type == Oper_Data_Type::BOOL);
    switch (ir_instr->opcode)
    {
    case IR_Eq:
        {
            PushInstruction(ctx, OP_mov, target, ImmOperand(false, AF_Read));
            Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
            PushInstruction(ctx, OP_mov, temp, ImmOperand(true, AF_Read));
            target.data_type = temp.data_type = Oper_Data_Type::U16;
            PushInstruction(ctx, OP_cmove, RW_(target), R_(temp));
            break;
        }
    case IR_Neq:
        {
            PushInstruction(ctx, OP_mov, target, ImmOperand(false, AF_Read));
            Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
            PushInstruction(ctx, OP_mov, temp, ImmOperand(true, AF_Read));
            target.data_type = temp.data_type = Oper_Data_Type::U16;
            PushInstruction(ctx, OP_cmovne, RW_(target), R_(temp));
            break;
        }
    case IR_Lt:
        {
            PushInstruction(ctx, OP_mov, target, ImmOperand(false, AF_Read));
            Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
            PushInstruction(ctx, OP_mov, temp, ImmOperand(true, AF_Read));
            Amd64_Opcode mov_op = is_signed ? OP_cmovl : OP_cmovb;
            target.data_type = temp.data_type = Oper_Data_Type::U16;
            PushInstruction(ctx, mov_op, RW_(target), R_(temp));
            break;
        }
    case IR_Leq:
        {
            PushInstruction(ctx, OP_mov, target, ImmOperand(false, AF_Read));
            Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
            PushInstruction(ctx, OP_mov, temp, ImmOperand(true, AF_Read));
            Amd64_Opcode mov_op = is_signed ? OP_cmovle : OP_cmovbe;
            target.data_type = temp.data_type = Oper_Data_Type::U16;
            PushInstruction(ctx, mov_op, RW_(target), R_(temp));
            break;
        }
    case IR_Gt:
        {
            // cmovg is not valid opcode for 64 bit registers/memory operands
            // => reverse the result of cmovle.
            if (is_signed)
            {
                PushInstruction(ctx, OP_mov, target, ImmOperand(true, AF_Read));
                Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
                PushInstruction(ctx, OP_mov, temp, ImmOperand(false, AF_Read));
                target.data_type = temp.data_type = Oper_Data_Type::U16;
                PushInstruction(ctx, OP_cmovle, RW_(target), R_(temp));
            }
            else
            {
                PushInstruction(ctx, OP_mov, target, ImmOperand(false, AF_Read));
                Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
                PushInstruction(ctx, OP_mov, temp, ImmOperand(true, AF_Read));
                target.data_type = temp.data_type = Oper_Data_Type::U16;
                PushInstruction(ctx, OP_cmova, RW_(target), R_(temp));
            }
            break;
        }
    case IR_Geq:
        {
            PushInstruction(ctx, OP_mov, target, ImmOperand(false, AF_Read));
            Operand temp = TempOperand(ctx, Oper_Data_Type::BOOL, AF_Write);
            PushInstruction(ctx, OP_mov, temp, ImmOperand(true, AF_Read));
            Amd64_Opcode mov_op = is_signed ? OP_cmovge : OP_cmovae;
            target.data_type = temp.data_type = Oper_Data_Type::U16;
            PushInstruction(ctx, mov_op, RW_(target), R_(temp));
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
                        IrOperand(ctx, &ir_instr->target, AF_ReadWrite),
                        IrOperand(ctx, &ir_instr->oper2, AF_Read));
            }
            else
            {
                PushInstruction(ctx, OP_add,
                        IrOperand(ctx, &ir_instr->target, AF_ReadWrite),
                        IrOperand(ctx, &ir_instr->oper2, AF_Read));
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
                        IrOperand(ctx, &ir_instr->target, AF_ReadWrite),
                        IrOperand(ctx, &ir_instr->oper2, AF_Read));
            }
            else
            {
                PushInstruction(ctx, OP_sub,
                        IrOperand(ctx, &ir_instr->target, AF_ReadWrite),
                        IrOperand(ctx, &ir_instr->oper2, AF_Read));
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
                        IrOperand(ctx, &ir_instr->target, AF_ReadWrite),
                        IrOperand(ctx, &ir_instr->oper2, AF_Read));
            }
            else if (is_signed)
            {
                if (ir_instr->target != ir_instr->oper1)
                    PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
                Operand oper2 = IrOperand(ctx, &ir_instr->oper2, AF_Read);
                Operand temp = TempOperand(ctx, oper2.data_type, AF_Write);
                PushLoad(ctx, temp, oper2);
                PushInstruction(ctx, OP_imul,
                        IrOperand(ctx, &ir_instr->target, AF_ReadWrite),
                        R_(temp));
            }
            else // unsigned
            {
                Operand oper1 = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                Operand oper2 = IrOperand(ctx, &ir_instr->oper2, AF_Read);
                Operand rax = FixedRegOperand(ctx, REG_rax, oper1.data_type, AF_Read);
                Operand rdx = FixedRegOperand(ctx, REG_rdx, oper1.data_type, AF_Read);
                Operand temp = TempOperand(ctx, oper2.data_type, AF_Write);
                PushLoad(ctx, W_(rax), oper1);
                PushZeroReg(ctx, rdx);
                PushLoad(ctx, temp, oper2);
                PushInstruction(ctx, OP_mul, R_(temp), S_(RW_(rax)), S_(RW_(rdx)));
                PushLoad(ctx, IrOperand(ctx, &ir_instr->target, AF_Write), R_(rax), S_(R_(rdx)));
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
                        IrOperand(ctx, &ir_instr->target, AF_ReadWrite),
                        IrOperand(ctx, &ir_instr->oper2, AF_Read));
            }
            else
            {
                Amd64_Opcode div_op = (is_signed) ? OP_idiv : OP_div;
                Operand oper1 = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                Operand oper2 = IrOperand(ctx, &ir_instr->oper2, AF_Read);
                Operand rax = FixedRegOperand(ctx, REG_rax, oper1.data_type, AF_Read);
                Operand rdx = FixedRegOperand(ctx, REG_rdx, oper1.data_type, AF_Read);
                Operand temp = TempOperand(ctx, oper2.data_type, AF_Write);
                PushLoad(ctx, W_(rax), oper1);
                if (is_signed)
                    PushInstruction(ctx, OP_cqo, S_(W_(rdx))); // Sign extend rax to rdx:rax
                else
                    PushZeroReg(ctx, rdx);
                PushLoad(ctx, temp, oper2);
                PushInstruction(ctx, div_op, R_(temp), S_(RW_(rax)), S_(RW_(rdx)));
                if (ir_instr->target == ir_instr->oper1)
                    PushLoad(ctx, RW_(oper1), R_(rax), S_(R_(rdx)));
                else
                    PushLoad(ctx, IrOperand(ctx, &ir_instr->target, AF_Write), R_(rax), S_(R_(rdx)));
            }
        } break;
    case IR_Mod:
        {
            ASSERT(!is_float);
            Amd64_Opcode div_op = (is_signed) ? OP_idiv : OP_div;
            Operand oper1 = IrOperand(ctx, &ir_instr->oper1, AF_Read);
            Operand oper2 = IrOperand(ctx, &ir_instr->oper2, AF_Read);
            Operand rax = FixedRegOperand(ctx, REG_rax, oper1.data_type, AF_Read);
            Operand rdx = FixedRegOperand(ctx, REG_rdx, oper1.data_type, AF_Read);
            Operand temp = TempOperand(ctx, oper2.data_type, AF_Write);
            PushLoad(ctx, W_(rax), oper1);
            if (is_signed)
                PushInstruction(ctx, OP_cqo, S_(W_(rdx))); // Sign extend rax to rdx:rax
            else
                PushZeroReg(ctx, rdx);
            PushLoad(ctx, temp, oper2);
            PushInstruction(ctx, div_op, R_(temp), S_(RW_(rax)), S_(RW_(rdx)));
            if (ir_instr->target == ir_instr->oper1)
                PushLoad(ctx, RW_(oper1), R_(rdx));
            else
                PushLoad(ctx, IrOperand(ctx, &ir_instr->target, AF_Write), R_(rdx));
        } break;

    case IR_LShift:
        {
            ASSERT(!is_float);
            Amd64_Opcode shift_op = (is_signed) ? OP_sal : OP_shl;
            if (ir_instr->target != ir_instr->oper1)
                PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
            Operand oper2 = IrOperand(ctx, &ir_instr->oper2, AF_Read);
            Operand rcx = FixedRegOperand(ctx, REG_rcx, oper2.data_type, AF_Read);
            PushLoad(ctx, W_(rcx), oper2);
            rcx.data_type = Oper_Data_Type::U8;
            PushInstruction(ctx, shift_op,
                    IrOperand(ctx, &ir_instr->target, AF_ReadWrite),
                    R_(rcx));
        } break;
    case IR_RShift:
        {
            ASSERT(!is_float);
            Amd64_Opcode shift_op = (is_signed) ? OP_sar : OP_shr;
            if (ir_instr->target != ir_instr->oper1)
                PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
            Operand oper2 = IrOperand(ctx, &ir_instr->oper2, AF_Read);
            Operand rcx = FixedRegOperand(ctx, REG_rcx, oper2.data_type, AF_Read);
            PushLoad(ctx, W_(rcx), oper2);
            rcx.data_type = Oper_Data_Type::U8;
            PushInstruction(ctx, shift_op,
                    IrOperand(ctx, &ir_instr->target, AF_ReadWrite),
                    R_(rcx));
        } break;

    case IR_Sqrt:
        {
            Amd64_Opcode sqrt_op = (ltype->tag == TYP_f32) ? OP_sqrtss : OP_sqrtsd;
            PushInstruction(ctx, sqrt_op,
                    IrOperand(ctx, &ir_instr->target, AF_Write),
                    IrOperand(ctx, &ir_instr->oper1, AF_Read));
        } break;

    case IR_And:
        PushInstruction(ctx, OP_and,
                IrOperand(ctx, &ir_instr->oper1, AF_ReadWrite),
                IrOperand(ctx, &ir_instr->oper2, AF_Read));
        break;
    case IR_Or:
        PushInstruction(ctx, OP_or,
                IrOperand(ctx, &ir_instr->oper1, AF_ReadWrite),
                IrOperand(ctx, &ir_instr->oper2, AF_Read));
        break;
    case IR_Xor:
        PushInstruction(ctx, OP_xor,
                IrOperand(ctx, &ir_instr->oper1, AF_ReadWrite),
                IrOperand(ctx, &ir_instr->oper2, AF_Read));
        break;

    case IR_Neg:
        if (is_float)
        {
            if (ltype->tag == TYP_f32)
            {
                PushLoad(ctx, IrOperand(ctx, &ir_instr->target, AF_Write), ImmOperand(0.0f, AF_Read));
                PushInstruction(ctx, OP_subss,
                        IrOperand(ctx, &ir_instr->target, AF_ReadWrite),
                        IrOperand(ctx, &ir_instr->oper1, AF_Read));
            }
            else if (ltype->tag == TYP_f64)
            {
                PushLoad(ctx, IrOperand(ctx, &ir_instr->target, AF_Write), ImmOperand(0.0, AF_Read));
                PushInstruction(ctx, OP_subsd,
                        IrOperand(ctx, &ir_instr->target, AF_ReadWrite),
                        IrOperand(ctx, &ir_instr->oper1, AF_Read));
            }
        }
        else
        {
            PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
            PushInstruction(ctx, OP_neg, IrOperand(ctx, &ir_instr->target, AF_ReadWrite));
        }
        break;
    case IR_Not:
        {
            PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
            Operand target = IrOperand(ctx, &ir_instr->target, AF_ReadWrite);
            PushInstruction(ctx, OP_not, target);
            PushInstruction(ctx, OP_and, target, ImmOperand(1, AF_Read));
        } break;
    case IR_Compl:
        PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
        PushInstruction(ctx, OP_not, IrOperand(ctx, &ir_instr->target, AF_ReadWrite));
        break;
    }
}

static u32 GetSize(Oper_Data_Type data_type)
{
    switch (data_type)
    {
        case Oper_Data_Type::PTR:
            return 8;
        case Oper_Data_Type::BOOL:
        case Oper_Data_Type::U8:
        case Oper_Data_Type::S8:
            return 1;
        case Oper_Data_Type::U16:
        case Oper_Data_Type::S16:
            return 2;
        case Oper_Data_Type::U32:
        case Oper_Data_Type::S32:
            return 4;
        case Oper_Data_Type::U64:
        case Oper_Data_Type::S64:
            return 8;
        case Oper_Data_Type::F32:
            return 4;
        case Oper_Data_Type::F64:
            return 8;
    }
    INVALID_CODE_PATH;
    return 0;
}

static u32 GetAlign(Oper_Data_Type data_type)
{
    switch (data_type)
    {
        case Oper_Data_Type::PTR:
            return 8;
        case Oper_Data_Type::BOOL:
        case Oper_Data_Type::U8:
        case Oper_Data_Type::S8:
            return 1;
        case Oper_Data_Type::U16:
        case Oper_Data_Type::S16:
            return 2;
        case Oper_Data_Type::U32:
        case Oper_Data_Type::S32:
            return 4;
        case Oper_Data_Type::U64:
        case Oper_Data_Type::S64:
            return 8;
        case Oper_Data_Type::F32:
            return 4;
        case Oper_Data_Type::F64:
            return 8;
    }
    INVALID_CODE_PATH;
    return 0;
}

static void SetArgLocalOffset(Codegen_Context *ctx, Name name, s64 offset)
{
    Routine *routine = ctx->current_routine;
    ASSERT(!hashtable::Lookup(routine->local_offsets, name));

    Local_Offset *offs = PushStruct<Local_Offset>(&ctx->arena);
    offs->name = name;
    offs->offset = offset;

    hashtable::Put(routine->local_offsets, name, offs);
}

static s64 GetLocalOffset(Codegen_Context *ctx, Name name, Oper_Data_Type data_type)
{
    Routine *routine = ctx->current_routine;
    Local_Offset *offs = hashtable::Lookup(routine->local_offsets, name);
    if (offs) return offs->offset;

    offs = PushStruct<Local_Offset>(&ctx->arena);

    routine->locals_size += GetSize(data_type);
    routine->locals_size = Align(routine->locals_size, GetAlign(data_type));

    offs->name = name;
    offs->offset = -routine->locals_size;

    hashtable::Put(routine->local_offsets, name, offs);
    return offs->offset;
}

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

static Operand GetAddress(Codegen_Context *ctx, Ir_Operand *ir_oper)
{
    // TODO(henrik): Differentiate between IR immediate and IR string constant.
    //ASSERT(ir_oper->oper_type != IR_OPER_Immediate);
    if (ir_oper->oper_type == IR_OPER_Immediate)
    {
        if (TypeIsString(ir_oper->type))
            return IrOperand(ctx, ir_oper, AF_ReadWrite);
        INVALID_CODE_PATH;
    }
    else if (ir_oper->oper_type == IR_OPER_GlobalVariable)
    {
        return IrOperand(ctx, ir_oper, AF_ReadWrite);
    }
    Name name;
    if (ir_oper->oper_type == IR_OPER_Variable)
        name = ir_oper->var.name;
    else if (ir_oper->oper_type == IR_OPER_Temp)
        name = ir_oper->var.name;
    else
        INVALID_CODE_PATH;
    Oper_Data_Type data_type = DataTypeFromType(ir_oper->type);
    s64 local_offs = GetLocalOffset(ctx, name, data_type);
    return BaseOffsetOperand(REG_rbp, local_offs, data_type, AF_ReadWrite);
}

static void PushOperandUse(Codegen_Context *ctx, Operand_Use **use, const Operand &oper)
{
    Operand_Use *oper_use = PushStruct<Operand_Use>(&ctx->arena);
    oper_use->oper = oper;
    oper_use->next = nullptr;
    (*use)->next = oper_use;
    *use = oper_use;
}

static s64 PushArgs(Codegen_Context *ctx,
        Ir_Routine *ir_routine, Ir_Instruction *ir_instr, Operand_Use **uses)
{
    Reg_Seq_Index arg_reg_index = { };

    // NOTE(henrik): The allocated stack space is added to alloc_stack_instr
    // instruction later.
    Instruction *alloc_stack_instr = PushInstruction(ctx, OP_sub,
            RegOperand(REG_rsp, Oper_Data_Type::U64, AF_Write));

    ASSERT(ir_instr->oper2.oper_type == IR_OPER_Immediate);
    s64 arg_instr_idx = ir_instr->oper2.imm_s64;

    Operand_Use use_head = { };
    Operand_Use *use = &use_head;
    while (arg_instr_idx != -1)
    {
        Ir_Instruction *arg_instr = &ir_routine->instructions[arg_instr_idx];
        ASSERT(arg_instr->opcode == IR_Arg);

        ctx->comment = &arg_instr->comment;

        Type *arg_type = arg_instr->target.type;
        Oper_Data_Type arg_data_type = DataTypeFromType(arg_type);
        const Reg *arg_reg = GetArgRegister(ctx->reg_alloc, arg_data_type, &arg_reg_index);
        s64 arg_sp_offset = GetOffsetFromStackPointer(ctx->reg_alloc, arg_reg_index);
        if (TypeIsStruct(arg_type))
        {
            if (arg_reg)
            {
                Operand arg_target = FixedRegOperand(ctx, *arg_reg, Oper_Data_Type::PTR, AF_Write);
                PushLoadAddr(ctx,
                        arg_target,
                        R_(GetAddress(ctx, &arg_instr->target)));
                PushOperandUse(ctx, &use, arg_target);
            }
            else
            {
                Operand temp = TempOperand(ctx, Oper_Data_Type::PTR, AF_Write);
                PushLoadAddr(ctx, W_(temp), R_(GetAddress(ctx, &arg_instr->target)));
                PushLoad(ctx,
                        BaseOffsetOperand(REG_rsp, arg_sp_offset, temp.data_type, AF_Write),
                        R_(temp));
                PushOperandUse(ctx, &use, IrOperand(ctx, &arg_instr->target, AF_Read));
            }
        }
        else
        {
            if (arg_reg)
            {
                Operand arg_target = FixedRegOperand(ctx, *arg_reg, arg_data_type, AF_Write);
                Operand arg_oper = IrOperand(ctx, &arg_instr->target, AF_Read);
                PushLoad(ctx, arg_target, arg_oper);
                PushOperandUse(ctx, &use, arg_target);
            }
            else
            {
                Operand arg_oper = IrOperand(ctx, &arg_instr->target, AF_Read);
                PushLoad(ctx,
                        BaseOffsetOperand(REG_rsp, arg_sp_offset, arg_oper.data_type, AF_Write),
                        arg_oper);
            }
        }

        ASSERT(arg_instr->oper1.oper_type == IR_OPER_Immediate);
        arg_instr_idx = arg_instr->oper1.imm_s64;
    }

    *uses = use_head.next;

    s64 arg_stack_alloc = GetArgStackAllocSize(ctx->reg_alloc, arg_reg_index);
    alloc_stack_instr->oper2 = ImmOperand(arg_stack_alloc, AF_Read);
    return arg_stack_alloc;
}

static void AddLocal(Codegen_Context *ctx, Ir_Operand *ir_oper)
{
    if (TypeIsStruct(ir_oper->type))
    {
        Routine *routine = ctx->current_routine;
        Name name = ir_oper->var.name;
        ASSERT(!hashtable::Lookup(routine->local_offsets, name));

        Local_Offset *offs = PushStruct<Local_Offset>(&ctx->arena);

        routine->locals_size += GetAlignedSize(ir_oper->type);
        routine->locals_size = Align(routine->locals_size, 8);
        offs->name = name;
        offs->offset = -routine->locals_size;

        hashtable::Put(routine->local_offsets, name, offs);

        PushLoadAddr(ctx,
                IrOperand(ctx, ir_oper, AF_Write),
                BaseOffsetOperand(REG_rbp, offs->offset, Oper_Data_Type::PTR, AF_Read));
    }
}

static void Copy(Codegen_Context *ctx,
        Operand target, Operand source, Type *type, s64 offset = 0)
{
    if (TypeIsStruct(type))
    {
        for (s64 i = 0; i < type->struct_type.member_count; i++)
        {
            Struct_Member *member = &type->struct_type.members[i];
            if (TypeIsStruct(member->type))
            {
                Copy(ctx, target, source, member->type, member->offset + offset);
            }
            else
            {
                Operand temp = TempOperand(ctx, DataTypeFromType(member->type), AF_Write);
                Operand src = BaseOffsetOperand(source, member->offset + offset, AF_Read);
                Operand dst = BaseOffsetOperand(target, member->offset + offset, AF_Write);
                src.data_type = temp.data_type;
                dst.data_type = temp.data_type;
                PushLoad(ctx, W_(temp), R_(src));
                PushLoad(ctx, W_(dst), R_(temp));
            }
        }
    }
    else
    {
        PushLoad(ctx, W_(target), R_(source));
    }
}

static void GenerateCode(Codegen_Context *ctx, Ir_Routine *routine,
        Ir_Instruction *ir_instr, Ir_Instruction *ir_next_instr,
        bool *skip_next)
{
    switch (ir_instr->opcode)
    {
        case IR_COUNT:
            INVALID_CODE_PATH;
            break;

        case IR_Label:
            PushLabel(ctx, ir_instr->target.label->name);
            break;

        case IR_VarDecl:
            AddLocal(ctx, &ir_instr->target);
            break;

        case IR_Add:
        case IR_Sub:
        case IR_Mul:
        case IR_Div:
        case IR_Mod:
        case IR_LShift:
        case IR_RShift:
        case IR_And:
        case IR_Or:
        case IR_Xor:
        case IR_Not:
        case IR_Neg:
        case IR_Compl:
        case IR_Sqrt:
            GenerateArithmetic(ctx, ir_instr);
            break;

        case IR_Eq:
        case IR_Neq:
        case IR_Lt:
        case IR_Leq:
        case IR_Gt:
        case IR_Geq:
            GenerateCompare(ctx, ir_instr, ir_next_instr, skip_next);
            break;

        case IR_Deref:
            {
                Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
                PushLoad(ctx,
                        target,
                        BaseOffsetOperand(ctx, &ir_instr->oper1, 0, target.data_type, AF_Read));
            } break;

        case IR_Addr:
            {
                if (TypeIsStruct(ir_instr->oper1.type))
                {
                    PushLoad(ctx, &ir_instr->target, &ir_instr->oper1);
                    break;
                }

                Operand addr_oper = GetAddress(ctx, &ir_instr->oper1);
                Operand oper = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                //PushLoad(ctx, W_(addr_oper), IrOperand(ctx, &ir_instr->oper1, AF_Read));
                PushInstruction(ctx, OP_SPILL, oper);
                //PushSpill(ctx, oper);
                PushLoadAddr(ctx,
                        IrOperand(ctx, &ir_instr->target, AF_Write),
                        R_(addr_oper));
            } break;

        case IR_Mov:
            {
                if (TypeIsStruct(ir_instr->target.type))
                {
                    Type *type = ir_instr->target.type;
                    if (ir_instr->oper1.oper_type == IR_OPER_Immediate)
                    {
                        Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
                        Operand source_addr = TempOperand(ctx, Oper_Data_Type::PTR, AF_Write);
                        PushLoadAddr(ctx, target, R_(GetAddress(ctx, &ir_instr->target)));
                        PushLoadAddr(ctx, source_addr, R_(GetAddress(ctx, &ir_instr->oper1)));
                        Copy(ctx, target, source_addr, type);
                    }
                    else
                    {
                    Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
                    Operand source = IrOperand(ctx, &ir_instr->oper1, AF_Write);
                    PushLoadAddr(ctx, target, R_(GetAddress(ctx, &ir_instr->target)));
                    //Operand source_addr = TempOperand(ctx, Oper_Data_Type::PTR, AF_Write);
                    //PushLoadAddr(ctx, source_addr, R_(GetAddress(ctx, &ir_instr->oper1)));
                    //PushLoad(ctx, source_addr, IrOperand(ctx, &ir_instr->oper1, AF_Read));
                    Copy(ctx, target, source, type);
                    }
                }
                else
                {
                    Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
                    Operand oper1 = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                    if ( 0 && oper1.type == Oper_Type::Immediate)
                    {
                        oper1.data_type = target.data_type;
                        Operand temp = TempOperand(ctx, oper1.data_type, AF_Write);
                        PushLoad(ctx, temp, oper1);
                        PushLoad(ctx, target, R_(temp));
                    }
                    else
                    {
                        // TODO(henrik): remove this data_type "coercion"
                        oper1.data_type = target.data_type;
                        PushLoad(ctx, target, oper1);
                    }
                }
            } break;
        case IR_MovSX:
            {
                Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
                Operand oper1 = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                if (ir_instr->oper1.oper_type == IR_OPER_Immediate)
                {
                    Operand temp = TempOperand(ctx, oper1.data_type, AF_Write);
                    PushLoad(ctx, temp, oper1);
                    PushInstruction(ctx, OP_movsx, target, R_(temp));
                }
                else
                {
                    PushInstruction(ctx, OP_movsx, target, oper1);
                }
            } break;
        case IR_MovZX:
            {
                Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
                Operand oper1 = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                if (ir_instr->oper1.oper_type == IR_OPER_Immediate)
                {
                    Operand temp = TempOperand(ctx, oper1.data_type, AF_Write);
                    if (GetSize(oper1.data_type) == 4)
                    {
                        PushZeroReg(ctx, temp);
                        PushLoad(ctx, temp, oper1);
                        temp.data_type = target.data_type;
                        PushInstruction(ctx, OP_mov, target, R_(temp));
                    }
                    else
                    {
                        PushLoad(ctx, temp, oper1);
                        PushInstruction(ctx, OP_movzx, target, R_(temp));
                    }
                }
                else
                {
                    if (GetSize(oper1.data_type) == 4)
                    {
                        Operand temp = TempOperand(ctx, oper1.data_type, AF_Write);
                        PushZeroReg(ctx, temp);
                        PushLoad(ctx, temp, oper1);
                        temp.data_type = target.data_type;
                        PushInstruction(ctx, OP_mov, target, R_(temp));
                    }
                    else
                    {
                        PushInstruction(ctx, OP_movzx, target, oper1);
                    }
                }
            } break;
        case IR_Load:
            {
                //Operand oper = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                //PushInstruction(ctx, OP_SPILL, oper);

                Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
                //Operand source = R_(GetAddress(ctx, &ir_instr->oper1));
                Operand source = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                source.data_type = target.data_type;
                PushLoad(ctx, target, BaseOffsetOperand(source, 0, AF_Read));
                //PushLoad(ctx, target, source);
            } break;
        case IR_Store:
            {
                Operand target = BaseOffsetOperand(ctx, &ir_instr->target, 0,
                        DataTypeFromType(ir_instr->target.type), AF_Write);
                Operand oper1 = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                target.data_type = oper1.data_type;
                PushLoad(ctx, target, oper1);
            } break;
        case IR_MovMember:
            {
                Type *oper_type = ir_instr->oper1.type;
                s64 member_index = ir_instr->oper2.imm_s64;
                Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
                if (TypeIsPointer(oper_type))
                {
                    ASSERT(TypeIsStruct(oper_type->base_type));
                    s64 member_offset = GetStructMemberOffset(oper_type->base_type, member_index);
                    Operand oper1 = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                    oper1.data_type = target.data_type;
                    PushLoad(ctx,
                            target,
                            BaseOffsetOperand(oper1, member_offset, AF_Read));
                            //BaseOffsetOperand(ctx, &ir_instr->oper1, member_offset, target.data_type, AF_Read));
                }
                else
                {
                    ASSERT(TypeIsStruct(oper_type));
                    s64 member_offset = GetStructMemberOffset(oper_type, member_index);
                    Operand oper1 = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                    oper1.data_type = target.data_type;
                    PushLoad(ctx,
                            target,
                            BaseOffsetOperand(oper1, member_offset, AF_Read));
                    //s64 member_offset = GetStructMemberOffset(oper_type, member_index);
                    //Operand source = GetAddress(ctx, &ir_instr->oper1);
                    //source.scale_offset += member_offset;
                    //source.data_type = target.data_type;
                    //PushLoad(ctx, target, R_(source));
                }
            } break;
        case IR_LoadMemberAddr:
            {
                Type *oper_type = ir_instr->oper1.type;
                s64 member_index = ir_instr->oper2.imm_s64;
                Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
                ASSERT(target.data_type == Oper_Data_Type::PTR);
                if (TypeIsPointer(oper_type))
                {
                    s64 member_offset = GetStructMemberOffset(oper_type->base_type, member_index);
                    //Operand base = TempOperand(ctx, Oper_Data_Type::PTR, AF_Write);
                    //Operand oper1 = GetAddress(ctx, &ir_instr->oper1);
                    //PushLoad(ctx, base, R_(oper1));
                    //PushLoadAddr(ctx, target, BaseOffsetOperand(base, member_offset, AF_Read));
                    Operand oper1 = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                    PushLoadAddr(ctx, target, BaseOffsetOperand(oper1, member_offset, AF_Read));
                }
                else
                {
                    s64 member_offset = GetStructMemberOffset(oper_type, member_index);
                    //Operand temp = TempOperand(ctx, target.data_type, AF_Write);
                    //PushLoadAddr(ctx, temp, R_(GetAddress(ctx, &ir_instr->oper1)));
                    //PushLoadAddr(ctx, target, BaseOffsetOperand(temp, member_offset, AF_Read));
                    Operand oper1 = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                    PushLoadAddr(ctx, target, BaseOffsetOperand(oper1, member_offset, AF_Read));
                }
            } break;
        case IR_MovElement:
            {
                // target <- [base + index*size]
                Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
                s64 size = GetAlignedElementSize(ir_instr->oper1.type);
                // NOTE(henrik): If the size is valid as index scale, we will emit
                // only one instruction.
                if (size == 1 || size == 2 || size == 4 || size == 8)
                {
                    PushLoad(ctx, target,
                            BaseIndexOffsetOperand(ctx, &ir_instr->oper1, 0, target.data_type, AF_Read),
                            IndexScaleOperand(ctx, &ir_instr->oper2, size, AF_Read));
                }
                else
                {
                    Operand index = TempOperand(ctx, Oper_Data_Type::S64, AF_Write);
                    Operand idx = IrOperand(ctx, &ir_instr->oper2, AF_Read);
                    idx.data_type = index.data_type;
                    PushLoad(ctx, index, idx);
                    PushInstruction(ctx, OP_imul, RW_(index), ImmOperand(size, AF_Read));

                    Operand base = TempOperand(ctx, Oper_Data_Type::PTR, AF_Write);
                    //Operand oper1 = GetAddress(ctx, &ir_instr->oper1);
                    Operand oper1 = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                    PushLoad(ctx, base, R_(oper1), S_(IrOperand(ctx, &ir_instr->oper1, AF_Read)));
                    PushLoad(ctx, target,
                            BaseIndexOffsetOperand(base, 0, AF_Read),
                            //BaseIndexOffsetOperand(ctx, &ir_instr->oper1, 0, target.data_type, AF_Read),
                            IndexScaleOperand(index, 1, AF_Read));
                }
            } break;
        case IR_LoadElementAddr:
            {
                Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
                s64 size = GetAlignedElementSize(ir_instr->oper1.type);
                ASSERT(target.data_type == Oper_Data_Type::PTR);
                // NOTE(henrik): If the size is valid as index scale, we will emit
                // only one instruction.
                if (size == 1 || size == 2 || size == 4 || size == 8)
                {
                    PushLoadAddr(ctx, target,
                            BaseIndexOffsetOperand(ctx, &ir_instr->oper1, 0, target.data_type, AF_Read),
                            IndexScaleOperand(ctx, &ir_instr->oper2, size, AF_Read));
                }
                else
                {
                    Operand index = TempOperand(ctx, Oper_Data_Type::S64, AF_Write);
                    Operand idx = IrOperand(ctx, &ir_instr->oper2, AF_Read);
                    idx.data_type = index.data_type;
                    PushLoad(ctx, index, idx);
                    PushInstruction(ctx, OP_imul, RW_(index), ImmOperand(size, AF_Read));

                    Operand base = TempOperand(ctx, Oper_Data_Type::PTR, AF_Write);
                    //Operand oper1 = GetAddress(ctx, &ir_instr->oper1);
                    Operand oper1 = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                    PushLoad(ctx, base, R_(oper1), S_(IrOperand(ctx, &ir_instr->oper1, AF_Read)));
                    PushLoadAddr(ctx, target,
                            BaseIndexOffsetOperand(base, 0, AF_Read),
                            IndexScaleOperand(index, 1, AF_Read));
                }
            } break;

        case IR_Arg:
            ctx->current_arg_count++;
            break;

        case IR_Call:
        case IR_CallForeign:
            {
                Operand_Use *uses = nullptr;
                s64 arg_stack_alloc = PushArgs(ctx, routine, ir_instr, &uses);
                Instruction *call = PushInstruction(ctx,
                        OP_call, IrOperand(ctx, &ir_instr->oper1, AF_Read));
                call->uses = uses;
                PushInstruction(ctx, OP_add,
                        RegOperand(REG_rsp, Oper_Data_Type::U64, AF_Write),
                        ImmOperand(arg_stack_alloc, AF_Read));
                if (ir_instr->target.oper_type != IR_OPER_None)
                {
                    Oper_Data_Type data_type = DataTypeFromType(ir_instr->target.type);
                    const Reg *ret_reg = GetReturnRegister(ctx->reg_alloc, data_type, 0);
                    Operand ret_oper = FixedRegOperand(ctx, *ret_reg, data_type, AF_Write);
                    call->oper2 = S_(ret_oper);
                    Instruction *load_rval = PushLoad(ctx,
                        IrOperand(ctx, &ir_instr->target, AF_Write),
                        R_(ret_oper));
                    load_rval->uses = uses;
                }
                ctx->current_arg_count = 0;
            } break;

        case IR_Jump:
            PushInstruction(ctx, OP_jmp, LabelOperand(&ir_instr->target, AF_Read));
            break;
        case IR_Jz:
            PushInstruction(ctx, OP_cmp,
                    IrOperand(ctx, &ir_instr->oper1, AF_Read),
                    ImmOperand((s64)0, AF_Read));
            PushInstruction(ctx, OP_je, LabelOperand(&ir_instr->target, AF_Read));
            break;
        case IR_Jnz:
            PushInstruction(ctx, OP_cmp,
                    IrOperand(ctx, &ir_instr->oper1, AF_Read),
                    ImmOperand((s64)0, AF_Read));
            PushInstruction(ctx, OP_jne, LabelOperand(&ir_instr->target, AF_Read));
            break;
        case IR_Return:
            if (ir_instr->target.oper_type != IR_OPER_None)
            {
                Oper_Data_Type data_type = DataTypeFromType(ir_instr->target.type);
                const Reg *ret_reg = GetReturnRegister(ctx->reg_alloc, data_type, 0);
                PushLoad(ctx,
                    RegOperand(*ret_reg, data_type, AF_Write),
                    IrOperand(ctx, &ir_instr->target, AF_Read));
            }
            PushInstruction(ctx, OP_jmp, LabelOperand(ctx->return_label_name, AF_Read));
            break;

        case IR_S_TO_F32:
            {
                Operand source = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                if (GetSize(source.data_type) < 4)
                {
                    source.data_type = Oper_Data_Type::S32;
                }
                PushInstruction(ctx, OP_cvtsi2ss,
                        IrOperand(ctx, &ir_instr->target, AF_Write),
                        source);
            }
            break;
        case IR_S_TO_F64:
            {
                Operand source = IrOperand(ctx, &ir_instr->oper1, AF_Read);
                if (GetSize(source.data_type) < 4)
                {
                    source.data_type = Oper_Data_Type::S32;
                }
                PushInstruction(ctx, OP_cvtsi2sd,
                        IrOperand(ctx, &ir_instr->target, AF_Write),
                        source);
            }
            break;
        case IR_F32_TO_S:
            {
                Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
                if (GetSize(target.data_type) < 4)
                {
                    target.data_type = Oper_Data_Type::S32;
                }
                PushInstruction(ctx, OP_cvtss2si,
                        target,
                        IrOperand(ctx, &ir_instr->oper1, AF_Read));
            }
            break;
        case IR_F64_TO_S:
            {
                Operand target = IrOperand(ctx, &ir_instr->target, AF_Write);
                if (GetSize(target.data_type) < 4)
                {
                    target.data_type = Oper_Data_Type::S32;
                }
                PushInstruction(ctx, OP_cvtsd2si,
                        target,
                        IrOperand(ctx, &ir_instr->oper1, AF_Read));
            }
            break;
        case IR_F32_TO_F64:
            PushInstruction(ctx, OP_cvtss2sd,
                    IrOperand(ctx, &ir_instr->target, AF_Write),
                    IrOperand(ctx, &ir_instr->oper1, AF_Read));
            break;
        case IR_F64_TO_F32:
            PushInstruction(ctx, OP_cvtsd2ss,
                    IrOperand(ctx, &ir_instr->target, AF_Write),
                    IrOperand(ctx, &ir_instr->oper1, AF_Read));
            break;
    }
}


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

struct Name_Data_Type
{
    Name name;
    Reg fixed_reg;
    Oper_Data_Type data_type;
    b32 spilled;
    b32 arg;
};

struct Live_Sets
{
    Array<Name_Data_Type> live_in;
    Array<Name_Data_Type> live_out;
};

struct Cfg_Edge
{
    s64 instr_index;
    s64 branch_instr_index;
    b32 falls_through;
};


b32 SetAddSpilled(Array<Name_Data_Type> &set, Name name, Oper_Data_Type data_type)
{
    for (s64 i = 0; i < set.count; i++)
    {
        if (set[i].name == name) return false;
    }
    Name_Data_Type nd = { };
    nd.name = name;
    nd.fixed_reg = { };
    nd.data_type = data_type;
    nd.spilled = true;
    array::Push(set, nd);
    return true;
}

b32 SetAddArg(Array<Name_Data_Type> &set, Name name, Oper_Data_Type data_type, Reg fixed_reg = { })
{
    for (s64 i = 0; i < set.count; i++)
    {
        if (set[i].name == name) return false;
    }
    Name_Data_Type nd = { };
    nd.name = name;
    nd.fixed_reg = fixed_reg;
    nd.data_type = data_type;
    nd.arg = true;
    array::Push(set, nd);
    return true;
}

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
        case Oper_Type::FixedRegister:      name = oper.fixed_reg.name; break;
        case Oper_Type::VirtualRegister:    name = oper.virtual_reg.name; break;
    }
    return name;
}

static Name GetOperName(Operand oper, Reg *fixed_reg)
{
    Name name = { };
    *fixed_reg = { };
    switch (oper.type)
    {
        default: break;
        case Oper_Type::FixedRegister:
            name = oper.fixed_reg.name;
            *fixed_reg = oper.fixed_reg.reg;
            break;
        case Oper_Type::VirtualRegister:
            name = oper.virtual_reg.name;
            break;
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
        {
            Oper_Data_Type data_type = oper.data_type;
            if (oper.addr_mode != Oper_Addr_Mode::Direct)
                data_type = Oper_Data_Type::PTR;
            return SetAdd(set, name, data_type, fixed_reg);
        }
    }
    return false;
}

static void PrintInstruction(IoFile *file, const Instruction *instr);

static void ComputeLiveness(Codegen_Context *ctx,
        Ir_Routine *ir_routine, Routine *routine, 
        Array<Live_Interval*> &live_intervals,
        Array<Live_Sets> &live_sets,
        Array<Cfg_Edge> &cfg_edges)
{
    Instruction_List &instructions = routine->instructions;

    array::Resize(live_sets, instructions.count);
    for (s64 i = 0; i < live_sets.count; i++)
    {
        live_sets[i].live_in = { };
        live_sets[i].live_out = { };
    }

    // Add argument registers to the first instructions live out set.
    Live_Sets &entry = live_sets[0];

    Reg_Seq_Index arg_reg_index = { };
    for (s64 i = 0; i < ir_routine->arg_count; i++)
    {
        Ir_Operand *arg = &ir_routine->args[i];
        Oper_Data_Type data_type = DataTypeFromType(arg->type);
        const Reg *arg_reg = GetArgRegister(ctx->reg_alloc, data_type, &arg_reg_index);
        if (arg_reg)
        {
            SetAddArg(entry.live_out, arg->var.name, data_type, *arg_reg);
        }
        else if (i >= ctx->reg_alloc->shadow_arg_reg_count)
        {
            SetAddSpilled(entry.live_out, arg->var.name, data_type);
        }
    }

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (s64 i = 0; i < instructions.count; i++)
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

            for (Operand_Use *use = instr->uses; use; use = use->next)
            {
                // NOTE(henrik): the operands are reads, but their access flags may not be reads.
                // TODO(henrik): Add rax (or the return value registers) to the uses of call instructions as writes
                changed |= AddOper(live_sets[i].live_in, use->oper, AF_ReadWrite);
            }

            Name writes[3] = { };
            if (instr->oper1.addr_mode == Oper_Addr_Mode::Direct && (instr->oper1.access_flags & AF_Write) != 0)
                writes[0] = GetOperName(instr->oper1);
            if (instr->oper2.addr_mode == Oper_Addr_Mode::Direct && (instr->oper2.access_flags & AF_Write) != 0)
                writes[1] = GetOperName(instr->oper2);
            if (instr->oper3.addr_mode == Oper_Addr_Mode::Direct && (instr->oper3.access_flags & AF_Write) != 0)
                writes[2] = GetOperName(instr->oper3);

            changed |= SetUnion(live_sets[i].live_in, live_sets[i].live_out, writes, 3);

            changed |= AddOper(live_sets[i].live_out, instr->oper1, AF_Write);
            changed |= AddOper(live_sets[i].live_out, instr->oper2, AF_Write);
            changed |= AddOper(live_sets[i].live_out, instr->oper3, AF_Write);

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
                    //changed |= SetUnion(live_sets[i].live_out, live_sets[li->instr_index].live_in);
                    s64 label_instr_i = li->instr_index;
                    while (label_instr_i < instructions.count &&
                        (Amd64_Opcode)instructions[label_instr_i]->opcode == OP_LABEL)
                    {
                        label_instr_i++;
                    }
                    if (label_instr_i < live_sets.count)
                        changed |= SetUnion(live_sets[i].live_out, live_sets[label_instr_i].live_in);
                }
            }
        }
    }

    // Collect CFG edges
    for (s64 current_I = 0; current_I < live_sets.count; current_I++)
    {
        Instruction *instr = instructions[current_I];
        if ((instr->flags & IF_Branch) != 0)
        {
            ASSERT(instr->oper1.type == Oper_Type::Label);
            Name label_name = instr->oper1.label.name;
            const Label_Instr *li = hashtable::Lookup(routine->labels, label_name);
            ASSERT(li != nullptr);
            s64 label_instr_index = -1;
            if (li->instr)
            {
                label_instr_index = li->instr_index;
            }

            Cfg_Edge edge = { };
            edge.instr_index = current_I;
            edge.branch_instr_index = label_instr_index;
            edge.falls_through = (instr->flags & IF_FallsThrough) != 0;
            array::Push(cfg_edges, edge);
        }
    }

    // Reduce liveness information to coarse live intervals
    for (s64 current_I = 0; current_I < live_sets.count; current_I++)
    {
        Live_Sets &sets = live_sets[current_I];
        for (s64 out_I = 0; out_I < sets.live_out.count; out_I++)
        {
            Name_Data_Type name_dt = sets.live_out[out_I];
            Live_Interval li = { };
            li.start = current_I;
            li.name = name_dt.name;
            li.reg = name_dt.fixed_reg;
            li.data_type = name_dt.data_type;
            li.is_fixed = (name_dt.fixed_reg.reg_index != REG_NONE) && !name_dt.arg;
            li.is_spilled = name_dt.spilled;
            for (s64 instr_i = current_I + 1; instr_i < live_sets.count; instr_i++)
            {
                Live_Sets &ls = live_sets[instr_i];
                b32 live_in = false;
                //b32 live_out = false;
                for (s64 i = 0; i < ls.live_in.count; i++)
                {
                    if (ls.live_in[i].name == li.name)
                    { live_in = true; break; }
                }
                //for (s64 i = 0; i < ls.live_out.count; i++)
                //{
                //    if (ls.live_out[i].name == li.name)
                //    { live_out = true; break; }
                //}
                if (!live_in)
                {
                    li.end = instr_i - 1;
                    break;
                }
                else
                {
                    for (s64 i = 0; i < ls.live_out.count; i++)
                    {
                        if (ls.live_out[i].name == li.name)
                        { array::EraseBySwap(ls.live_out, i); break; }
                    }
                }
            }
            Live_Interval *new_li = PushStruct<Live_Interval>(&ctx->arena);
            *new_li = li;

            Live_Interval *prev_li = nullptr;
            for (s64 i = 0; i < live_intervals.count; i++)
            {
                if (li.name == live_intervals[i]->name)
                {
                    prev_li = live_intervals[i];
                    break;
                }
            }
            if (!prev_li)
                array::Push(live_intervals, new_li);
            else
            {
                while (prev_li->next)
                    prev_li = prev_li->next;
                prev_li->next = new_li;
            }
        }
    }

    RA_DEBUG(ctx, {
        fprintf(stderr, "\n--Live in/out-- ");
        PrintName((IoFile*)stderr, routine->name);
        fprintf(stderr, "\n");
        for (s64 instr_i = 0; instr_i < live_sets.count; instr_i++)
        {
            Live_Sets sets = live_sets[instr_i];
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
            //fprintf(stderr, "\n");
        }
        fprintf(stderr, "--Live in/out end--\n");

        fprintf(stderr, "\n--Live intervals-- ");
        PrintName((IoFile*)stderr, routine->name);
        fprintf(stderr, "\n");
        for (s64 i = 0; i < live_intervals.count; i++)
        {
            Live_Interval *interval = live_intervals[i];
            if (!interval) continue;

            const char *indent = "";
            PrintName((IoFile*)stderr, interval->name);
            do
            {
                fprintf(stderr, "%s: \t[%d,%d] %d %s\n", indent,
                        interval->start, interval->end, (s32)interval->data_type,
                        (interval->is_spilled) ? "(spilled)" : "");
                indent = "\t";
                interval = interval->next;
            } while (interval);
        }
        fprintf(stderr, "--Live intervals end--\n\n");
    })
}

static void FreeLiveSets(Array<Live_Sets> &live_sets)
{
    for (s64 i = 0; i < live_sets.count; i++)
    {
        array::Free(live_sets[i].live_in);
        array::Free(live_sets[i].live_out);
    }
    array::Free(live_sets);
}


// Register allocation

static void MakeSpillComment(Codegen_Context *ctx, Ir_Comment *comment,
        String spill_name, const char *spill_type, const char *note)
{
    s64 note_size = 0;
    note_size += snprintf(nullptr, 0, "%s ", spill_type);
    note_size += spill_name.size;
    if (note)
        note_size += snprintf(nullptr, 0, "; %s", note);
    note_size += 1; // snprintf null terminates, thus needs one extra byte.

    char *buf = PushArray<char>(&ctx->arena, note_size);

    s64 len = snprintf(buf, note_size, "%s ", spill_type);
    strncpy(buf + len, spill_name.data, spill_name.size);
    len += spill_name.size;
    if (note)
        len += snprintf(buf + len, note_size - len, "; %s", note);

    comment->start = buf;
    comment->end = buf + note_size - 1; // take out null termination
}

static void InsertSpills(Codegen_Context *ctx, Routine *routine)
{
    Reg_Alloc *reg_alloc = ctx->reg_alloc;
    Array<Spill_Info> &spills = reg_alloc->spills;
    // Sort spills
    for (s64 i = 0; i < spills.count - 1; i++)
    {
        for (s64 j = i + 1; j < spills.count; j++)
        {
            if (spills[j].instr_index < spills[i].instr_index)
            {
                Spill_Info spill = spills[j];
                spills[j] = spills[i];
                spills[i] = spill;
            }
        }
    }
    s64 idx_offset = 0;
    for (s64 i = 0; i < spills.count; i++)
    {
        Spill_Info spill_info = spills[i];
        s64 index = spill_info.instr_index / 2;
        index += idx_offset;

        s64 count_old = routine->instructions.count;

        Ir_Comment comment = { };
        ctx->comment = &comment;
        String spill_name = spill_info.interval.name.str;

        switch (spill_info.spill_type)
        {
        case Spill_Type::Move:
            {
                MakeSpillComment(ctx, &comment, spill_name, "move", spill_info.note);
                InsertLoad(ctx, routine->instructions, index,
                        RegOperand(spill_info.target, spill_info.interval.data_type, AF_Write),
                        RegOperand(spill_info.interval.reg, spill_info.interval.data_type, AF_Read));
            } break;
        case Spill_Type::Spill:
            {
                RA_DEBUG(ctx, 
                {
                    fprintf(stderr, "Insert spill of ");
                    PrintName((IoFile*)stderr, spill_info.interval.name);
                    fprintf(stderr, " before instr %" PRId64 "", index);
                    fprintf(stderr, " data_type = %" PRId64 "\n", (s64)spill_info.interval.data_type);
                })

                MakeSpillComment(ctx, &comment, spill_name, "spill", spill_info.note);
                s64 offs = GetLocalOffset(ctx, spill_info.interval.name, spill_info.interval.data_type);
                InsertLoad(ctx, routine->instructions, index,
                        BaseOffsetOperand(REG_rbp, offs, spill_info.interval.data_type, AF_Write),
                        RegOperand(spill_info.interval.reg, spill_info.interval.data_type, AF_Read));
            } break;
        case Spill_Type::Unspill:
            {
                MakeSpillComment(ctx, &comment, spill_name, "unspill", spill_info.note);
                s64 offs = GetLocalOffset(ctx, spill_info.interval.name, spill_info.interval.data_type);
                InsertLoad(ctx, routine->instructions, index,
                        RegOperand(spill_info.interval.reg, spill_info.interval.data_type, AF_Write),
                        BaseOffsetOperand(REG_rbp, offs, spill_info.interval.data_type, AF_Read));
            } break;
        }
        idx_offset += routine->instructions.count - count_old;
    }
}

static void Spill(Reg_Alloc *reg_alloc,
        Live_Interval interval, s64 instr_index, s64 bias = 0, const char *note = nullptr)
{
    ASSERT(instr_index >= 0);
    Spill_Info spill_info = { };
    spill_info.note = note;
    spill_info.interval = interval;
    spill_info.instr_index = instr_index * 2 + bias;
    spill_info.spill_type = Spill_Type::Spill;
    array::Push(reg_alloc->spills, spill_info);
}

static void Unspill(Reg_Alloc *reg_alloc,
        Live_Interval interval, s64 instr_index, s64 bias = 0, const char *note = nullptr)
{
    if (instr_index < 0) return;
    ASSERT(instr_index >= 0);
    Spill_Info spill_info = { };
    spill_info.note = note;
    spill_info.interval = interval;
    spill_info.instr_index = instr_index * 2 + bias;
    spill_info.spill_type = Spill_Type::Unspill;
    array::Push(reg_alloc->spills, spill_info);
}

static void InsertMove(Reg_Alloc *reg_alloc,
        Live_Interval interval, Reg target, s64 instr_index, const char *note = nullptr)
{
    if (instr_index < 0) return;
    ASSERT(instr_index >= 0);
    Spill_Info spill_info = { };
    spill_info.note = note;
    spill_info.interval = interval;
    spill_info.target = target;
    spill_info.instr_index = instr_index * 2;
    spill_info.spill_type = Spill_Type::Move;
    array::Push(reg_alloc->spills, spill_info);
}

b32 IsLive(Live_Sets live_sets, Name name)
{
    for (s64 i = 0; i < live_sets.live_in.count; i++)
    {
        if (live_sets.live_in[0].name == name) return true;
    }
    for (s64 i = 0; i < live_sets.live_out.count; i++)
    {
        if (live_sets.live_out[0].name == name) return true;
    }
    return false;
}

static void CfgEdgeResolution(Codegen_Context *ctx,
        Array<Live_Interval> &live_intervals,
        Array<Live_Sets> &live_sets,
        Array<Cfg_Edge> &cfg_edges)
{
    (void)live_sets;
    for (s64 ei = 0; ei < cfg_edges.count; ei++)
    {
        Cfg_Edge edge = cfg_edges[ei];
        for (s64 i = 0; i < live_intervals.count; i++)
        {
            Live_Interval li = live_intervals[i];

            // If the live interval intersects with the edge..
            if (li.start <= edge.instr_index &&
                edge.instr_index <= li.end)
            {
                // ..find a conflicting interval (i.e. having the same name,
                // thus representing the same virtual register, but with
                // different register) at the branch target.
                s64 index = -1;
                for (s64 j = 0; j < live_intervals.count; j++)
                {
                    Live_Interval lj = live_intervals[j];
                    if (lj.start <= edge.branch_instr_index &&
                        edge.branch_instr_index <= lj.end)
                    {
                        if (li.name == lj.name)
                        {
                            if (li.reg != lj.reg)
                            {
                                index = j;
                            }
                            break;
                        }
                    }
                }
                // No confilicting interval found, continue to next interval.
                if (index == -1) continue; 

                // Check if there are other intervals using the register at the
                // branch point.
                s64 active_index = -1;
                Live_Interval interval = live_intervals[index];
                for (s64 j = 0; j < live_intervals.count; j++)
                {
                    Live_Interval lj = live_intervals[j];
                    if (lj.start <= edge.instr_index &&
                        edge.instr_index <= lj.end)
                    {
                        if (interval.reg == lj.reg)
                        {
                            active_index = j;
                            break;
                        }
                    }
                }
                // If the register was in use.. 
                if (active_index != -1)
                {
                    // ..use spilling to make sure that no value is
                    // overwritten.
                    Spill(ctx->reg_alloc, li, edge.instr_index, 0, "consistency");
                    li.reg = interval.reg;
                    Unspill(ctx->reg_alloc, li, edge.instr_index, 1, "consistency");
                }
                else
                {
                    // Otherwise we can do just a straight copy.
                    InsertMove(ctx->reg_alloc, li, interval.reg, edge.instr_index, "consistency");
                }
            }
        }
    }
}


static bool MaybeRemoveFromFreeRegs(Array<Reg> &free_regs, Reg reg)
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

static bool MaybeRemoveFromFreeRegs(Codegen_Context *ctx, Reg reg)
{
    if (IsFloatRegister(ctx->reg_alloc, reg))
        return MaybeRemoveFromFreeRegs(ctx->reg_alloc->free_float_regs, reg);
    else
        return MaybeRemoveFromFreeRegs(ctx->reg_alloc->free_regs, reg);
}

static void RemoveFromFreeRegs(Codegen_Context *ctx, Reg reg)
{
    ASSERT(MaybeRemoveFromFreeRegs(ctx, reg) == true);
}

// Adds live interval "inteval" to the set "active". The set is kept sorted
// by ascending iterval end.
static void AddToActive(Array<Live_Interval> &active, Live_Interval interval)
{
    s64 index = 0;
    for (; index < active.count; index++)
    {
        ASSERT(active[index].reg != interval.reg);
        if (interval.end <= active[index].end)
            break;
    }
    array::Insert(active, index, interval);
}

// Adds live interval "inteval" to the set "unhandled". The set is kept sorted
// by ascending iterval start.
static void AddToUnhandled(Array<Live_Interval> &unhandled, Live_Interval interval)
{
    s64 index = 0;
    for (; index < unhandled.count; index++)
    {
        if (interval.start <= unhandled[index].start)
            break;
    }
    array::Insert(unhandled, index, interval);
}

// Remove expired intervals from "active" set to the "handled" set and add the
// inteval's next, if it has one, to the "inactive" set.
static void ExpireOldIntervals(Codegen_Context *ctx,
        Array<Live_Interval> &active,
        Array<Live_Interval> &inactive,
        Array<Live_Interval> &handled,
        s64 instr_index)
{
    for (s64 i = 0; i < active.count; )
    {
        Live_Interval active_interval = active[i];
        if (active_interval.end >= instr_index)
            return;
        array::Erase(active, i);
        AddToUnhandled(handled, active_interval);

        if (active_interval.next)
        {
            Live_Interval next = *active_interval.next;
            next.reg = active_interval.reg;

            AddToUnhandled(inactive, next);
        }

        ReleaseRegister(ctx->reg_alloc, active_interval.reg, active_interval.data_type);
    }
}

// Remove intervals that have expired from the "inactive" set, and move
// intervals that overlap current instruction index "instr_index" from the
// "inactive" to the "active" set.
static void RenewInactiveIntervals(Codegen_Context *ctx,
        Array<Live_Interval> &active,
        Array<Live_Interval> &inactive,
        s64 instr_index)
{
    for (s64 i = 0; i < inactive.count; )
    {
        Live_Interval inactive_interval = inactive[i];
        if (inactive_interval.end < instr_index)
        {
            // NOTE(henrik): I don't know when this should happen, but it is included in the algorithm given in
            //
            INVALID_CODE_PATH;
            array::Erase(inactive, i);
            continue;
        }
        if (inactive_interval.start <= instr_index)
        {
            array::Erase(inactive, i);
            if (!MaybeRemoveFromFreeRegs(ctx, inactive_interval.reg))
            {
                if (!HasFreeRegisters(ctx->reg_alloc, inactive_interval.data_type))
                {
                    continue;
                }
                else
                {
                    Reg free_reg = GetFreeRegister(ctx->reg_alloc, inactive_interval.data_type);
                    inactive_interval.reg = free_reg;
                }
            }
            AddToActive(active, inactive_interval);
            //Unspill(ctx->reg_alloc, inactive_interval, inactive_interval.start);
        }
        i++;
    }
}

// Spill either "interval" or the last interval in the "active" set, depending
// on which expires earlier.
static void SpillAtInterval(Codegen_Context *ctx, Array<Live_Interval> &active, Live_Interval interval)
{
    s64 spill_i = active.count-1;
    Live_Interval spill = active[spill_i];
    if (spill.end > interval.end)
    {
        Spill(ctx->reg_alloc, spill, interval.start);

        interval.reg = spill.reg;
        GetLocalOffset(ctx, spill.name, spill.data_type);
        array::Erase(active, spill_i);
        AddToActive(active, interval);
    }
    else
    {
        Spill(ctx->reg_alloc, spill, interval.start);

        GetLocalOffset(ctx, interval.name, interval.data_type);
    }
}

// Spill interval from "active" set that has the fixed register of "interval"
// allocated. If the register is not currently allocated, then allocate it for
// the "interval".
static void SpillFixedRegAtInterval(Codegen_Context *ctx,
        Array<Live_Interval> &unhandled,
        Array<Live_Interval> &active, Live_Interval interval)
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
        // TODO(henrik): We could allocate a new register for the active
        // interval, if there was any free registers. This could be a bit
        // hairy, if some of the free registers are going to be needed soon,
        // e.g. in case of a function call argument registers. Could
        // potentially lead to: active has arg_reg0 allocated, new fixed
        // interval needs arg_reg0, active allocates new free reg which by
        // chance is arg_reg1.  Soon new fixed interval needs arg_reg1, and so
        // on...

        Live_Interval spill = active[spill_i];
        // NOTE(henrik): This here prevents spilling registers whose live
        // interval ends after the this instruction.
        if (spill.end == interval.start)
            return;
        Spill(ctx->reg_alloc, spill, interval.start);

        RA_DEBUG(ctx,
        {
            fprintf(stderr, "Spilled ");
            PrintName((IoFile*)stderr, spill.name);
            fprintf(stderr, " in reg %s at instr %d\n",
                    GetRegNameStr(spill.reg), interval.start);

            s64 offs = GetLocalOffset(ctx, spill.name, spill.data_type);
            fprintf(stderr, " at offset %" PRId64 "\n", offs);
        })

        GetLocalOffset(ctx, spill.name, spill.data_type);
        array::Erase(active, spill_i);
        AddToActive(active, interval);

        spill.start = interval.end + 1;
        if (spill.end > spill.start)
        {
            Unspill(ctx->reg_alloc, spill, spill.start);
            AddToUnhandled(unhandled, spill);
        }
    }
}

static b32 SetRegOperand(Reg_Alloc *reg_alloc, Array<Live_Interval> active, Operand *oper, Name oper_name)
{
    if (oper_name.str.size == 0)
        return true;

    Reg reg = { };
    if (oper->type == Oper_Type::FixedRegister)
    {
        reg = oper->fixed_reg.reg;
    }
    else
    {
        for (s64 i = 0; i < active.count; i++)
        {
            if (active[i].name == oper_name)
            {
                reg = active[i].reg;
                break;
            }
        }
    }
    if (reg.reg_index != REG_NONE)
    {
        Operand reg_oper = RegOperand(reg, oper->data_type, oper->access_flags);
        reg_oper.addr_mode = oper->addr_mode;
        reg_oper.scale_offset = oper->scale_offset;
        *oper = reg_oper;

        DirtyRegister(reg_alloc, reg);
        return true;
    }
    return false;
}

static void SetOperand(Codegen_Context *ctx,
        Array<Live_Interval> active,
        Operand *oper, s64 instr_index)
{
    Name oper_name = GetOperName(*oper);
    if (SetRegOperand(ctx->reg_alloc, active, oper, oper_name)) return;

    (void)instr_index;
    s64 offs;
    if (GetLocalOffset(ctx, oper_name, &offs))
    {
        *oper = BaseOffsetOperand(REG_rbp, offs, oper->data_type, oper->access_flags);

        RA_DEBUG(ctx,
        {
            fprintf(stderr, "Local offset %" PRId64 " for ", offs);
            PrintName((IoFile*)stderr, oper_name);
            fprintf(stderr, " at %" PRId64 "\n", instr_index);
        })
    }
    else
    {
        RA_DEBUG(ctx, 
        {
            fprintf(stderr, "No local offset for ");
            PrintName((IoFile*)stderr, oper_name);
            fprintf(stderr, " at %" PRId64 "\n", instr_index);
        })
        INVALID_CODE_PATH;
    }
}

static void SpillCallerSaves(Reg_Alloc *reg_alloc, Array<Live_Interval> active, s64 instr_index)
{
    for (s64 i = 0; i < active.count; i++)
    {
        if (!active[i].is_fixed && IsCallerSave(reg_alloc, active[i].reg))
        {
            Live_Interval interval = active[i];
            interval.name = reg_save_names[interval.reg.reg_index];
            Spill(reg_alloc, interval, instr_index);
        }
    }
}

static void UnspillCallerSaves(Reg_Alloc *reg_alloc, Array<Live_Interval> active, s64 instr_index)
{
    for (s64 i = 0; i < active.count; i++)
    {
        if (!active[i].is_fixed && IsCallerSave(reg_alloc, active[i].reg))
        {
            Live_Interval interval = active[i];
            interval.name = reg_save_names[interval.reg.reg_index];
            Unspill(reg_alloc, interval, instr_index);
        }
    }
}

static void ScanInstruction(Codegen_Context *ctx, Routine *routine,
        Array<Live_Interval> &active, s64 instr_i)
{
    Reg_Alloc *reg_alloc = ctx->reg_alloc;
    Instruction *instr = routine->instructions[instr_i];
    if ((Amd64_Opcode)instr->opcode == OP_call)
    {
        SpillCallerSaves(reg_alloc, active, instr_i - 1);
        UnspillCallerSaves(reg_alloc, active, instr_i + 1);
    }
    else if ((Amd64_Opcode)instr->opcode == OP_SPILL)
    {
        for (s64 i = 0; i < active.count; i++)
        {
            Live_Interval interval = active[i];
            Name oper_name = GetOperName(instr->oper1);
            if (interval.name == oper_name)
            {
                Spill(reg_alloc, interval, instr_i);
                array::Erase(active, i);
                ReleaseRegister(reg_alloc, interval.reg, interval.data_type);
                break;
            }
        }
        instr->opcode = (Opcode)OP_nop;
        instr->oper1 = NoneOperand();
    }
#if 0
    else if ((instr->flags & IF_Branch) != 0)
    {
        for (s64 i = 0; i < active.count; i++)
        {
            if (active[i].is_fixed) continue;
            if (active[i].is_spilled) continue;
            Spill(reg_alloc, active[i], instr_i);
        }
    }
    else if ((Amd64_Opcode)instr->opcode == OP_LABEL)
    {
        Instruction *prev_instr = (instr_i > 0) ?
            routine->instructions[instr_i-1] : nullptr;
        if (prev_instr && (prev_instr->flags & IF_FallsThrough) != 0)
        for (s64 i = 0; i < active.count; i++)
        {
            if (active[i].is_fixed) continue;
            if (active[i].is_spilled) continue;
            Spill(reg_alloc, active[i], instr_i);
        }
        for (s64 i = 0; i < active.count; i++)
        {
            if (active[i].is_fixed) continue;
            if (active[i].is_spilled) continue;
            Unspill(reg_alloc, active[i], instr_i+1);
        }
    }
#endif
    SetOperand(ctx, active, &instr->oper1, instr_i);
    SetOperand(ctx, active, &instr->oper2, instr_i);
    SetOperand(ctx, active, &instr->oper3, instr_i);
}

static void PrintIntervals(Array<Live_Interval> intervals)
{
    for (s64 i = 0; i < intervals.count; i++)
    {
        Live_Interval li = intervals[i];
        PrintName((IoFile*)stderr, li.name);
        fprintf(stderr, "=%s", GetRegNameStr(li.reg));
        fprintf(stderr, ", ");
    }
    fprintf(stderr, "\n");
}

static void ScanInstructions(Codegen_Context *ctx, Routine *routine,
        Array<Live_Interval> &active,
        Array<Live_Interval> &inactive,
        Array<Live_Interval> &handled,
        s64 interval_start, s64 next_interval_start)
{
    RA_DEBUG(ctx,
        fprintf(stderr, "Scanning instructions in live interval [%" PRId64 ",%" PRId64 "]\n",
                interval_start, next_interval_start);
    )

    for (s64 instr_i = interval_start; instr_i <= next_interval_start; instr_i++)
    {
        ExpireOldIntervals(ctx, active, inactive, handled, instr_i);
        RenewInactiveIntervals(ctx, active, inactive, instr_i);

        RA_DEBUG(ctx,
        {
            fprintf(stderr, "%d\ta:", (s32)instr_i);
            PrintIntervals(active);
            fprintf(stderr, "\ti:");
            PrintIntervals(inactive);
        })
        
        ScanInstruction(ctx, routine, active, instr_i);
    }
}

static void LinearScanRegAllocation(Codegen_Context *ctx, Routine *routine,
        Array<Live_Interval> &live_intervals, Array<Live_Interval> &handled)
{
    Reg_Alloc *reg_alloc = ctx->reg_alloc;

    bool is_leaf = (routine->flags & ROUT_Leaf) != 0;
    ResetRegAlloc(reg_alloc, !is_leaf);

    s32 last_interval_start = 0;
    //s32 last_interval_end = 0;
    Array<Live_Interval> active = { };
    Array<Live_Interval> inactive = { };

    for (s64 i = 0; i < live_intervals.count; i++)
    {
        Live_Interval interval = live_intervals[i];
        s64 next_interval_start = interval.end;
        if (i + 1 < live_intervals.count)
            next_interval_start = live_intervals[i + 1].start;

        ExpireOldIntervals(ctx, active, inactive, handled, interval.start);
        RenewInactiveIntervals(ctx, active, inactive, interval.start);

        if (interval.reg.reg_index != REG_NONE)
        {
            SpillFixedRegAtInterval(ctx, live_intervals, active, interval);
        }
        else if (!HasFreeRegisters(reg_alloc, interval.data_type))
        {
            SpillAtInterval(ctx, active, interval);
        }
        else
        {
            Reg reg = GetFreeRegister(reg_alloc, interval.data_type);
            interval.reg = reg;
            AddToActive(active, interval);

            if (interval.is_spilled)
            {
                Unspill(reg_alloc, interval, interval.start);
            }
        }

        ScanInstructions(ctx, routine, active, inactive, handled,
                interval.start, next_interval_start - 1);

        last_interval_start = interval.start;
        //if (interval.end > last_interval_end)
        //    last_interval_end = interval.end;
    }

    s64 last_interval_end = routine->instructions.count - 1;
    ScanInstructions(ctx, routine, active, inactive, handled,
            last_interval_start, last_interval_end);

    array::Free(active);
    array::Free(inactive);

    // TODO(henrik): Implement this more cleanly.
    for (s64 i = REG_NONE + 1; i < REG_COUNT; i++)
    {
        Reg reg = { (u8)i };
        if (IsCalleeSave(reg_alloc, reg) && IsRegisterDirty(reg_alloc, reg))
        {
            Oper_Data_Type data_type = (IsFloatRegister(reg_alloc, reg)) ?
                Oper_Data_Type::F64 : Oper_Data_Type::U64;
            s64 offs = GetLocalOffset(ctx, reg_save_names[i], data_type);
            Amd64_Opcode mov_op = MoveOp(data_type);

            Operand stack_slot = BaseOffsetOperand(REG_rbp, offs, data_type, AF_Write);
            Operand reg_oper = RegOperand(reg, data_type, AF_Read);

            PushInstruction(ctx, routine->callee_save_spills,
                    mov_op, stack_slot, reg_oper);
            PushInstruction(ctx, routine->callee_save_unspills,
                    mov_op, W_(reg_oper), R_(stack_slot));
        }
    }
}

static void GenerateCode(Codegen_Context *ctx, Ir_Routine *ir_routine, Routine *routine)
{
    routine->ir_routine = ir_routine;
    routine->flags = ir_routine->flags;

    bool toplevel = (ir_routine->name.str.size == 0);
    routine->name = (toplevel)
        ? PushName(&ctx->arena, "init_")
        : ir_routine->name;

    // Set local offsets for arguments
    Reg_Seq_Index arg_reg_index = { };
    for (s64 i = 0; i < ir_routine->arg_count; i++)
    {
        Ir_Operand *arg = &ir_routine->args[i];
        Oper_Data_Type data_type = DataTypeFromType(arg->type);
        GetArgRegister(ctx->reg_alloc, data_type, &arg_reg_index);
        s64 offset = GetOffsetFromBasePointer(ctx->reg_alloc, arg_reg_index);
        if (offset > 0)
            SetArgLocalOffset(ctx, arg->var.name, offset);
    }

    Oper_Data_Type dt = Oper_Data_Type::U64;
    Operand rbp = RegOperand(REG_rbp, dt, AF_Read);
    Operand rsp = RegOperand(REG_rsp, dt, AF_Read);
    if (toplevel)
    {
        // NOTE(henrik): Here we align stack to 16 byte boundary, if we are the
        // toplevel init procedure (i.e. program entry point).
        Operand rax = RegOperand(REG_rax, dt, AF_Read);
        Operand rbx = RegOperand(REG_rbx, dt, AF_Read);
        PushPrologue(ctx, OP_mov, W_(rax), R_(rbp));
        PushPrologue(ctx, OP_and, RW_(rax), ImmOperand((u64)0xf, AF_Read));
        PushPrologue(ctx, OP_mov, W_(rbx), ImmOperand((u64)0x10, AF_Read));
        PushPrologue(ctx, OP_sub, RW_(rbx), R_(rax));
        PushPrologue(ctx, OP_sub, RW_(rsp), R_(rbx));
    }
    else
    {
        PushPrologue(ctx, OP_push, R_(rbp));
    }
    PushPrologue(ctx, OP_mov, W_(rbp), R_(rsp));


    for (s64 i = 0; i < ir_routine->instructions.count; i++)
    {
        Ir_Instruction *ir_instr = &ir_routine->instructions[i];
        if (!ctx->comment)
            ctx->comment = &ir_instr->comment;
        Ir_Instruction *ir_next_instr = nullptr;
        if (i + 1 < ir_routine->instructions.count)
            ir_next_instr = &ir_routine->instructions[i + 1];
        bool skip_next = false;
        GenerateCode(ctx, ir_routine, ir_instr, ir_next_instr, &skip_next);
        if (skip_next) i++;
    }
    if (toplevel)
    {
        // Add call to main function
        Operand main_label = LabelOperand(ctx->comp_ctx->env.main_func_name, AF_Read);
        PushInstruction(ctx, OP_call, main_label);

        // Add call to exit
        Oper_Data_Type return_type = Oper_Data_Type::S32;
        Reg_Seq_Index arg_reg_index = { };
        const Reg *ret_reg = GetReturnRegister(ctx->reg_alloc, return_type, 0);
        const Reg *arg_reg = GetArgRegister(ctx->reg_alloc, return_type, &arg_reg_index);
        PushLoad(ctx,
                RegOperand(*arg_reg, return_type, AF_Write),
                RegOperand(*ret_reg, return_type, AF_Read));

        Name exit_name = MakeConstName("exit");
        Operand exit_label = LabelOperand(exit_name, AF_Read);
        PushInstruction(ctx, OP_call, exit_label);
    }
    PushInstruction(ctx, OP_LABEL, LabelOperand(ctx->return_label_name, AF_Read));
}

static void AllocateRegisters(Codegen_Context *ctx, Ir_Routine *ir_routine, Routine *routine)
{
    CollectLabelInstructions(ctx, routine);

    Array<Cfg_Edge> cfg_edges = { };
    Array<Live_Sets> live_sets = { };

    Array<Live_Interval*> live_interval_set = { };
    ComputeLiveness(ctx, ir_routine, routine,
            live_interval_set, live_sets, cfg_edges);

    // Sort the intervals in the singly linked list of every interval.
    for (s64 i = 0; i < live_interval_set.count; i++)
    {
        Live_Interval **interval = &live_interval_set[i];
        //if (!(*interval)->next) continue;

        bool done = false;
        while (!done)
        {
            Live_Interval **prev = interval;
            Live_Interval *ival = *interval;
            Live_Interval *next = ival->next;

            done = true;

            while (next)
            {
                if (next->start < ival->start)
                {
                    ival->next = next->next;
                    next->next = ival;
                    *prev = next;

                    done = false;
                }
                prev = &ival->next;
                ival = next;
                next = next->next;
            }
        }
    }

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
    array::Free(live_interval_set);

    Array<Live_Interval> final_intervals = { };
    LinearScanRegAllocation(ctx, routine, live_intervals, final_intervals);

    CfgEdgeResolution(ctx, final_intervals, live_sets, cfg_edges);

    InsertSpills(ctx, routine);

    array::Free(live_intervals);
    array::Free(cfg_edges);
    FreeLiveSets(live_sets);

    s64 locals_size = routine->locals_size;
    if (locals_size > 0)
    {
        locals_size = Align(locals_size, 16);
        PushPrologue(ctx, OP_sub,
                RegOperand(REG_rsp, Oper_Data_Type::U64, AF_Write),
                ImmOperand(locals_size, AF_Read));
        PushEpilogue(ctx, OP_mov,
                RegOperand(REG_rsp, Oper_Data_Type::PTR, AF_Write),
                RegOperand(REG_rbp, Oper_Data_Type::PTR, AF_Read));
    }
    PushEpilogue(ctx, OP_pop, RegOperand(REG_rbp, Oper_Data_Type::U64, AF_Write));
    PushEpilogue(ctx, OP_ret);
}

// Some "optimizations" to the generated code.

static b32 IsMove(Opcode opcode)
{
    return ((Amd64_Opcode)opcode == OP_mov ||
            (Amd64_Opcode)opcode == OP_movss ||
            (Amd64_Opcode)opcode == OP_movsd);
}

static b32 IsSameRegister(Operand oper1, Operand oper2)
{
    if (oper1.addr_mode != Oper_Addr_Mode::Direct) return false;
    if (oper2.addr_mode != Oper_Addr_Mode::Direct) return false;
    Reg r1;
    if (oper1.type == Oper_Type::Register)
        r1 = oper1.reg;
    else if (oper1.type == Oper_Type::FixedRegister)
        r1 = oper1.fixed_reg.reg;
    else
        return false;

    Reg r2;
    if (oper2.type == Oper_Type::Register)
        r2 = oper2.reg;
    else if (oper2.type == Oper_Type::FixedRegister)
        r2 = oper2.fixed_reg.reg;
    else
        return false;

    return r1 == r2;
}

static b32 IsSame(Operand oper1, Operand oper2)
{
    return IsSameRegister(oper1, oper2);
}

void OptimizeCode(Codegen_Context *ctx, Routine *routine)
{
    (void)ctx;
    for (s64 i = 0; i < routine->instructions.count; i++)
    {
        Instruction *instr = routine->instructions[i];
        if (IsMove(instr->opcode))
        {
            if (IsSame(instr->oper1, instr->oper2))
            {
                //MakeNop(instr);
                instr->flags |= IF_CommentedOut;
            }
        }
    }
    for (s64 i = 0; i < routine->instructions.count - 1; i++)
    {
        Instruction *instr_0 = routine->instructions[i];
        Instruction *instr_1 = routine->instructions[i + 1];
        if ((Amd64_Opcode)instr_0->opcode == OP_jmp &&
            (Amd64_Opcode)instr_1->opcode == OP_LABEL)
        {
            if (instr_0->oper1.label.name == instr_1->oper1.label.name)
            {
                instr_0->flags |= IF_CommentedOut;
            }
        }
        else if (IsMove(instr_0->opcode) &&
                 (instr_0->opcode == instr_1->opcode))
        {
            if (IsSame(instr_0->oper1, instr_1->oper2) &&
                IsSame(instr_0->oper2, instr_1->oper1))
            {
                instr_1->flags |= IF_CommentedOut;
            }
            else
            if (IsSame(instr_0->oper1, instr_1->oper1) &&
                IsSame(instr_0->oper2, instr_1->oper2))
            {
                instr_1->flags |= IF_CommentedOut;
            }
        }
    }
}

void GenerateCode_Amd64(Codegen_Context *ctx, Ir_Routine_List ir_routines)
{
    ctx->routine_count = ir_routines.count;
    ctx->routines = PushArray<Routine>(&ctx->arena, ir_routines.count);
    for (s64 i = 0; i < ir_routines.count; i++)
    {
        Routine *routine = &ctx->routines[i];
        *routine = { };
        ctx->current_routine = routine;
        GenerateCode(ctx, ir_routines[i], routine);
    }

    RA_DEBUG(ctx,
    {
        // Output generated code before register allocation for debugging.
        IoFile *f = ctx->code_out;
        FILE *tfile = fopen("out_.s", "w");

        ctx->code_out = (IoFile*)tfile;
        OutputCode(ctx);

        fclose(tfile);
        ctx->code_out = f;
    })

    for (s64 i = 0; i < ir_routines.count; i++)
    {
        Routine *routine = &ctx->routines[i];
        ctx->current_routine = routine;
        AllocateRegisters(ctx, ir_routines[i], routine);
    }

    for (s64 i = 0; i < ir_routines.count; i++)
    {
        OptimizeCode(ctx, &ctx->routines[i]);
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
        case Oper_Type::Register:
            if (oper.addr_mode == Oper_Addr_Mode::Direct)
            {
                len += fprintf((FILE*)file, "%s",
                        GetRegNameStr(oper.reg, oper.data_type));
            }
            else
            {
                len += fprintf((FILE*)file, "%s", GetRegNameStr(oper.reg));
            }
            break;
        case Oper_Type::FixedRegister:
            if (oper.addr_mode == Oper_Addr_Mode::Direct)
            {
                len += fprintf((FILE*)file, "%s",
                        GetRegNameStr(oper.fixed_reg.reg, oper.data_type));
            }
            else
            {
                len += fprintf((FILE*)file, "%s", GetRegNameStr(oper.fixed_reg.reg));
            }
            break;
        case Oper_Type::VirtualRegister:
            len += PrintName(file, oper.virtual_reg.name);
            break;
        case Oper_Type::Immediate:
            len += fprintf((FILE*)file, "%" PRIu64, oper.imm_u64);
            break;
    }
    return len;
}

static s64 PrintOperand(IoFile *file,
        Operand oper, const Operand *next_oper,
        b32 first, b32 lea)
{
    if (oper.type == Oper_Type::None) return 0;
    if (oper.addr_mode == Oper_Addr_Mode::IndexScale) return 0;
    if ((oper.access_flags & AF_Shadow) != 0) return 0;

    s64 len = 0;
    if (!first)
        len += fprintf((FILE*)file, ", ");

    if (oper.addr_mode == Oper_Addr_Mode::BaseOffset ||
        oper.addr_mode == Oper_Addr_Mode::BaseIndexOffset)
    {
        if (!lea)
        {
            switch (oper.data_type)
            {
                case Oper_Data_Type::BOOL:
                case Oper_Data_Type::U8:
                case Oper_Data_Type::S8:
                    len += fprintf((FILE*)file, "byte "); break;
                case Oper_Data_Type::U16:
                case Oper_Data_Type::S16:
                    len += fprintf((FILE*)file, "word "); break;
                case Oper_Data_Type::U32:
                case Oper_Data_Type::S32:
                case Oper_Data_Type::F32:
                    len += fprintf((FILE*)file, "dword "); break;
                case Oper_Data_Type::U64:
                case Oper_Data_Type::S64:
                case Oper_Data_Type::F64:
                case Oper_Data_Type::PTR:
                    len += fprintf((FILE*)file, "qword "); break;
            }
        }
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
    if ((instr->flags & IF_CommentedOut) != 0)
    {
        len += fprintf((FILE*)file, ";");
    }
    if ((Amd64_Opcode)instr->opcode == OP_LABEL)
    {
        len += PrintLabel(file, instr->oper1);
    }
    else
    {
        len += PrintPadding((FILE*)file, len, 4);
        len += PrintOpcode(file, (Amd64_Opcode)instr->opcode);

        if (instr->oper1.type != Oper_Type::None)
        {
            b32 lea = ((Amd64_Opcode)instr->opcode == OP_lea);
            len += PrintPadding((FILE*)file, len, 16);
            len += PrintOperand(file, instr->oper1, &instr->oper2, true, lea);
            len += PrintOperand(file, instr->oper2, &instr->oper3, false, lea);
            len += PrintOperand(file, instr->oper3, nullptr, false, lea);
        }
    }
    PrintComment((FILE*)file, len, instr->comment);
    fprintf((FILE*)file, "\n");
}

static void PrintInstructions(IoFile *file, Instruction_List &instructions)
{
    for (s64 i = 0; i < instructions.count; i++)
    {
        PrintInstruction(file, array::At(instructions, i));
    }
}

static void PrintRoutineArgs(IoFile *file, Routine *routine)
{
    s64 arg_count = routine->ir_routine->arg_count;
    Ir_Operand *args = routine->ir_routine->args;
    for (s64 i = 0; i < arg_count; i++)
    {
        Ir_Operand *arg = &args[i];
        fprintf((FILE*)file, ";   ");
        PrintName(file, arg->var.name);
        fprintf((FILE*)file, " : ");
        PrintType(file, arg->type);
        fprintf((FILE*)file, "\n");
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

        PrintRoutineArgs(file, routine);

        fprintf(f, "; prologue\n");
        PrintInstructions(file, routine->prologue);
        if (routine->callee_save_spills.count > 0)
        {
            fprintf(f, "; callee save spills\n");
            PrintInstructions(file, routine->callee_save_spills);
        }
        fprintf(f, "; routine body\n");
        PrintInstructions(file, routine->instructions);
        if (routine->callee_save_unspills.count > 0)
        {
            fprintf(f, "; callee save unspills\n");
            PrintInstructions(file, routine->callee_save_unspills);
        }
        fprintf(f, "; epilogue\n");
        PrintInstructions(file, routine->epilogue);
        fprintf(f, "; -----\n\n");
    }

    fprintf(f, "\nsection .bss\n");

    // globals

    if (ctx->global_var_count)
    {
        s64 offset = 0;
        fprintf(f, "\n;global variables\n");
        for (s64 i = 0; i < ctx->global_var_count; i++)
        {
            Symbol *symbol = ctx->global_vars[i];
            u32 align = GetAlign(symbol->type);
            u32 align_res_size = offset & (align - 1);
            offset += align_res_size;

            if (align_res_size)
                fprintf(f, "\tresb %u\t; (padding)\n", align_res_size);

            u32 size = GetAlignedSize(symbol->type);
            PrintName(file, symbol->unique_name);
            fprintf(f, ":\tresb %u\t; ", size);
            PrintType(file, symbol->type);
            fprintf(f, "\n");

            offset += size;
        }
    }

    fprintf(f, "\nsection .data\n");

    // constants

    if (ctx->float32_consts.count)
    {
        fprintf(f, "\nalign 16\n");
        for (s64 i = 0; i < ctx->float32_consts.count; i++)
        {
            Float32_Const fconst = ctx->float32_consts[i];
            PrintName(file, fconst.label_name);
            fprintf(f, ":\tdd\t%" PRIu32 "\t; %f\n", fconst.uvalue, fconst.value);
        }
    }
    if (ctx->float64_consts.count)
    {
        fprintf(f, "\nalign 16\n");
        for (s64 i = 0; i < ctx->float64_consts.count; i++)
        {
            Float64_Const fconst = ctx->float64_consts[i];
            PrintName(file, fconst.label_name);
            fprintf(f, ":\tdq\t%" PRIu64 "\t; %f\n", fconst.uvalue, fconst.value);
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
