
#include "ir_gen.h"
#include "common.h"
#include "compiler.h"
#include "symbols.h"
#include "ast_types.h"
#include "assert.h"

#include <cstdio>
#include <cinttypes> // For printf formats

namespace hplang
{

b32 operator == (Ir_Operand oper1, Ir_Operand oper2)
{
    if (oper1.oper_type != oper2.oper_type) return false;
    switch (oper1.oper_type)
    {
        case IR_OPER_None: return true;
        case IR_OPER_Variable:
        case IR_OPER_Routine:
        case IR_OPER_ForeignRoutine:
            return oper1.var.name == oper2.var.name;
        case IR_OPER_Temp:
            return oper1.temp.name == oper2.temp.name;
            //return oper1.temp.temp_id == oper2.temp.temp_id;
        case IR_OPER_Immediate:
            if (oper1.type != oper2.type) return false;
            switch (oper1.type->tag)
            {
                case TYP_none:
                case TYP_pending:
                case TYP_null:
                case TYP_void:
                    INVALID_CODE_PATH;
                    break;

                case TYP_pointer:   return oper1.imm_ptr == oper2.imm_ptr;
                case TYP_bool:      return oper1.imm_bool == oper2.imm_bool;
                case TYP_char: case TYP_u8: case TYP_s8:
                    return oper1.imm_u8 == oper2.imm_u8;
                case TYP_u16: case TYP_s16:
                    return oper1.imm_u16 == oper2.imm_u16;
                case TYP_u32: case TYP_s32:
                    return oper1.imm_u32 == oper2.imm_u32;
                case TYP_u64: case TYP_s64:
                    return oper1.imm_u64 == oper2.imm_u64;
                case TYP_f32:
                    return oper1.imm_f32 == oper2.imm_f32;
                case TYP_f64:
                    return oper1.imm_f64 == oper2.imm_f64;
                case TYP_string:
                    return oper1.imm_str == oper2.imm_str;

                case TYP_Struct:
                case TYP_Function:
                    INVALID_CODE_PATH;
                    return false;
            }
            return false;
        case IR_OPER_Label:
            return oper1.label->target_loc == oper2.label->target_loc;
    }
    INVALID_CODE_PATH;
    return false;
}

b32 operator != (Ir_Operand oper1, Ir_Operand oper2)
{
    return !(oper1 == oper2);
}


#define PASTE_IR(ir) #ir,
static const char *ir_opcode_names[] = {
    IR_OPCODES
};
#undef PASTE_IR

Ir_Gen_Context NewIrGenContext(Compiler_Context *comp_ctx)
{
    Ir_Gen_Context result = { };
    result.comp_ctx = comp_ctx;
    return result;
}

static void FreeRoutine(Ir_Routine *routine)
{
    array::Free(routine->instructions);
}

void FreeIrGenContext(Ir_Gen_Context *ctx)
{
    for (s64 i = 0; i < ctx->routines.count; i++)
    {
        FreeRoutine(array::At(ctx->routines, i));
    }
    array::Free(ctx->routines);
}

static b32 ContinueGen(Ir_Gen_Context *ctx)
{
    (void)ctx;
    // NOTE(henrik): if there is a possibility for ICE, could use this to stop
    // compilation.
    return true;
}

static Ir_Routine* PushRoutine(Ir_Gen_Context *ctx, Name name, s64 arg_count)
{
    Ir_Routine *routine = PushStruct<Ir_Routine>(&ctx->arena);
    *routine = { };
    routine->name = name;
    if (arg_count > 0)
    {
        routine->arg_count = arg_count;
        routine->args = PushArray<Ir_Operand>(&ctx->arena, arg_count);
    }
    array::Push(ctx->routines, routine);
    return routine;
}

static Ir_Operand NoneOperand()
{
    Ir_Operand oper = { };
    oper.oper_type = IR_OPER_None;
    return oper;
}

static Ir_Operand NewImmediateNull(Ir_Routine *routine)
{
    (void)routine;
    Ir_Operand oper = { };
    oper.oper_type = IR_OPER_Immediate;
    oper.type = GetBuiltinType(TYP_pointer);
    oper.imm_ptr = nullptr;
    return oper;
}

#define IR_IMM(t, typ, member)\
static Ir_Operand NewImmediate(Ir_Routine *routine, t value)\
{\
    (void)routine;\
    Ir_Operand oper = { };\
    oper.oper_type = IR_OPER_Immediate;\
    oper.type = GetBuiltinType(typ);\
    oper.member = value;\
    return oper;\
}

//IR_IMM(void*, TYP_pointer, imm_ptr)
IR_IMM(bool, TYP_bool, imm_bool)
IR_IMM(u8, TYP_u8, imm_u8)
//IR_IMM(s8, TYP_s8, imm_s8)
//IR_IMM(u16, TYP_u16, imm_u16)
//IR_IMM(s16, TYP_s16, imm_s16)
//IR_IMM(u32, TYP_u32, imm_u32)
//IR_IMM(s32, TYP_s32, imm_s32)
//IR_IMM(u64, TYP_u64, imm_u64)
//IR_IMM(s64, TYP_s64, imm_s64)
IR_IMM(f32, TYP_f32, imm_f32)
IR_IMM(f64, TYP_f64, imm_f64)
IR_IMM(String, TYP_string, imm_str)

#undef IR_IMM

static Ir_Operand NewImmediateInt(Ir_Routine *routine, u64 value, Type *type)
{
    (void)routine;
    Ir_Operand oper = { };
    oper.oper_type = IR_OPER_Immediate;
    oper.type = type;
    oper.imm_u64 = value;
    return oper;
}

static Ir_Operand NewImmediateOffset(Ir_Routine *routine, s64 value)
{
    (void)routine;
    Ir_Operand oper = { };
    oper.oper_type = IR_OPER_Immediate;
    oper.type = GetBuiltinType(TYP_s64);
    oper.imm_s64 = value;
    return oper;
}

static Ir_Operand NewVariableRef(Ir_Routine *routine, Type *type, Name name)
{
    (void)routine;
    Ir_Operand oper = { };
    oper.oper_type = IR_OPER_Variable;
    oper.type = type;
    oper.var.name = name;
    return oper;
}

static Ir_Operand NewRoutineRef(Ir_Routine *routine, Type *type, Name name)
{
    (void)routine;
    Ir_Operand oper = { };
    oper.oper_type = IR_OPER_Routine;
    oper.type = type;
    oper.var.name = name;
    return oper;
}

static Ir_Operand NewForeignRoutineRef(Ir_Routine *routine, Type *type, Name name)
{
    (void)routine;
    Ir_Operand oper = { };
    oper.oper_type = IR_OPER_ForeignRoutine;
    oper.type = type;
    oper.var.name = name;
    return oper;
}

static Ir_Operand NewTemp(Ir_Gen_Context *ctx, Ir_Routine *routine, Type *type)
{
    s64 temp_id = routine->temp_count++;

    const s64 buf_size = 40;
    char buf[buf_size];
    snprintf(buf, buf_size, "@temp%" PRId64, temp_id);

    Ir_Operand oper = { };
    oper.oper_type = IR_OPER_Temp;
    oper.type = type;
    //oper.temp.temp_id = routine->temp_count++;
    oper.temp.name = PushName(&ctx->arena, buf);
    return oper;
}

static Ir_Operand NewLabel(Ir_Gen_Context *ctx)
{
    Ir_Label *label = PushStruct<Ir_Label>(&ctx->arena);
    *label = { };

    Ir_Operand oper = { };
    oper.oper_type = IR_OPER_Label;
    oper.label = label;
    return oper;
}

static void ExtractComment(Ir_Gen_Context *ctx, Ast_Node *node)
{
    Ir_Comment comment = { };

    Open_File *open_file = node->file_loc.file;
    const char *file_start = (const char*)open_file->contents.ptr;
    const char *file_end = file_start + open_file->contents.size;
    comment.start = file_start + node->file_loc.offset_start;
    comment.end = comment.start;

    while (comment.end != file_end)
    {
        if (comment.end[0] == '\n' || comment.end[0] == '\r')
            break;
        comment.end++;
        if (comment.end - comment.start > 32)
            break;
    }

    ctx->comment = comment;
}

static void PushInstruction(Ir_Gen_Context *ctx, Ir_Routine *routine,
        Ir_Opcode opcode,
        Ir_Operand target = NoneOperand(),
        Ir_Operand oper1 = NoneOperand(),
        Ir_Operand oper2 = NoneOperand())
{
    Ir_Instruction instr = { };
    instr.opcode = opcode;
    instr.target = target;
    instr.oper1 = oper1;
    instr.oper2 = oper2;
    instr.comment = ctx->comment;
    array::Push(routine->instructions, instr);

    ctx->comment = { };
}

static void PushJump(Ir_Gen_Context *ctx, Ir_Routine *routine,
        Ir_Opcode opcode,
        Ir_Operand jump_target,
        Ir_Operand oper1 = NoneOperand(),
        Ir_Operand oper2 = NoneOperand())
{
    ASSERT(jump_target.oper_type == IR_OPER_Label);
    PushInstruction(ctx, routine, opcode, jump_target, oper1, oper2);
}

static void SetLabelTarget(Ir_Routine *routine, Ir_Operand label_oper)
{
    s64 target = routine->instructions.count;
    label_oper.label->target_loc = target;
}



static Ir_Operand GenExpression(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine);

static Ir_Operand GenTypecastExpr(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    Ast_Expr *oper_expr = expr->typecast_expr.expr;
    Ir_Operand oper_res = GenExpression(ctx, oper_expr, routine);

    Type *oper_type = oper_expr->expr_type;
    Ir_Operand res = NewTemp(ctx, routine, oper_type);
    Type *res_type = res.type;
    switch (oper_type->tag)
    {
        case TYP_none:
        case TYP_pending:
        case TYP_null:
        case TYP_void:
            INVALID_CODE_PATH;
            break;
        case TYP_string:
            INVALID_CODE_PATH;
            break;
        case TYP_Struct:
            INVALID_CODE_PATH;
            break;
        case TYP_Function:
            INVALID_CODE_PATH;
            break;

        case TYP_pointer:
            PushInstruction(ctx, routine, IR_Mov, res, oper_res);
            break;
        case TYP_bool:
            PushInstruction(ctx, routine, IR_Mov, res, oper_res);
            break;
        case TYP_char:
            PushInstruction(ctx, routine, IR_Mov, res, oper_res);
            break;
        case TYP_u8:
            switch (res_type->tag)
            {
            case TYP_f32:
                PushInstruction(ctx, routine, IR_S_TO_F32, res, oper_res);
                break;
            case TYP_f64:
                PushInstruction(ctx, routine, IR_S_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(ctx, routine, IR_Mov, res, oper_res);
            }
            break;
        case TYP_s8:
            switch (res_type->tag)
            {
            case TYP_f32:
                PushInstruction(ctx, routine, IR_S_TO_F32, res, oper_res);
                break;
            case TYP_f64:
                PushInstruction(ctx, routine, IR_S_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(ctx, routine, IR_Mov, res, oper_res);
            }
            break;
        case TYP_u16:
            switch (res_type->tag)
            {
            case TYP_f32:
                PushInstruction(ctx, routine, IR_S_TO_F32, res, oper_res);
                break;
            case TYP_f64:
                PushInstruction(ctx, routine, IR_S_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(ctx, routine, IR_Mov, res, oper_res);
            }
            break;
        case TYP_s16:
            switch (res_type->tag)
            {
            case TYP_f32:
                PushInstruction(ctx, routine, IR_S_TO_F32, res, oper_res);
                break;
            case TYP_f64:
                PushInstruction(ctx, routine, IR_S_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(ctx, routine, IR_Mov, res, oper_res);
            }
            break;
            // TODO(henrik): There are no AMD64 instructions from unsigned int -> float/double
        case TYP_u32:
            switch (res_type->tag)
            {
            case TYP_f32:
                PushInstruction(ctx, routine, IR_S_TO_F32, res, oper_res);
                break;
            case TYP_f64:
                PushInstruction(ctx, routine, IR_S_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(ctx, routine, IR_Mov, res, oper_res);
            }
            break;
        case TYP_s32:
            switch (res_type->tag)
            {
            case TYP_f32:
                PushInstruction(ctx, routine, IR_S_TO_F32, res, oper_res);
                break;
            case TYP_f64:
                PushInstruction(ctx, routine, IR_S_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(ctx, routine, IR_Mov, res, oper_res);
            }
            break;
        case TYP_u64:
            switch (res_type->tag)
            {
            case TYP_f32:
                PushInstruction(ctx, routine, IR_S_TO_F32, res, oper_res);
                break;
            case TYP_f64:
                PushInstruction(ctx, routine, IR_S_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(ctx, routine, IR_Mov, res, oper_res);
            }
            break;
        case TYP_s64:
            switch (res_type->tag)
            {
            case TYP_f32:
                PushInstruction(ctx, routine, IR_S_TO_F32, res, oper_res);
                break;
            case TYP_f64:
                PushInstruction(ctx, routine, IR_S_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(ctx, routine, IR_Mov, res, oper_res);
            }
            break;
        case TYP_f32:
            switch (res_type->tag)
            {
            case TYP_pointer:
            case TYP_bool:
                INVALID_CODE_PATH;
                break;
            case TYP_u8:
            case TYP_u16:
            case TYP_u32:
            case TYP_u64:
                PushInstruction(ctx, routine, IR_F32_TO_S, res, oper_res);
                break;
            case TYP_s8:
            case TYP_s16:
            case TYP_s32:
            case TYP_s64:
                PushInstruction(ctx, routine, IR_F32_TO_S, res, oper_res);
                break;
            case TYP_f32:
                PushInstruction(ctx, routine, IR_Mov, res, oper_res);
                break;
            case TYP_f64:
                PushInstruction(ctx, routine, IR_F32_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(ctx, routine, IR_Mov, res, oper_res);
            }
            break;
        case TYP_f64:
            switch (res_type->tag)
            {
            case TYP_pointer:
            case TYP_bool:
                INVALID_CODE_PATH;
                break;
            case TYP_u8:
            case TYP_u16:
            case TYP_u32:
            case TYP_u64:
                PushInstruction(ctx, routine, IR_F64_TO_S, res, oper_res);
                break;
            case TYP_s8:
            case TYP_s16:
            case TYP_s32:
            case TYP_s64:
                PushInstruction(ctx, routine, IR_F64_TO_S, res, oper_res);
                break;
            case TYP_f32:
                PushInstruction(ctx, routine, IR_F64_TO_F32, res, oper_res);
                break;
            case TYP_f64:
                PushInstruction(ctx, routine, IR_Mov, res, oper_res);
                break;
            default:
                PushInstruction(ctx, routine, IR_Mov, res, oper_res);
            }
            break;
    }
    return res;
}

static Ir_Operand GenAccessExpr(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    Ast_Expr *base_expr = expr->access_expr.base;
    Ast_Expr *member_expr = expr->access_expr.member;
    Type *base_type = base_expr->expr_type;
    Type *member_type = member_expr->expr_type;

    Name member_name = member_expr->variable_ref.name;
    s64 member_index = 0;
    for (; member_index < base_type->struct_type.member_count; member_index++)
    {
        Struct_Member *member = &base_type->struct_type.members[member_index];
        if (member->name == member_name)
        {
            break;
        }
    }

    Ir_Operand base_res = GenExpression(ctx, base_expr, routine);
    Ir_Operand member_res = NewTemp(ctx, routine, member_type);
    Ir_Operand member_offs = NewImmediateOffset(routine, member_index);
    PushInstruction(ctx, routine, IR_MovMember, member_res, base_res, member_offs);
    return member_res;
}

static Ir_Operand GenSubscriptExpr(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    Ast_Expr *base_expr = expr->subscript_expr.base;
    Ast_Expr *index_expr = expr->subscript_expr.index;

    Ir_Operand base_res = GenExpression(ctx, base_expr, routine);
    Ir_Operand index_res = GenExpression(ctx, index_expr, routine);
    Ir_Operand elem_res = NewTemp(ctx, routine, expr->expr_type);
    PushInstruction(ctx, routine, IR_MovElement, elem_res, base_res, index_res);
    return elem_res;
}

static Ir_Operand GenTernaryExpr(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    Ast_Expr *cond_expr = expr->ternary_expr.cond_expr;
    Ast_Expr *true_expr = expr->ternary_expr.true_expr;
    Ast_Expr *false_expr = expr->ternary_expr.false_expr;

    Ir_Operand false_label = NewLabel(ctx);
    Ir_Operand ternary_end = NewLabel(ctx);
    Ir_Operand res = NewTemp(ctx, routine, expr->expr_type);

    Ir_Operand cond_res = GenExpression(ctx, cond_expr, routine);
    PushJump(ctx, routine, IR_Jz, false_label, cond_res);

    Ir_Operand true_res = GenExpression(ctx, true_expr, routine);
    PushInstruction(ctx, routine, IR_Mov, res, true_res);
    PushJump(ctx, routine, IR_Jump, ternary_end);

    SetLabelTarget(routine, false_label);
    Ir_Operand false_res = GenExpression(ctx, false_expr, routine);
    PushInstruction(ctx, routine, IR_Mov, res, false_res);

    SetLabelTarget(routine, ternary_end);
    return res;
}

static Ir_Operand GenUnaryExpr(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    Unary_Op op = expr->unary_expr.op;
    Ast_Expr *oper_expr = expr->unary_expr.expr;
    Ir_Operand oper = GenExpression(ctx, oper_expr, routine);
    Ir_Operand target = NewTemp(ctx, routine, expr->expr_type);
    switch (op)
    {
        case UN_OP_Positive:
            PushInstruction(ctx, routine, IR_Mov, target, oper);
            break;
        case UN_OP_Negative:
            PushInstruction(ctx, routine, IR_Neg, target, oper);
            break;
        case UN_OP_Not:
            PushInstruction(ctx, routine, IR_Not, target, oper);
            break;
        case UN_OP_Complement:
            PushInstruction(ctx, routine, IR_Compl, target, oper);
            break;
        case UN_OP_Address:
            PushInstruction(ctx, routine, IR_Addr, target, oper);
            break;
        case UN_OP_Deref:
            PushInstruction(ctx, routine, IR_Deref, target, oper);
            break;
    }
    return target;
}

static Ir_Operand GenBinaryExpr(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    Binary_Op op = expr->binary_expr.op;
    Ast_Expr *left = expr->binary_expr.left;
    Ast_Expr *right = expr->binary_expr.right;

    switch (op)
    {
        case BIN_OP_And:
            {
                Ir_Operand and_end = NewLabel(ctx);
                Ir_Operand target = NewTemp(ctx, routine, expr->expr_type);

                Ir_Operand loper = GenExpression(ctx, left, routine);
                PushInstruction(ctx, routine, IR_Mov, target, loper);
                PushJump(ctx, routine, IR_Jz, and_end, loper);

                Ir_Operand roper = GenExpression(ctx, left, routine);
                PushInstruction(ctx, routine, IR_Mov, target, roper);

                SetLabelTarget(routine, and_end);
                return target;
            }
        case BIN_OP_Or:
            {
                Ir_Operand or_end = NewLabel(ctx);
                Ir_Operand target = NewTemp(ctx, routine, expr->expr_type);

                Ir_Operand loper = GenExpression(ctx, left, routine);
                PushInstruction(ctx, routine, IR_Mov, target, loper);
                PushJump(ctx, routine, IR_Jnz, or_end, loper);

                Ir_Operand roper = GenExpression(ctx, left, routine);
                PushInstruction(ctx, routine, IR_Mov, target, roper);

                SetLabelTarget(routine, or_end);
                return target;
            }
        default:
            break;
    }

    Ir_Operand loper = GenExpression(ctx, left, routine);
    Ir_Operand roper = GenExpression(ctx, right, routine);
    Ir_Operand target = NewTemp(ctx, routine, expr->expr_type);
    switch (op)
    {
        case BIN_OP_Add:
            PushInstruction(ctx, routine, IR_Add, target, loper, roper);
            break;
        case BIN_OP_Subtract:
            PushInstruction(ctx, routine, IR_Sub, target, loper, roper);
            break;
        case BIN_OP_Multiply:
            PushInstruction(ctx, routine, IR_Mul, target, loper, roper);
            break;
        case BIN_OP_Divide:
            PushInstruction(ctx, routine, IR_Div, target, loper, roper);
            break;
        case BIN_OP_Modulo:
            PushInstruction(ctx, routine, IR_Mod, target, loper, roper);
            break;
        case BIN_OP_BitAnd:
            PushInstruction(ctx, routine, IR_And, target, loper, roper);
            break;
        case BIN_OP_BitOr:
            PushInstruction(ctx, routine, IR_Or, target, loper, roper);
            break;
        case BIN_OP_BitXor:
            PushInstruction(ctx, routine, IR_Xor, target, loper, roper);
            break;
        case BIN_OP_And:
            INVALID_CODE_PATH;
            break;
        case BIN_OP_Or:
            INVALID_CODE_PATH;
            break;
        case BIN_OP_Equal:
            PushInstruction(ctx, routine, IR_Eq, target, loper, roper);
            break;
        case BIN_OP_NotEqual:
            PushInstruction(ctx, routine, IR_Neq, target, loper, roper);
            break;
        case BIN_OP_Less:
            PushInstruction(ctx, routine, IR_Lt, target, loper, roper);
            break;
        case BIN_OP_LessEq:
            PushInstruction(ctx, routine, IR_Leq, target, loper, roper);
            break;
        case BIN_OP_Greater:
            PushInstruction(ctx, routine, IR_Gt, target, loper, roper);
            break;
        case BIN_OP_GreaterEq:
            PushInstruction(ctx, routine, IR_Geq, target, loper, roper);
            break;
        case BIN_OP_Range:
            NOT_IMPLEMENTED("IR gen for range op");
            INVALID_CODE_PATH;
            break;
    }
    return target;
}

static Ir_Operand GenAssignmentExpr(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    Assignment_Op op = expr->assignment.op;
    Ast_Expr *left = expr->assignment.left;
    Ast_Expr *right = expr->assignment.right;
    Ir_Operand loper = GenExpression(ctx, left, routine);
    Ir_Operand roper = GenExpression(ctx, right, routine);
    switch (op)
    {
        case AS_OP_Assign:
            PushInstruction(ctx, routine, IR_Mov, loper, roper);
            break;
        case AS_OP_AddAssign:
            PushInstruction(ctx, routine, IR_Add, loper, loper, roper);
            break;
        case AS_OP_SubtractAssign:
            PushInstruction(ctx, routine, IR_Sub, loper, loper, roper);
            break;
        case AS_OP_MultiplyAssign:
            PushInstruction(ctx, routine, IR_Mul, loper, loper, roper);
            break;
        case AS_OP_DivideAssign:
            PushInstruction(ctx, routine, IR_Div, loper, loper, roper);
            break;
        case AS_OP_ModuloAssign:
            PushInstruction(ctx, routine, IR_Mod, loper, loper, roper);
            break;
        case AS_OP_BitAndAssign:
            PushInstruction(ctx, routine, IR_And, loper, loper, roper);
            break;
        case AS_OP_BitOrAssign:
            PushInstruction(ctx, routine, IR_Or, loper, loper, roper);
            break;
        case AS_OP_BitXorAssign:
            PushInstruction(ctx, routine, IR_Xor, loper, loper, roper);
            break;
    }
    return loper;
}

static Ir_Operand GenVariableRef(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    (void)ctx;
    Symbol *symbol = expr->variable_ref.symbol;
    if (symbol->sym_type == SYM_Function)
    {
        return NewRoutineRef(routine, expr->expr_type, symbol->unique_name);
    }
    else if (symbol->sym_type == SYM_ForeignFunction)
    {
        return NewForeignRoutineRef(routine, expr->expr_type, expr->variable_ref.name);
    }
    else if (symbol->sym_type == SYM_Parameter)
    {
        return NewVariableRef(routine, expr->expr_type, expr->variable_ref.name);
    }
    else if (symbol->sym_type == SYM_Variable)
    {
        return NewVariableRef(routine, expr->expr_type, expr->variable_ref.name);
    }
    else if (symbol->sym_type == SYM_Constant)
    {
        return NewVariableRef(routine, expr->expr_type, expr->variable_ref.name);
    }
    INVALID_CODE_PATH;
    return NoneOperand();
}

static Ir_Operand GenFunctionCall(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    Ir_Operand res = { };
    if (TypeIsVoid(expr->expr_type))
        res = NoneOperand();
    else
        res = NewTemp(ctx, routine, expr->expr_type);

    Ir_Operand fv_res = GenExpression(ctx, expr->function_call.fexpr, routine);

    // NOTE(henrik): Here we link the arguments to a list based on their
    // indices.  This makes it impossible (or impractical) to delete or insert
    // instructions later to the ir instruction list.
    s64 arg_instr_idx = -1;
    for (s64 i = 0; i < expr->function_call.args.count; i++)
    {
        Ast_Expr *arg = array::At(expr->function_call.args, i);
        Ir_Operand arg_res = GenExpression(ctx, arg, routine);

        s64 instr_idx = routine->instructions.count;
        PushInstruction(ctx, routine, IR_Arg, arg_res,
                NewImmediateOffset(routine, arg_instr_idx));
        arg_instr_idx = instr_idx;
    }

    if (fv_res.oper_type == IR_OPER_ForeignRoutine)
    {
        PushInstruction(ctx, routine, IR_CallForeign, res, fv_res,
                NewImmediateOffset(routine, arg_instr_idx));
    }
    else
    {
        PushInstruction(ctx, routine, IR_Call, res, fv_res,
                NewImmediateOffset(routine, arg_instr_idx));
    }

    return res;
}

static Ir_Operand GenExpression(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    (void)ctx;
    switch (expr->type)
    {
        case AST_Null:
            return NewImmediateNull(routine);
        case AST_BoolLiteral:
            return NewImmediate(routine, expr->bool_literal.value);
        case AST_CharLiteral:
            return NewImmediate(routine, (u8)expr->char_literal.value);
        case AST_IntLiteral:
            return NewImmediateInt(routine, expr->int_literal.value, expr->expr_type);
        case AST_Float32Literal:
            return NewImmediate(routine, expr->float32_literal.value);
        case AST_Float64Literal:
            return NewImmediate(routine, expr->float64_literal.value);
        case AST_StringLiteral:
            return NewImmediate(routine, expr->string_literal.value);

        case AST_VariableRef:
            return GenVariableRef(ctx, expr, routine);
        case AST_FunctionCall:
            return GenFunctionCall(ctx, expr, routine);

        case AST_AssignmentExpr:
            return GenAssignmentExpr(ctx, expr, routine);
        case AST_BinaryExpr:
            return GenBinaryExpr(ctx, expr, routine);
        case AST_UnaryExpr:
            return GenUnaryExpr(ctx, expr, routine);
        case AST_TernaryExpr:
            return GenTernaryExpr(ctx, expr, routine);
        case AST_AccessExpr:
            return GenAccessExpr(ctx, expr, routine);
        case AST_SubscriptExpr:
            return GenSubscriptExpr(ctx, expr, routine);
        case AST_TypecastExpr:
            return GenTypecastExpr(ctx, expr, routine);
        default:
            INVALID_CODE_PATH;
    }
    return NewImmediateNull(routine);
}

static void GenIr(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine);

static void GenIfStatement(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    ExtractComment(ctx, node);

    Ir_Operand if_false_label = NewLabel(ctx);

    Ir_Operand cond_res = GenExpression(ctx, node->if_stmt.cond_expr, routine);
    PushJump(ctx, routine, IR_Jz, if_false_label, cond_res);

    GenIr(ctx, node->if_stmt.then_stmt, routine);

    if (node->if_stmt.else_stmt)
    {
        Ir_Operand else_end_label = NewLabel(ctx);
        PushJump(ctx, routine, IR_Jump, else_end_label);

        SetLabelTarget(routine, if_false_label);

        GenIr(ctx, node->if_stmt.else_stmt, routine);

        SetLabelTarget(routine, else_end_label);
    }
    else
    {
        SetLabelTarget(routine, if_false_label);
    }
}

static void GenWhileStmt(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    ExtractComment(ctx, node);

    Ir_Operand while_end_label = NewLabel(ctx);

    Ir_Operand while_start_label = NewLabel(ctx);
    SetLabelTarget(routine, while_start_label);

    Ir_Operand cond_res = GenExpression(ctx, node->while_stmt.cond_expr, routine);
    PushJump(ctx, routine, IR_Jz, while_end_label, cond_res);

    GenIr(ctx, node->while_stmt.loop_stmt, routine);

    PushJump(ctx, routine, IR_Jump, while_start_label);

    SetLabelTarget(routine, while_end_label);
}

static void GenReturnStmt(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    ExtractComment(ctx, node);

    Ir_Operand res_expr = NoneOperand();
    if (node->return_stmt.expr)
    {
        res_expr = GenExpression(ctx, node->return_stmt.expr, routine);
    }
    PushInstruction(ctx, routine, IR_Return, res_expr);
}

static void GenBlockStatement(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    Ast_Node_List *statements = &node->block_stmt.statements;
    for (s64 i = 0; i < statements->count && ContinueGen(ctx); i++)
    {
        GenIr(ctx, array::At(*statements, i), routine);
    }
}

static void GenFunction(Ir_Gen_Context *ctx, Ast_Node *node)
{
    Symbol *symbol = node->function.symbol;
    s64 arg_count = symbol->type->function_type.parameter_count;
    Ir_Routine *func_routine = PushRoutine(ctx, symbol->unique_name, arg_count);
    for (s64 i = 0; i < arg_count; i++)
    {
        Ast_Node *param_node = array::At(node->function.parameters, i);
        Type *type = symbol->type->function_type.parameter_types[i];
        func_routine->args[i] = NewVariableRef(func_routine, type, param_node->parameter.name);
    }
    GenIr(ctx, node->function.body, func_routine);
}

static void GenVariable(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    ExtractComment(ctx, node);

    Symbol *symbol = node->variable_decl.symbol;
    Ast_Expr *init_expr = node->variable_decl.init_expr;

    if (init_expr)
    {
        Ir_Operand init_res = GenExpression(ctx, init_expr, routine);
        Ir_Operand var_oper = NewVariableRef(routine, init_expr->expr_type, symbol->name);
        PushInstruction(ctx, routine, IR_Mov, var_oper, init_res);
    }
}

static void GenIr(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    switch (node->type)
    {
        case AST_TopLevel: INVALID_CODE_PATH; break;

        case AST_Import: break;
        case AST_ForeignBlock: break;

        case AST_VariableDecl:
            GenVariable(ctx, node, routine);
            break;
        case AST_FunctionDef:
            GenFunction(ctx, node);
            break;

        case AST_StructDef: break;
        case AST_Typealias: break;

        case AST_Parameter:
            INVALID_CODE_PATH;
            break;

        case AST_Type_Plain:
            INVALID_CODE_PATH;
            break;
        case AST_Type_Pointer:
            INVALID_CODE_PATH;
            break;
        case AST_Type_Array:
            INVALID_CODE_PATH;
            break;
        case AST_Type_Function:
            INVALID_CODE_PATH;
            break;

        case AST_StructMember:
            INVALID_CODE_PATH;
            break;

        case AST_BlockStmt:
            GenBlockStatement(ctx, node, routine);
            break;
        case AST_IfStmt:
            GenIfStatement(ctx, node, routine);
            break;
        case AST_WhileStmt:
            GenWhileStmt(ctx, node, routine);
            break;
        case AST_ForStmt:
            NOT_IMPLEMENTED("IR gen for AST_ForStmt");
            break;
        case AST_RangeForStmt:
            NOT_IMPLEMENTED("IR gen for AST_RangeForStmt");
            break;
        case AST_ReturnStmt:
            GenReturnStmt(ctx, node, routine);
            break;
        case AST_ExpressionStmt:
            ExtractComment(ctx, node);
            GenExpression(ctx, node->expr_stmt.expr, routine);
            break;
    }
}

static void GenIrAst(Ir_Gen_Context *ctx, Ast *ast, Ir_Routine *routine)
{
    Ast_Node *root = ast->root;
    ASSERT(root);

    Ast_Node_List *statements = &root->top_level.statements;
    for (s64 index = 0;
        index < statements->count && ContinueGen(ctx);
        index++)
    {
        Ast_Node *node = array::At(*statements, index);
        GenIr(ctx, node, routine);
    }
}

b32 GenIr(Ir_Gen_Context *ctx)
{
    Name top_level_name = PushName(&ctx->arena, "@toplevel");
    Ir_Routine *top_level_routine = PushRoutine(ctx, top_level_name, 0);
    Module_List modules = ctx->comp_ctx->modules;
    for (s64 index = 0;
         index < modules.count && ContinueGen(ctx);
         index++)
    {
        Module *module = array::At(modules, index);
        GenIrAst(ctx, &module->ast, top_level_routine);
    }
    return HasError(ctx->comp_ctx);
}


// IR printing

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

static s64 PrintName(FILE *file, Name name, s64 max_len)
{
    return PrintString(file, name.str, max_len);
}

static void PrintPadding(FILE *file, s64 len, s64 min_len)
{
    while (len < min_len)
    {
        fputc(' ', file);
        len++;
    }
}

static void PrintOpcode(FILE *file, Ir_Opcode opcode)
{
    ASSERT((s64)opcode < IR_COUNT);
    s64 len = fprintf(file, "%s", ir_opcode_names[opcode]);
    PrintPadding(file, len, 16);
}

static s64 PrintPtr(FILE *file, void *ptr)
{
    if (ptr)
        return fprintf(file, "%p", ptr);
    return fprintf(file, "(null)");
}

static s64 PrintBool(FILE *file, bool value)
{
    return fprintf(file, "%s", (value ? "(true)" : "(false)"));
}

static s64 PrintImmediate(FILE *file, Ir_Operand oper)
{
    s64 len = 0;
    switch (oper.type->tag)
    {
        case TYP_none:
        case TYP_pending:
        case TYP_null:
        case TYP_void:
            INVALID_CODE_PATH;
            break;

        case TYP_pointer:   len = PrintPtr(file, oper.imm_ptr); break;
        case TYP_bool:      len = PrintBool(file, oper.imm_bool); break;
        case TYP_char:      len = fprintf(file, "'%c'", oper.imm_u8); break;
        case TYP_u8:        len = fprintf(file, "%u", oper.imm_u8); break;
        case TYP_s8:        len = fprintf(file, "%d", oper.imm_s8); break;
        case TYP_u16:       len = fprintf(file, "%u", oper.imm_u16); break;
        case TYP_s16:       len = fprintf(file, "%d", oper.imm_s16); break;
        case TYP_u32:       len = fprintf(file, "%u", oper.imm_u32); break;
        case TYP_s32:       len = fprintf(file, "%d", oper.imm_s32); break;
        case TYP_u64:       len = fprintf(file, "%" PRIu64, oper.imm_u64); break;
        case TYP_s64:       len = fprintf(file, "%" PRId64, oper.imm_s64); break;
        case TYP_f32:       len = fprintf(file, "%ff", oper.imm_f32); break;
        case TYP_f64:       len = fprintf(file, "%fd", oper.imm_f64); break;
        case TYP_string:
            len += fprintf(file, "\"");
            len += PrintString(file, oper.imm_str, 16);
            len += fprintf(file, "\"");
            break;

        case TYP_Struct:
        case TYP_Function:
            INVALID_CODE_PATH;
            break;
    }
    return len;
}

static s64 PrintLabel(FILE *file, Ir_Operand label_oper)
{
    return fprintf(file, "L:%" PRId64, label_oper.label->target_loc);
}

static void PrintOperand(FILE *file, Ir_Operand oper)
{
    s64 len = 0;
    switch (oper.oper_type)
    {
        case IR_OPER_None:
            len = fprintf(file, "_");
            break;
        case IR_OPER_Variable:
            len = PrintName(file, oper.var.name, 17);
            break;
        case IR_OPER_Temp:
            len = PrintName(file, oper.temp.name, 17);
            break;
        case IR_OPER_Immediate:
            len = PrintImmediate(file, oper);
            break;
        case IR_OPER_Label:
            len = PrintLabel(file, oper);
            break;
        case IR_OPER_Routine:
        case IR_OPER_ForeignRoutine:
            len += fprintf(file, "<");
            //len += PrintName(file, oper.routine->name, 15);
            len += PrintName(file, oper.var.name, 15);
            len += fprintf(file, ">");
            break;
    }
    PrintPadding(file, len, 20);
}

static void PrintComment(FILE *file, Ir_Comment comment)
{
    if (comment.start)
    {
        fprintf(file, "// ");
        fwrite(comment.start, 1, comment.end - comment.start, file);
    }
}

static void PrintInstruction(FILE *file, Ir_Instruction instr)
{
    PrintOpcode(file, instr.opcode);
    fprintf(file, "  ");
    PrintOperand(file, instr.target);
    fprintf(file, "  ");
    PrintOperand(file, instr.oper1);
    fprintf(file, "  ");
    PrintOperand(file, instr.oper2);
    PrintComment(file, instr.comment);
    fprintf(file, "\n");
}

static void PrintRoutine(FILE *file, Ir_Routine *routine)
{
    PrintName(file, routine->name, 80);
    fprintf(file, ":\n");
    for (s64 i = 0; i < routine->instructions.count; i++)
    {
        fprintf(file, "%" PRId64 ":\t", i);
        PrintInstruction(file, array::At(routine->instructions, i));
    }
}

void PrintIr(IoFile *file, Ir_Gen_Context *ctx)
{
    for (s64 i = 0; i < ctx->routines.count; i++)
    {
        PrintRoutine((FILE*)file, array::At(ctx->routines, i));
    }
}

} // hplang
