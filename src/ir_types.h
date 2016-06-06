#ifndef H_HPLANG_IR_TYPES_H

#include "types.h"
#include "array.h"

namespace hplang
{

#define IR_OPCODES\
    PASTE_IR(IR_Label)\
    \
    PASTE_IR(IR_VarDecl)\
    \
    PASTE_IR(IR_Mov)\
    PASTE_IR(IR_MovSX)\
    PASTE_IR(IR_MovMember)\
    PASTE_IR(IR_MovElement)\
    \
    PASTE_IR(IR_Add)\
    PASTE_IR(IR_Sub)\
    PASTE_IR(IR_Mul)\
    PASTE_IR(IR_Div)\
    PASTE_IR(IR_Mod)\
    \
    PASTE_IR(IR_Eq)\
    PASTE_IR(IR_Neq)\
    PASTE_IR(IR_Lt)\
    PASTE_IR(IR_Leq)\
    PASTE_IR(IR_Gt)\
    PASTE_IR(IR_Geq)\
    \
    PASTE_IR(IR_And)\
    PASTE_IR(IR_Or)\
    PASTE_IR(IR_Xor)\
    \
    PASTE_IR(IR_Neg)\
    PASTE_IR(IR_Not)\
    PASTE_IR(IR_Compl)\
    PASTE_IR(IR_Addr)\
    PASTE_IR(IR_Deref)\
    \
    PASTE_IR(IR_Call)\
    PASTE_IR(IR_CallForeign)\
    PASTE_IR(IR_Arg)\
    PASTE_IR(IR_Return)\
    PASTE_IR(IR_Jump)\
    PASTE_IR(IR_Jz)\
    PASTE_IR(IR_Jnz)\
    \
    PASTE_IR(IR_S_TO_F32)\
    PASTE_IR(IR_S_TO_F64)\
    PASTE_IR(IR_F32_TO_S)\
    PASTE_IR(IR_F64_TO_S)\
    PASTE_IR(IR_F32_TO_F64)\
    PASTE_IR(IR_F64_TO_F32)\
    \
    PASTE_IR(IR_COUNT)

#define PASTE_IR(ir) ir,
enum Ir_Opcode
{
    IR_OPCODES
};
#undef PASTE_IR

enum Ir_Oper_Type
{
    IR_OPER_None,
    IR_OPER_Variable,
    //IR_OPER_GlobalVariable,
    IR_OPER_Temp,
    IR_OPER_Immediate,
    IR_OPER_Label,
    IR_OPER_Routine,
    IR_OPER_ForeignRoutine,
};

struct Type;

struct Ir_Variable
{
    Name name;
};

struct Ir_Temp
{
    Name name;
};

struct Ir_Label
{
    Name name;
    s64 target_loc;
};

struct Ir_Routine;

struct Ir_Operand
{
    Ir_Oper_Type oper_type;
    Type *type;
    union {
        Ir_Variable var;
        Ir_Temp temp;
        Ir_Label *label;

        bool imm_bool;
        s8 imm_s8;
        u8 imm_u8;
        s16 imm_s16;
        u16 imm_u16;
        s32 imm_s32;
        u32 imm_u32;
        s64 imm_s64;
        u64 imm_u64;
        f32 imm_f32;
        f64 imm_f64;
        void* imm_ptr;
        String imm_str;
    };
};

b32 operator == (Ir_Operand oper1, Ir_Operand oper2);
b32 operator != (Ir_Operand oper1, Ir_Operand oper2);

struct Ir_Comment
{
    const char *start;
    const char *end;
};

struct Ir_Instruction
{
    Ir_Opcode opcode;
    Ir_Operand target, oper1, oper2;
    Ir_Comment comment;
};

typedef Array<Ir_Instruction> Ir_Instruction_List;

struct Symbol;

struct Ir_Routine
{
    Symbol *symbol;
    Name name;

    s64 arg_count;
    Ir_Operand *args;

    Ir_Instruction_List instructions;
    s64 temp_count;
};

typedef Array<Ir_Routine*> Ir_Routine_List;

} // hplang

#define H_HPLANG_IR_TYPES_H
#endif
