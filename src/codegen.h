#ifndef H_HPLANG_CODEGEN_H

#include "types.h"
#include "array.h"
#include "memory.h"
#include "ir_types.h"
#include "io.h"

namespace hplang
{

struct Reg
{
    u8 reg_index;
};

inline b32 operator == (Reg r1, Reg r2)
{ return r1.reg_index == r2.reg_index; }
inline b32 operator != (Reg r1, Reg r2)
{ return r1.reg_index != r2.reg_index; }


enum class Oper_Type : u8
{
    None,
    Register,
    VirtualRegister,
    FixedRegister,
    Immediate,
    Label,
//    StringConst,
};

enum class Oper_Data_Type : u8
{
    PTR,
    BOOL,
    U8,
    S8,
    U16,
    S16,
    U32,
    S32,
    U64,
    S64,
    F32,
    F64,
};

enum class Oper_Addr_Mode : u8
{
    Direct,
    BaseOffset,
    BaseIndexOffset,
    IndexScale,
};

struct Label
{
    Name name;
};

// Physical register
struct Fixed_Reg
{
    Reg reg;
    Name name;
};

struct Virtual_Reg
{
    Name name;
};


enum Oper_Access_Flag_Bits
{
    AF_Read         = 1,
    AF_Write        = 2,
    AF_ReadWrite    = AF_Read | AF_Write,
    AF_Shadow       = 4,                    // The operand is not used directly
};

typedef Flag<Oper_Access_Flag_Bits, u8> Oper_Access_Flags;

struct Operand
{
    Oper_Type type;
    Oper_Access_Flags access_flags;
    Oper_Data_Type data_type;
    Oper_Addr_Mode addr_mode;
    s32 scale_offset;
    union {
        Reg         reg;
        Fixed_Reg   fixed_reg;
        Virtual_Reg virtual_reg;
        Label       label;
        //String      string_const;
        union {
            void *imm_ptr;
            bool imm_bool;
            u8 imm_u8;
            s8 imm_s8;
            u16 imm_u16;
            s16 imm_s16;
            u32 imm_u32;
            s32 imm_s32;
            u64 imm_u64;
            s64 imm_s64;
            f32 imm_f32;
            f64 imm_f64;
        };
    };
};

enum Opcode { };

enum Instr_Flag_Bits
{
    IF_FallsThrough = 1,
    IF_Branch       = 2,
    IF_CommentedOut = 4,
};

typedef Flag<Instr_Flag_Bits, u8> Instr_Flags;

struct Operand_Use
{
    Operand oper;
    Operand_Use *next;
};

struct Instruction
{
    Opcode opcode;
    Operand oper1;
    Operand oper2;
    Operand oper3;
    Ir_Comment comment;
    Instr_Flags flags;
    Operand_Use *uses;
};

typedef Array<Instruction*> Instruction_List;

struct Local_Offset
{
    Name name;
    s64 offset;
};

struct Label_Instr
{
    Name name;
    Instruction *instr; // the next instruction after label
    s64 instr_index;
};

struct Routine
{
    Name name;
    s64 temp_count;
    u32 flags;

    s64 locals_size;
    Array<Local_Offset*> local_offsets;

    Array<Label_Instr*> labels;

    Ir_Routine *ir_routine;

    Instruction_List instructions;
    Instruction_List prologue;
    Instruction_List callee_save_spills;
    Instruction_List callee_save_unspills;
    Instruction_List epilogue;
};

struct Float32_Const
{
    Name label_name;
    union {
        f32 value;
        u32 uvalue;
    };
};

struct Float64_Const
{
    Name label_name;
    union {
        f64 value;
        u64 uvalue;
    };
};

struct String_Const
{
    Name label_name;
    String value;
};

struct Compiler_Context;
struct Reg_Alloc;

struct Codegen_Context
{
    Memory_Arena arena;

    Codegen_Target target;
    Reg_Alloc *reg_alloc;

    Name return_label_name;

    s64 current_arg_count;
    s64 fixed_reg_id;
    s64 temp_id;
    Ir_Comment *comment;

    Array<Float32_Const> float32_consts;
    Array<Float64_Const> float64_consts;
    Array<String_Const> str_consts;

    s64 routine_count;
    Routine *routines;

    Routine *current_routine;

    s64 foreign_routine_count;
    Name *foreign_routines;

    s64 global_var_count;
    Symbol **global_vars;

    IoFile *code_out;
    Compiler_Context *comp_ctx;
};

Codegen_Context NewCodegenContext(IoFile *out,
        Compiler_Context *comp_ctx, Codegen_Target cg_target);
void FreeCodegenContext(Codegen_Context *ctx);

void GenerateCode(Codegen_Context *ctx, Ir_Routine_List routines,
        Array<Name> foreign_routines, Array<Symbol*> global_vars);

void OutputCode(Codegen_Context *ctx);

const char* GetTargetString(Codegen_Target target);

} // hplang

#define H_HPLANG_CODEGEN_H
#endif
