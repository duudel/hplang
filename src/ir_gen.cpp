
#include "ir_gen.h"
#include "compiler.h"
#include "symbols.h"
#include "ast_types.h"
#include "assert.h"

#include <cinttypes> // For printf formats

namespace hplang
{

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

static Ir_Routine* PushRoutine(Ir_Gen_Context *ctx, Name name)
{
    Ir_Routine *routine = PushStruct<Ir_Routine>(&ctx->arena);
    *routine = { };
    routine->name = name;
    array::Push(ctx->routines, routine);
    return routine;
}

Ir_Type GetIrType(Type *type)
{
    switch (type->tag)
    {
        case TYP_pointer:   return IR_TYP_ptr;
        case TYP_bool:      return IR_TYP_bool;
        case TYP_char:      return IR_TYP_u8;
        case TYP_u8:        return IR_TYP_u8;
        case TYP_s8:        return IR_TYP_s8;
        case TYP_u16:       return IR_TYP_u16;
        case TYP_s16:       return IR_TYP_s16;
        case TYP_u32:       return IR_TYP_u32;
        case TYP_s32:       return IR_TYP_s32;
        case TYP_u64:       return IR_TYP_u64;
        case TYP_s64:       return IR_TYP_s64;
        case TYP_f32:       return IR_TYP_f32;
        case TYP_f64:       return IR_TYP_f64;
        case TYP_string:    return IR_TYP_str;
        case TYP_Function:  return IR_TYP_ptr;
        default:
                            break;
    }
    fprintf(stderr, "\ntype->tag = %d\n", type->tag);
    INVALID_CODE_PATH;
    return IR_TYP_ptr;
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
    oper.type = IR_TYP_ptr;
    oper.imm_ptr = nullptr;
    return oper;
}

#define IR_IMM(t, ir_t, member)\
static Ir_Operand NewImmediate(Ir_Routine *routine, t value)\
{\
    (void)routine;\
    Ir_Operand oper = { };\
    oper.oper_type = IR_OPER_Immediate;\
    oper.type = ir_t;\
    oper.member = value;\
    return oper;\
}

//IR_IMM(void*, IR_TYP_ptr, imm_ptr)
IR_IMM(bool, IR_TYP_bool, imm_bool)
IR_IMM(u8, IR_TYP_u8, imm_u8)
//IR_IMM(s8, IR_TYP_s8, imm_s8)
//IR_IMM(u16, IR_TYP_u16, imm_u16)
//IR_IMM(s16, IR_TYP_s16, imm_s16)
//IR_IMM(u32, IR_TYP_u32, imm_u32)
//IR_IMM(s32, IR_TYP_s32, imm_s32)
//IR_IMM(u64, IR_TYP_u64, imm_u64)
//IR_IMM(s64, IR_TYP_s64, imm_s64)
IR_IMM(f32, IR_TYP_f32, imm_f32)
IR_IMM(f64, IR_TYP_f64, imm_f64)
IR_IMM(String, IR_TYP_str, imm_str)

#undef IR_IMM

static Ir_Operand NewImmediateInt(Ir_Routine *routine, u64 value, Type *type)
{
    (void)routine;
    Ir_Operand oper = { };
    oper.oper_type = IR_OPER_Immediate;
    oper.type = GetIrType(type);
    oper.imm_u64 = value;
    return oper;
}

static Ir_Operand NewVariable(Ir_Routine *routine, Ir_Type type, Name name)
{
    (void)routine;
    Ir_Operand oper = { };
    oper.oper_type = IR_OPER_Variable;
    oper.type = type;
    oper.var.name = name;
    return oper;
}

static Ir_Operand NewTemp(Ir_Routine *routine, Ir_Type type)
{
    Ir_Operand oper = { };
    oper.oper_type = IR_OPER_Temp;
    oper.type = type;
    oper.temp.temp_id = routine->temp_count++;
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

static void PushInstruction(Ir_Routine *routine, Ir_Opcode opcode,
        Ir_Operand target = NoneOperand(),
        Ir_Operand oper1 = NoneOperand(),
        Ir_Operand oper2 = NoneOperand())
{
    Ir_Instruction instr = { };
    instr.opcode = opcode;
    instr.target = target;
    instr.oper1 = oper1;
    instr.oper2 = oper2;
    array::Push(routine->instructions, instr);
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

    Ir_Type oper_type = GetIrType(oper_expr->expr_type);
    Ir_Type res_type = GetIrType(expr->expr_type);
    Ir_Operand res = NewTemp(routine, res_type);
    switch (oper_type)
    {
        case IR_TYP_str:
            INVALID_CODE_PATH;
            break;
        case IR_TYP_ptr:
            PushInstruction(routine, IR_Mov, res, oper_res);
            break;
        case IR_TYP_bool:
            PushInstruction(routine, IR_Mov, res, oper_res);
            break;
        case IR_TYP_u8:
            switch (res_type)
            {
            case IR_TYP_f32:
                PushInstruction(routine, IR_U_TO_F32, res, oper_res);
                break;
            case IR_TYP_f64:
                PushInstruction(routine, IR_U_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(routine, IR_Mov, res, oper_res);
            }
            break;
        case IR_TYP_s8:
            switch (res_type)
            {
            case IR_TYP_f32:
                PushInstruction(routine, IR_S_TO_F32, res, oper_res);
                break;
            case IR_TYP_f64:
                PushInstruction(routine, IR_S_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(routine, IR_Mov, res, oper_res);
            }
            break;
        case IR_TYP_u16:
            switch (res_type)
            {
            case IR_TYP_f32:
                PushInstruction(routine, IR_U_TO_F32, res, oper_res);
                break;
            case IR_TYP_f64:
                PushInstruction(routine, IR_U_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(routine, IR_Mov, res, oper_res);
            }
            break;
        case IR_TYP_s16:
            switch (res_type)
            {
            case IR_TYP_f32:
                PushInstruction(routine, IR_S_TO_F32, res, oper_res);
                break;
            case IR_TYP_f64:
                PushInstruction(routine, IR_S_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(routine, IR_Mov, res, oper_res);
            }
            break;
        case IR_TYP_u32:
            switch (res_type)
            {
            case IR_TYP_f32:
                PushInstruction(routine, IR_U_TO_F32, res, oper_res);
                break;
            case IR_TYP_f64:
                PushInstruction(routine, IR_U_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(routine, IR_Mov, res, oper_res);
            }
            break;
        case IR_TYP_s32:
            switch (res_type)
            {
            case IR_TYP_f32:
                PushInstruction(routine, IR_S_TO_F32, res, oper_res);
                break;
            case IR_TYP_f64:
                PushInstruction(routine, IR_S_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(routine, IR_Mov, res, oper_res);
            }
            break;
        case IR_TYP_u64:
            switch (res_type)
            {
            case IR_TYP_f32:
                PushInstruction(routine, IR_U_TO_F32, res, oper_res);
                break;
            case IR_TYP_f64:
                PushInstruction(routine, IR_U_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(routine, IR_Mov, res, oper_res);
            }
            break;
        case IR_TYP_s64:
            switch (res_type)
            {
            case IR_TYP_f32:
                PushInstruction(routine, IR_S_TO_F32, res, oper_res);
                break;
            case IR_TYP_f64:
                PushInstruction(routine, IR_S_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(routine, IR_Mov, res, oper_res);
            }
            break;
        case IR_TYP_f32:
            switch (res_type)
            {
            case IR_TYP_ptr:
            case IR_TYP_bool:
                INVALID_CODE_PATH;
                break;
            case IR_TYP_u8:
            case IR_TYP_u16:
            case IR_TYP_u32:
            case IR_TYP_u64:
                PushInstruction(routine, IR_U_TO_F32, res, oper_res);
                break;
            case IR_TYP_s8:
            case IR_TYP_s16:
            case IR_TYP_s32:
            case IR_TYP_s64:
                PushInstruction(routine, IR_S_TO_F32, res, oper_res);
                break;
            case IR_TYP_f32:
                PushInstruction(routine, IR_Mov, res, oper_res);
                break;
            case IR_TYP_f64:
                PushInstruction(routine, IR_F32_TO_F64, res, oper_res);
                break;
            default:
                PushInstruction(routine, IR_Mov, res, oper_res);
            }
            break;
        case IR_TYP_f64:
            switch (res_type)
            {
            case IR_TYP_ptr:
            case IR_TYP_bool:
                INVALID_CODE_PATH;
                break;
            case IR_TYP_u8:
            case IR_TYP_u16:
            case IR_TYP_u32:
            case IR_TYP_u64:
                PushInstruction(routine, IR_U_TO_F64, res, oper_res);
                break;
            case IR_TYP_s8:
            case IR_TYP_s16:
            case IR_TYP_s32:
            case IR_TYP_s64:
                PushInstruction(routine, IR_S_TO_F64, res, oper_res);
                break;
            case IR_TYP_f32:
                PushInstruction(routine, IR_F64_TO_F32, res, oper_res);
                break;
            case IR_TYP_f64:
                PushInstruction(routine, IR_Mov, res, oper_res);
                break;
            default:
                PushInstruction(routine, IR_Mov, res, oper_res);
            }
            break;
    }
    return res;
}

static Ir_Operand GenTernaryExpr(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    Ast_Expr *cond_expr = expr->ternary_expr.cond_expr;
    Ast_Expr *true_expr = expr->ternary_expr.true_expr;
    Ast_Expr *false_expr = expr->ternary_expr.false_expr;

    Ir_Operand false_label = NewLabel(ctx);
    Ir_Operand ternary_end = NewLabel(ctx);
    Ir_Operand res = NewTemp(routine, GetIrType(expr->expr_type));

    Ir_Operand cond_res = GenExpression(ctx, cond_expr, routine);
    PushInstruction(routine, IR_Jz, false_label, cond_res);

    Ir_Operand true_res = GenExpression(ctx, true_expr, routine);
    PushInstruction(routine, IR_Mov, res, true_res);
    PushInstruction(routine, IR_Jump, ternary_end);

    SetLabelTarget(routine, false_label);
    Ir_Operand false_res = GenExpression(ctx, false_expr, routine);
    PushInstruction(routine, IR_Mov, res, false_res);

    SetLabelTarget(routine, ternary_end);
    return res;
}

static Ir_Operand GenUnaryExpr(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    Unary_Op op = expr->unary_expr.op;
    Ast_Expr *oper_expr = expr->unary_expr.expr;
    Ir_Operand oper = GenExpression(ctx, oper_expr, routine);
    Ir_Operand target = NewTemp(routine, GetIrType(expr->expr_type));
    switch (op)
    {
        case UN_OP_Positive:
            PushInstruction(routine, IR_Mov, target, oper);
            break;
        case UN_OP_Negative:
            PushInstruction(routine, IR_Neg, target, oper);
            break;
        case UN_OP_Not:
            PushInstruction(routine, IR_Not, target, oper);
            break;
        case UN_OP_Complement:
            PushInstruction(routine, IR_Compl, target, oper);
            break;
        case UN_OP_Address:
            PushInstruction(routine, IR_Addr, target, oper);
            break;
        case UN_OP_Deref:
            PushInstruction(routine, IR_Deref, target, oper);
            break;
    }
    return target;
}

static Ir_Operand GenBinaryExpr(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    Binary_Op op = expr->binary_expr.op;
    Ast_Expr *left = expr->binary_expr.left;
    Ast_Expr *right = expr->binary_expr.right;
    Ir_Operand loper = GenExpression(ctx, left, routine);
    Ir_Operand roper = GenExpression(ctx, right, routine);
    Ir_Operand target = NewTemp(routine, GetIrType(expr->expr_type));
    switch (op)
    {
        case BIN_OP_Add:
            PushInstruction(routine, IR_Add, target, loper, roper);
            break;
        case BIN_OP_Subtract:
            PushInstruction(routine, IR_Sub, target, loper, roper);
            break;
        case BIN_OP_Multiply:
            PushInstruction(routine, IR_Mul, target, loper, roper);
            break;
        case BIN_OP_Divide:
            PushInstruction(routine, IR_Div, target, loper, roper);
            break;
        case BIN_OP_Modulo:
            PushInstruction(routine, IR_Mod, target, loper, roper);
            break;
        case BIN_OP_BitAnd:
            PushInstruction(routine, IR_And, target, loper, roper);
            break;
        case BIN_OP_BitOr:
            PushInstruction(routine, IR_Or, target, loper, roper);
            break;
        case BIN_OP_BitXor:
            PushInstruction(routine, IR_Xor, target, loper, roper);
            break;
        case BIN_OP_And:
            NOT_IMPLEMENTED("IR gen for logical and");
            break;
        case BIN_OP_Or:
            NOT_IMPLEMENTED("IR gen for logical or");
            break;
        case BIN_OP_Equal:
            PushInstruction(routine, IR_Eq, target, loper, roper);
            break;
        case BIN_OP_NotEqual:
            PushInstruction(routine, IR_Neq, target, loper, roper);
            break;
        case BIN_OP_Less:
            PushInstruction(routine, IR_Lt, target, loper, roper);
            break;
        case BIN_OP_LessEq:
            PushInstruction(routine, IR_Leq, target, loper, roper);
            break;
        case BIN_OP_Greater:
            PushInstruction(routine, IR_Gt, target, loper, roper);
            break;
        case BIN_OP_GreaterEq:
            PushInstruction(routine, IR_Geq, target, loper, roper);
            break;
        case BIN_OP_Range:
            NOT_IMPLEMENTED("IR gen for range op");
            INVALID_CODE_PATH;
            break;
        case BIN_OP_Subscript:
            NOT_IMPLEMENTED("IR gen for subscript");
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
            PushInstruction(routine, IR_Mov, loper, roper);
            break;
        case AS_OP_AddAssign:
            PushInstruction(routine, IR_Add, loper, loper, roper);
            break;
        case AS_OP_SubtractAssign:
            PushInstruction(routine, IR_Sub, loper, loper, roper);
            break;
        case AS_OP_MultiplyAssign:
            PushInstruction(routine, IR_Mul, loper, loper, roper);
            break;
        case AS_OP_DivideAssign:
            PushInstruction(routine, IR_Div, loper, loper, roper);
            break;
        case AS_OP_ModuloAssign:
            PushInstruction(routine, IR_Mod, loper, loper, roper);
            break;
        case AS_OP_BitAndAssign:
            PushInstruction(routine, IR_And, loper, loper, roper);
            break;
        case AS_OP_BitOrAssign:
            PushInstruction(routine, IR_Or, loper, loper, roper);
            break;
        case AS_OP_BitXorAssign:
            PushInstruction(routine, IR_Xor, loper, loper, roper);
            break;
    }
    return loper;
}

static Ir_Operand GenVariableRef(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    (void)ctx;
    return NewVariable(routine, GetIrType(expr->expr_type), expr->variable_ref.name);
}

static Ir_Operand GenFunctionCall(Ir_Gen_Context *ctx, Ast_Expr *expr, Ir_Routine *routine)
{
    Ir_Operand res = { };
    if (TypeIsVoid(expr->expr_type))
        res = NoneOperand();
    else
        res = NewTemp(routine, GetIrType(expr->expr_type));

    for (s64 i = 0; i < expr->function_call.args.count; i++)
    {
        Ast_Expr *arg = array::At(expr->function_call.args, i);
        Ir_Operand arg_res = GenExpression(ctx, arg, routine);
        PushInstruction(routine, IR_Param, arg_res);
    }

    Ir_Operand fv_res = GenExpression(ctx, expr->function_call.fexpr, routine);
    PushInstruction(routine, IR_Call, res, fv_res);

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
            NOT_IMPLEMENTED("IR gen for AST_AccessExpr");
            break;
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
    Ir_Operand if_false_label = NewLabel(ctx);

    Ir_Operand cond_res = GenExpression(ctx, node->if_stmt.cond_expr, routine);
    PushInstruction(routine, IR_Jz, cond_res, if_false_label);

    GenIr(ctx, node->if_stmt.then_stmt, routine);

    if (node->if_stmt.else_stmt)
    {
        Ir_Operand else_end_label = NewLabel(ctx);
        PushInstruction(routine, IR_Jump, else_end_label);

        SetLabelTarget(routine, if_false_label);

        GenIr(ctx, node->if_stmt.else_stmt, routine);

        SetLabelTarget(routine, else_end_label);
    }
    else
    {
        SetLabelTarget(routine, if_false_label);
    }
}

static void GenReturnStmt(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    Ir_Operand res_expr = NoneOperand();
    if (node->return_stmt.expr)
    {
        res_expr = GenExpression(ctx, node->return_stmt.expr, routine);
    }
    PushInstruction(routine, IR_Return, res_expr);
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
    Ir_Routine *func_routine = PushRoutine(ctx, node->function.symbol->unique_name);
    GenIr(ctx, node->function.body, func_routine);
}

static void AddVariable(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    (void)ctx;
    (void)node;
    (void)routine;
    //Symbol *symbol = LookupSymbol(ctx->env, node->variable_decl.name);
}

static void GenIr(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    switch (node->type)
    {
        case AST_TopLevel: INVALID_CODE_PATH; break;

        case AST_Import: break;
        case AST_ForeignBlock: break;

        case AST_VariableDecl:
            AddVariable(ctx, node, routine);
            break;
        case AST_FunctionDef:
            GenFunction(ctx, node);
            break;

        case AST_StructDef: break;

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
            NOT_IMPLEMENTED("IR gen for AST_WhileStmt");
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
    Ir_Routine *top_level_routine = PushRoutine(ctx, top_level_name);
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

static s64 PrintString(FILE *file, String str)
{
    return fwrite(str.data, 1, str.size, file);
}

static s64 PrintName(FILE *file, Name name)
{
    return PrintString(file, name.str);
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
    PrintPadding(file, len, 10);
}

static s64 PrintImmediate(FILE *file, Ir_Operand oper)
{
    s64 len = 0;
    switch (oper.type)
    {
        case IR_TYP_ptr:    len = fprintf(file, "%p", oper.imm_ptr); break;
        case IR_TYP_bool:   len = fprintf(file, "%s", (oper.imm_bool ? "true" : "false")); break;
        case IR_TYP_u8:     len = fprintf(file, "%u", oper.imm_u8); break;
        case IR_TYP_s8:     len = fprintf(file, "%d", oper.imm_s8); break;
        case IR_TYP_u16:    len = fprintf(file, "%u", oper.imm_u16); break;
        case IR_TYP_s16:    len = fprintf(file, "%d", oper.imm_s16); break;
        case IR_TYP_u32:    len = fprintf(file, "%u", oper.imm_u32); break;
        case IR_TYP_s32:    len = fprintf(file, "%d", oper.imm_s32); break;
        case IR_TYP_u64:    len = fprintf(file, "%" PRIu64, oper.imm_u64); break;
        case IR_TYP_s64:    len = fprintf(file, "%" PRId64, oper.imm_s64); break;
        case IR_TYP_f32:    len = fprintf(file, "%ff", oper.imm_f32); break;
        case IR_TYP_f64:    len = fprintf(file, "%fd", oper.imm_f64); break;
        case IR_TYP_str:
            len = fprintf(file, "\"");
            len += PrintString(file, oper.imm_str);
            len += fprintf(file, "\"");
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
            len = PrintName(file, oper.var.name);
            break;
        case IR_OPER_Temp:
            len = fprintf(file, "temp%" PRId64, oper.temp.temp_id);
            break;
        case IR_OPER_Immediate:
            len = PrintImmediate(file, oper);
            break;
        case IR_OPER_Label:
            len = PrintLabel(file, oper);
            break;
    }
    PrintPadding(file, len, 16);
}

static void PrintInstruction(FILE *file, Ir_Instruction instr)
{
    PrintOpcode(file, instr.opcode);
    fprintf(file, "\t");
    PrintOperand(file, instr.target);
    fprintf(file, "\t");
    PrintOperand(file, instr.oper1);
    fprintf(file, "\t");
    PrintOperand(file, instr.oper2);
    fprintf(file, "\n");
}

static void PrintRoutine(FILE *file, Ir_Routine *routine)
{
    PrintName(file, routine->name);
    fprintf(file, ":\n");
    for (s64 i = 0; i < routine->instructions.count; i++)
    {
        fprintf(file, "%" PRId64 ":\t", i);
        PrintInstruction(file, array::At(routine->instructions, i));
    }
}

void PrintIr(FILE *file, Ir_Gen_Context *ctx)
{
    for (s64 i = 0; i < ctx->routines.count; i++)
    {
        PrintRoutine(file, array::At(ctx->routines, i));
    }
}

} // hplang
