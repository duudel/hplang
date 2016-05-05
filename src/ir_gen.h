#ifndef H_HPLANG_IR_GEN_H

#include "memory.h"
#include "array.h"
#include <cstdio>

namespace hplang
{

#define IR_OPCODES\
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
    PASTE_IR(IR_Call)\
    PASTE_IR(IR_CallForeign)\
    PASTE_IR(IR_Param)\
    PASTE_IR(IR_Return)\
\
    PASTE_IR(IR_COUNT)

#define PASTE_IR(ir) ir,
enum Ir_Opcode
{
    IR_OPCODES
};
#undef PASTE_IR

enum Ir_Type
{
    IR_TYP_ptr,
    IR_TYP_bool,
    IR_TYP_u8,
    IR_TYP_s8,
    IR_TYP_u16,
    IR_TYP_s16,
    IR_TYP_u32,
    IR_TYP_s32,
    IR_TYP_u64,
    IR_TYP_s64,
    IR_TYP_f32,
    IR_TYP_f64,
    IR_TYP_str,
};

enum Ir_Oper_Type
{
    IR_OPER_Variable,
    IR_OPER_Temp,
    IR_OPER_Immediate,
};

struct Ir_Temp
{
    //Name name;
    s64 temp_id;
};

struct Symbol;

struct Ir_Operand
{
    Ir_Oper_Type oper_type;
    Ir_Type type;
    union {
        Symbol *symbol;
        Ir_Temp temp;

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

struct Ir_Instruction
{
    Ir_Opcode opcode;
    Ir_Operand target, oper1, oper2;
};

typedef Array<Ir_Instruction> Ir_Instruction_List;

struct Ir_Routine
{
    Symbol *symbol;
    Name name;

    Ir_Instruction_List instructions;
    s64 temp_count;
};

typedef Array<Ir_Routine*> Ir_Routine_List;

struct Compiler_Context;

struct Ir_Gen_Context
{
    Memory_Arena arena;
    Ir_Routine_List routines;
    Compiler_Context *comp_ctx;
};

Ir_Gen_Context NewIrGenContext(Compiler_Context *comp_ctx);
void FreeIrGenContext(Ir_Gen_Context *ctx);

b32 GenIr(Ir_Gen_Context *ctx);

void PrintIr(FILE *file, Ir_Gen_Context *ctx);

} // hplang

#define H_HPLANG_IR_GEN_H
#endif
