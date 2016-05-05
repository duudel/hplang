
#include "ir_gen.h"
#include "compiler.h"
#include "symbols.h"
#include "ast_types.h"
#include "assert.h"

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

static void NewInstruction(Ir_Routine *routine, Ir_Opcode opcode,
        Ir_Operand target, Ir_Operand oper1, Ir_Operand oper2)
{
    Ir_Instruction instr = { };
    instr.opcode = opcode;
    instr.target = target;
    instr.oper1 = oper1;
    instr.oper2 = oper2;
    array::Push(routine->instructions, instr);
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
IR_IMM(s64, IR_TYP_s64, imm_s64)
IR_IMM(f32, IR_TYP_f32, imm_f32)
IR_IMM(f64, IR_TYP_f64, imm_f64)
IR_IMM(String, IR_TYP_str, imm_str)

#undef IR_IMM

static Ir_Operand NewTemp(Ir_Routine *routine, Ir_Type type)
{
    Ir_Operand oper = { };
    oper.oper_type = IR_OPER_Temp;
    oper.type = type;
    oper.temp.temp_id = routine->temp_count++;
    return oper;
}

static void GenIr(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine);

static Ir_Operand GenExpression(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine);


static Ir_Operand GenAssignmentExpr(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    Assignment_Op op = node->expression.assignment.op;
    Ast_Node *left = node->expression.assignment.left;
    Ast_Node *right = node->expression.assignment.right;
    Ir_Operand loper = GenExpression(ctx, left, routine);
    Ir_Operand roper = GenExpression(ctx, right, routine);
    (void)op;
    (void)roper;
    return loper;
}

static Ir_Operand GenExpression(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    (void)ctx;
    switch (node->type)
    {
        case AST_Null:
            return NewImmediateNull(routine);
        case AST_BoolLiteral:
            return NewImmediate(routine, node->expression.bool_literal.value);
        case AST_CharLiteral:
            return NewImmediate(routine, (u8)node->expression.char_literal.value);
        case AST_IntLiteral:
            return NewImmediate(routine, node->expression.int_literal.value);
        case AST_Float32Literal:
            return NewImmediate(routine, node->expression.float32_literal.value);
        case AST_Float64Literal:
            return NewImmediate(routine, node->expression.float64_literal.value);
        case AST_StringLiteral:
            return NewImmediate(routine, node->expression.string_literal.value);

        case AST_VariableRef:
            NOT_IMPLEMENTED("IR gen for AST_VariableRef");
            break;
        case AST_FunctionCall:
            NOT_IMPLEMENTED("IR gen for AST_FunctionCall");
            break;

        case AST_AssignmentExpr:
            return GenAssignmentExpr(ctx, node, routine);
        case AST_BinaryExpr:
            NOT_IMPLEMENTED("IR gen for AST_BinaryExpr");
            break;
        case AST_UnaryExpr:
            NOT_IMPLEMENTED("IR gen for AST_UnaryExpr");
            break;
        case AST_TernaryExpr:
            NOT_IMPLEMENTED("IR gen for AST_TernaryExpr");
            break;
        case AST_AccessExpr:
            NOT_IMPLEMENTED("IR gen for AST_AccessExpr");
            break;
        case AST_TypecastExpr:
            NOT_IMPLEMENTED("IR gen for AST_TypecastExpr");
            break;
        default:
            INVALID_CODE_PATH;
    }
    return NewImmediateNull(routine);
}
static void GenIfStatement(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    (void)ctx;
    (void)node;
    (void)routine;
    NOT_IMPLEMENTED("IR gen for AST_IfStmt");
}

static void GenBlockStatement(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    Ast_Node_List *statements = &node->block.statements;
    for (s64 i = 0; i < statements->count && ContinueGen(ctx); i++)
    {
        GenIr(ctx, statements->nodes[i], routine);
    }
}

static void GenFunction(Ir_Gen_Context *ctx, Ast_Node *node)
{
    Ir_Routine *func_routine = PushRoutine(ctx, node->function.symbol->unique_name);
    GenIr(ctx, node->function.body, func_routine);
}

static void GenIr(Ir_Gen_Context *ctx, Ast_Node *node, Ir_Routine *routine)
{
    switch (node->type)
    {
        case AST_TopLevel: INVALID_CODE_PATH; break;

        case AST_Import: break;
        case AST_ForeignBlock: break;

        case AST_VariableDecl:
            NOT_IMPLEMENTED("IR gen for AST_VariableDecl");
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
        case AST_ForStmt:
            NOT_IMPLEMENTED("IR gen for AST_ForStmt");
            break;
        case AST_WhileStmt:
            NOT_IMPLEMENTED("IR gen for AST_WhileStmt");
            break;
        case AST_ReturnStmt:
            NOT_IMPLEMENTED("IR gen for AST_ReturnStmt");
            break;

        case AST_Null:
            NOT_IMPLEMENTED("IR gen for AST_Null");
            break;
        case AST_BoolLiteral:
            NOT_IMPLEMENTED("IR gen for AST_BoolLiteral");
            break;
        case AST_CharLiteral:
            NOT_IMPLEMENTED("IR gen for AST_CharLiteral");
            break;
        case AST_IntLiteral:
            NOT_IMPLEMENTED("IR gen for AST_IntLiteral");
            break;
        case AST_Float32Literal:
            NOT_IMPLEMENTED("IR gen for AST_Float32Literal");
            break;
        case AST_Float64Literal:
            NOT_IMPLEMENTED("IR gen for AST_Float64Literal");
            break;
        case AST_StringLiteral:
            NOT_IMPLEMENTED("IR gen for AST_StringLiteral");
            break;
        case AST_VariableRef:
            NOT_IMPLEMENTED("IR gen for AST_VariableRef");
            break;
        case AST_FunctionCall:
            NOT_IMPLEMENTED("IR gen for AST_FunctionCall");
            break;

        case AST_AssignmentExpr:
            NOT_IMPLEMENTED("IR gen for AST_AssignmentExpr");
            break;
        case AST_BinaryExpr:
            NOT_IMPLEMENTED("IR gen for AST_BinaryExpr");
            break;
        case AST_UnaryExpr:
            NOT_IMPLEMENTED("IR gen for AST_UnaryExpr");
            break;
        case AST_TernaryExpr:
            NOT_IMPLEMENTED("IR gen for AST_TernaryExpr");
            break;
        case AST_AccessExpr:
            NOT_IMPLEMENTED("IR gen for AST_AccessExpr");
            break;
        case AST_TypecastExpr:
            NOT_IMPLEMENTED("IR gen for AST_TypecastExpr");
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
        Ast_Node *node = statements->nodes[index];
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

static void PrintString(FILE *file, String str)
{
    fwrite(str.data, 1, str.size, file);
}

static void PrintName(FILE *file, Name name)
{
    PrintString(file, name.str);
}

static void PrintOpcode(FILE *file, Ir_Opcode opcode)
{
    ASSERT((s64)opcode < IR_COUNT);
    fprintf(file, "%s", ir_opcode_names[opcode]);
}

static void PrintImmediate(FILE *file, Ir_Operand oper)
{
    switch (oper.type)
    {
        case IR_TYP_ptr:    fprintf(file, "%p", oper.imm_ptr); break;
        case IR_TYP_bool:   fprintf(file, "%s", (oper.imm_bool ? "true" : "false")); break;
        case IR_TYP_u8:     fprintf(file, "%d", oper.imm_u8); break;
        case IR_TYP_s8:     fprintf(file, "%d", oper.imm_s8); break;
        case IR_TYP_u16:    fprintf(file, "%d", oper.imm_u16); break;
        case IR_TYP_s16:    fprintf(file, "%d", oper.imm_s16); break;
        case IR_TYP_u32:    fprintf(file, "%d", oper.imm_u32); break;
        case IR_TYP_s32:    fprintf(file, "%d", oper.imm_s32); break;
        case IR_TYP_u64:    fprintf(file, "%lld", oper.imm_u64); break;
        case IR_TYP_s64:    fprintf(file, "%lld", oper.imm_s64); break;
        case IR_TYP_f32:    fprintf(file, "%ff", oper.imm_f32); break;
        case IR_TYP_f64:    fprintf(file, "%fd", oper.imm_f64); break;
        case IR_TYP_str:
            fprintf(file, "\"");
            PrintString(file, oper.imm_str);
            fprintf(file, "\"");
            break;
    }
}

static void PrintOperand(FILE *file, Ir_Operand oper)
{
    switch (oper.oper_type)
    {
        case IR_OPER_Variable:
            PrintName(file, oper.symbol->name);
            break;
        case IR_OPER_Temp:
            fprintf(file, "temp%lld", oper.temp.temp_id);
            break;
        case IR_OPER_Immediate:
            PrintImmediate(file, oper);
            break;
    }
}

static void PrintInstruction(FILE *file, Ir_Instruction instr)
{
    PrintOpcode(file, instr.opcode);
    fprintf(file, "\t");
    PrintOperand(file, instr.target);
    fprintf(file, ",\t");
    PrintOperand(file, instr.oper1);
    fprintf(file, ",\t");
    PrintOperand(file, instr.oper2);
    fprintf(file, "\n");
}

static void PrintRoutine(FILE *file, Ir_Routine *routine)
{
    PrintName(file, routine->name);
    fprintf(file, ":\n");
    for (s64 i = 0; i < routine->instructions.count; i++)
    {
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
