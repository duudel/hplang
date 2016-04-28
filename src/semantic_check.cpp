
#include "semantic_check.h"
#include "ast_types.h"
#include "compiler.h"
#include "error.h"
#include "compiler_options.h"
#include "assert.h"

namespace hplang
{

Sem_Check_Context NewSemanticCheckContext(
        Ast *ast,
        Open_File *open_file,
        Compiler_Context *comp_ctx)
{
    Sem_Check_Context ctx = { };
    ctx.ast = ast;
    ctx.env = &comp_ctx->env;
    ctx.open_file = open_file;
    ctx.comp_ctx = comp_ctx;
    return ctx;
}

void FreeSemanticCheckContext(Sem_Check_Context *ctx)
{
    FreeMemoryArena(&ctx->temp_arena);
    ctx->ast = nullptr;
}

// Semantic check
// --------------

static b32 ContinueChecking(Sem_Check_Context *ctx)
{
    return ContinueCompiling(ctx->comp_ctx);
}

static void PrintString(FILE *file, String str)
{
    fwrite(str.data, 1, str.size, file);
}

static void Error(Sem_Check_Context *ctx, Ast_Node *node, const char *message)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->file_loc);
    PrintFileLocation(err_ctx->file, node->file_loc);
    fprintf(err_ctx->file, "%s\n", message);
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, node->file_loc);
}

static void ErrorSymbolNotTypename(Sem_Check_Context *ctx, Ast_Node *node, Name name)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->file_loc);
    PrintFileLocation(err_ctx->file, node->file_loc);
    fprintf(err_ctx->file, "Symbol '");
    PrintString(err_ctx->file, name.str);
    fprintf(err_ctx->file, "' is not a typename\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, node->file_loc);
}

static void PrintType(FILE *file, Type *type)
{
    switch (type->tag)
    {
    case TYP_string:
    case TYP_Struct:
        PrintString(file, type->struct_type.name.str);
        break;
    case TYP_Function:
        {
            fprintf(file, "(");
            for (s64 i = 0; i < type->function_type.parameter_count; i++)
            {
                if (i > 0) fprintf(file, ", ");
                PrintType(file, type->function_type.parameter_types[i]);
            }
            fprintf(file, ")");
            fprintf(file, " : ");
            if (type->function_type.return_type)
                PrintType(file, type->function_type.return_type);
            else
                fprintf(file, "*");
        } break;
    case TYP_pointer:
        {
            PrintType(file, type->base_type);
            for (s64 i = 0; i < type->pointer; i++)
                fprintf(file, "*");
        } break;

    // These should not appear in any normal case
    case TYP_null:
        NOT_IMPLEMENTED("null type printing");
        break;
    case TYP_int_lit:
        NOT_IMPLEMENTED("int literal type printing");
        break;

    default:
        if (type->tag <= TYP_LAST_BUILTIN)
        {
            PrintString(file, type->type_name.str);
        }
        else
        {
            INVALID_CODE_PATH;
        }
    }
}

static void ErrorTypeMismatch(Sem_Check_Context *ctx, Ast_Node *node, Type *a, Type *b)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->file_loc);
    PrintFileLocation(err_ctx->file, node->file_loc);
    fprintf(err_ctx->file, "Type mismatch. '");
    PrintType(err_ctx->file, a);
    fprintf(err_ctx->file, "' does not match '");
    PrintType(err_ctx->file, b);
    fprintf(err_ctx->file, "'\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, node->file_loc);
}

#if 0
static void ErrorArgTypeMismatch(Sem_Check_Context *ctx, Ast_Node *node, Type *arg_type, Type *param_type)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->file_loc);
    PrintFileLocation(err_ctx->file, node->file_loc);
    fprintf(err_ctx->file, "Argument type '");
    PrintType(err_ctx->file, arg_type);
    fprintf(err_ctx->file, "' does not match the parameter type '");
    PrintType(err_ctx->file, param_type);
    fprintf(err_ctx->file, "'\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, node->file_loc);
}
#endif

static void ErrorImport(Sem_Check_Context *ctx, Ast_Node *node, String filename)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->file_loc);
    PrintFileLocation(err_ctx->file, node->file_loc);
    fprintf(err_ctx->file, "Could not open file '");
    PrintString(err_ctx->file, filename);
    fprintf(err_ctx->file, "'\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, node->file_loc);
}

static void ErrorUndefinedReference(Sem_Check_Context *ctx, Ast_Node *node, Name name)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->file_loc);
    PrintFileLocation(err_ctx->file, node->file_loc);
    fprintf(err_ctx->file, "Undefined reference to '");
    PrintString(err_ctx->file, name.str);
    fprintf(err_ctx->file, "'\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, node->file_loc);
}

static void ErrorDeclaredEarlierAs(Sem_Check_Context *ctx,
        Ast_Node *node, Name name, Symbol *symbol)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->file_loc);
    PrintFileLocation(err_ctx->file, node->file_loc);
    fprintf(err_ctx->file, "'");
    PrintString(err_ctx->file, name.str);

    const char *sym_type = "";
    switch (symbol->sym_type)
    {
        case SYM_Module: sym_type = "module"; break;
        case SYM_Function: sym_type = "function"; break;
        case SYM_ForeignFunction: sym_type = "foreign function"; break;
        case SYM_Constant: sym_type = "constant"; break;
        case SYM_Variable: sym_type = "variable"; break;
        case SYM_Parameter: sym_type = "parameter"; break;
        case SYM_Member: sym_type = "struct member"; break;
        case SYM_Struct: sym_type = "struct"; break;
        case SYM_PrimitiveType: sym_type = "primitive type"; break;
    }
    fprintf(err_ctx->file, "' was declared as %s earlier\n", sym_type);

    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, node->file_loc);
}


static void CheckImport(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(node->import.module_name.data);
    String module_filename = { };
    Open_File *open_file = OpenModule(ctx->comp_ctx,
            ctx->open_file, node->import.module_name, &module_filename);
    if (!open_file)
    {
        ErrorImport(ctx, node, module_filename);
        return;
    }
    CompileModule(ctx->comp_ctx, open_file, module_filename);
}

static b32 CheckTypeCoercion(Type *from, Type *to)
{
    if (from == to) return true;
    if (!from || !to) return false;
    if (from->tag == TYP_int_lit)
    {
        // TODO(henrik): Check if the literal can fit in the to-type.
        switch (to->tag)
        {
        case TYP_u8: case TYP_s8:
        case TYP_u16: case TYP_s16:
        case TYP_u32: case TYP_s32:
        case TYP_u64: case TYP_s64:
            return true;
        default:
            return false;
        }
    }
    else if (from->tag == TYP_null)
    {
        return to->pointer > 0;
    }
    else if (from->tag == TYP_pointer && to->tag == TYP_pointer)
    {
        return from->base_type == to->base_type;
    }
    return false;
}

static Type* CheckType(Sem_Check_Context *ctx, Ast_Node *node)
{
    if (!node) return nullptr;
    switch (node->type)
    {
    case AST_Type_Plain:
        {
            Symbol *symbol = LookupSymbol(ctx->env, node->type_node.plain.name);
            if (symbol)
            {
                switch (symbol->sym_type)
                {
                    case SYM_Module:
                    case SYM_Function:
                    case SYM_ForeignFunction:
                    case SYM_Constant:
                    case SYM_Variable:
                    case SYM_Parameter:
                    case SYM_Member:
                        ErrorSymbolNotTypename(ctx, node, node->type_node.plain.name);
                        break;

                    case SYM_Struct:
                        return symbol->type;
                    //case SYM_Enum:
                    //case SYM_Typealias:
                        NOT_IMPLEMENTED("struct, enum and typealias in CheckType");
                        break;

                    case SYM_PrimitiveType:
                        return symbol->type;
                }
            }
        } break;
    case AST_Type_Pointer:
        {
            Type *base_type = CheckType(ctx, node->type_node.pointer.base_type);
            Type *type = PushType(ctx->env, TYP_pointer);
            type->base_type = base_type;
            type->pointer = node->type_node.pointer.indirection;
            return type;
        } break;
    case AST_Type_Array:
        NOT_IMPLEMENTED("Array type check");
        return CheckType(ctx, node->type_node.array.base_type);
        break;
    default:
        INVALID_CODE_PATH;
    }
    return nullptr;
}

static Type* CheckExpression(Sem_Check_Context *ctx, Ast_Node *node, Value_Type *vt);

#if 0
static void CheckFunctionArgs(Sem_Check_Context *ctx,
        Type *ftype, Ast_Node *node, Ast_Function_Call *function_call)
{
    s64 param_count = ftype->function_type.parameter_count;
    if (param_count != function_call->args.count)
    {
        Error(ctx, node, "Function argument count does not match any overload");
        return;
    }
    for (s64 i = 0; i < function_call->args.count; i++)
    {
        Ast_Node *arg = function_call->args.nodes[i];
        Type *arg_type = CheckExpression(ctx, arg);
        Type *param_type = ftype->function_type.parameter_types[i];
        if (!CheckTypeCoercion(ctx, arg_type, param_type))
        {
            ErrorArgTypeMismatch(ctx, arg, arg_type, param_type);
        }
    }
}
#endif

// Returns -1, if not compatible.
// Otherwise returns >= 0, if compatible.
static s64 CheckFunctionArgs(Sem_Check_Context *ctx,
        Type *ftype, Ast_Node *node, Ast_Function_Call *function_call)
{
    // TODO(henrik): Make the check so that we can report the argument type
    // mismatch of the best matching overload

    s64 param_count = ftype->function_type.parameter_count;
    if (param_count != function_call->args.count)
    {
        return -1;
    }
    s64 score = 0;
    for (s64 i = 0; i < param_count; i++)
    {
        Ast_Node *arg = function_call->args.nodes[i];
        Value_Type vt;
        Type *arg_type = CheckExpression(ctx, arg, &vt);
        Type *param_type = ftype->function_type.parameter_types[i];

        score *= 10;
        if (TypesEqual(arg_type, param_type))
        {
            score += 2;
        }
        else if (CheckTypeCoercion(arg_type, param_type))
        {
            score += 1;
        }
        else
        {
            //fprintf(stderr, "Not compatible: \n");
            //PrintType(stderr, arg_type);
            //fprintf(stderr, " != ");
            //PrintType(stderr, param_type);
            //fprintf(stderr, "\n");
            return -1;
        }
    }
    return score;
}

static Type* CheckFunctionCall(Sem_Check_Context *ctx, Ast_Node *node)
{
    Ast_Function_Call *function_call = &node->expression.function_call;
    Value_Type vt;
    Type *type = CheckExpression(ctx, function_call->fexpr, &vt);
    if (!type) return nullptr;
    if (type->tag == TYP_Function)
    {
        Ast_Node *fexpr = function_call->fexpr;
        ASSERT(fexpr->type == AST_VariableRef);
        Symbol *func = LookupSymbol(ctx->env, fexpr->expression.variable_ref.name);

        s64 best_score = -1;
        Symbol *best_overload = nullptr;
        b32 ambiguous = false;
        while (func)
        {
            s64 score = CheckFunctionArgs(ctx, func->type, node, function_call);
            if (score > best_score)
            {
                best_score = score;
                best_overload = func;
                ambiguous = false;
            }
            else if (score > 0 && score == best_score)
            {
                ambiguous = true;
            }
            func = func->next_overload;
        }
        if (!best_overload)
        {
            Error(ctx, node, "Function call does not match any overload");
            return nullptr;
        }
        else if (ambiguous)
        {
            Error(ctx, node, "Function call is ambiguous");
        }
        return best_overload->type->function_type.return_type;
    }
    Error(ctx, function_call->fexpr, "Expression is not callable");
    return nullptr;
}

static Type* CheckVariableRef(Sem_Check_Context *ctx, Ast_Node *node)
{
    Symbol *symbol = LookupSymbol(ctx->env, node->expression.variable_ref.name);
    if (!symbol)
    {
        ErrorUndefinedReference(ctx, node, node->expression.variable_ref.name);
        return nullptr;
    }
    return symbol->type;
}

static b32 TypeIsPointer(Type *t)
{
    if (!t) return false;
    return (t->tag == TYP_pointer) || (t->tag == TYP_null);
}

static b32 TypeIsBoolean(Type *t)
{
    if (!t) return false;
    return t->tag == TYP_bool;
}

static b32 TypeIsIntegral(Type *t)
{
    if (!t) return false;
    switch (t->tag)
    {
        case TYP_int_lit:
        case TYP_u8: case TYP_s8:
        case TYP_u16: case TYP_s16:
        case TYP_u32: case TYP_s32:
        case TYP_u64: case TYP_s64:
            return true;
        default:
            break;
    }
    return false;
}

static b32 TypeIsFloat(Type *t)
{
    if (!t) return false;
    return (t->tag == TYP_f32) || (t->tag == TYP_f64);
}

static b32 TypeIsNumeric(Type *t)
{
    return TypeIsIntegral(t) || TypeIsFloat(t);
}

static b32 TypeIsString(Type *t)
{
    if (!t) return false;
    return t->tag == TYP_string;
}

static b32 TypeIsStruct(Type *t)
{
    if (!t) return false;
    return t->tag == TYP_Struct;
}

// TODO: Implement module.member
static Type* CheckAccessExpr(Sem_Check_Context *ctx, Ast_Node *node, Value_Type *vt)
{
    Ast_Node *left = node->expression.access_expr.left;
    Ast_Node *right = node->expression.access_expr.right;

    Value_Type lvt;
    Type *ltype = CheckExpression(ctx, left, &lvt);

    *vt = VT_Assignable;
    if (!TypeIsStruct(ltype) && !TypeIsString(ltype))
    {
        Error(ctx, node, "Operator . must be used with structs or modules");
        return nullptr;
    }
    if (right->type != AST_VariableRef)
    {
        Error(ctx, node, "Right hand side of operator . must be a member name");
        return nullptr;
    }
    for (s64 i = 0; i < ltype->struct_type.member_count; i++)
    {
        Struct_Member *member = &ltype->struct_type.members[i];
        if (member->name == right->expression.variable_ref.name)
        {
            //fprintf(stderr, "access '");
            //PrintString(stderr, member->name.str);
            //fprintf(stderr, "' -> ");
            //PrintType(stderr, member->type);
            //fprintf(stderr, "\n");
            return member->type;
        }
    }
    Error(ctx, right, "Struct member not found");
    return nullptr;
}

static Type* CheckUnaryExpr(Sem_Check_Context *ctx, Ast_Node *node, Value_Type *vt)
{
    Unary_Op op = node->expression.unary_expr.op;
    Ast_Node *expr = node->expression.unary_expr.expr;

    Value_Type evt;
    Type *type = CheckExpression(ctx, expr, &evt);

    *vt = VT_NonAssignable;
    switch (op)
    {
    case UN_OP_Positive:
        {
            if (TypeIsNumeric(type))
                break;
            Error(ctx, expr, "Invalid operand for unary +");
        } break;
    case UN_OP_Negative:
        {
            if (TypeIsNumeric(type))
                break;
            Error(ctx, expr, "Invalid operand for unary -");
        } break;
    case UN_OP_Complement:
        {
            if (TypeIsIntegral(type))
                break;
            Error(ctx, expr, "Invalid operand for unary ~");
        } break;
    case UN_OP_Not:
        {
            if (TypeIsBoolean(type))
                break;
            Error(ctx, expr, "Invalid operand for logical !");
        } break;
    case UN_OP_Address:
        {
            if (evt == VT_Assignable)
            {
                Type *ptr_type = PushType(ctx->env, TYP_pointer);
                ptr_type->pointer = 1;
                ptr_type->base_type = type;
                type = ptr_type;
                break;
            }
            Error(ctx, expr, "Taking address of non-l-value");
        } break;
    case UN_OP_Deref:
        {
            *vt = VT_Assignable;
            if (TypeIsPointer(type))
            {
                type = type->base_type;
                break;
            }
            Error(ctx, expr, "Dereferencing non-pointer type");
        } break;
    }
    return type;
}

static Type* CheckBinaryExpr(Sem_Check_Context *ctx, Ast_Node *node, Value_Type *vt)
{
    Binary_Op op = node->expression.binary_expr.op;
    Ast_Node *left = node->expression.binary_expr.left;
    Ast_Node *right = node->expression.binary_expr.right;

    Value_Type lvt, rvt;
    Type *ltype = CheckExpression(ctx, left, &lvt);
    Type *rtype = CheckExpression(ctx, right, &rvt);

    *vt = VT_NonAssignable;
    switch (op)
    {
    case BIN_OP_Add:
        {
            if (TypeIsNumeric(ltype) && TypeIsNumeric(rtype))
                break;
            if (TypeIsPointer(ltype) && TypeIsNumeric(rtype))
                break;
            Error(ctx, node, "Invalid operands for binary +");
        } break;
    case BIN_OP_Subtract:
        {
            if (TypeIsNumeric(ltype) && TypeIsNumeric(rtype))
                break;
            if (TypeIsPointer(ltype) && TypeIsNumeric(rtype))
                break;
            Error(ctx, node, "Invalid operands for binary -");
        } break;
    case BIN_OP_Multiply:
        {
            if (TypeIsNumeric(ltype) && TypeIsNumeric(rtype))
                break;
            Error(ctx, node, "Operator * expects numeric type for left and right hand side");
        } break;
    case BIN_OP_Divide:
        {
            if (TypeIsNumeric(ltype) && TypeIsNumeric(rtype))
                break;
            Error(ctx, node, "Operator / expects numeric type for left and right hand side");
        } break;
    case BIN_OP_Modulo:
        {
            // TODO(henrik): Should modulo work for floats too?
            if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                break;
            Error(ctx, node, "Operator \% expects numeric type for left and right hand side");
        } break;

    case BIN_OP_BitAnd:
        {
            if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                break;
            Error(ctx, node, "Bitwise & expects integral type for left and right hand side");
        } break;
    case BIN_OP_BitOr:
        {
            if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                break;
            Error(ctx, node, "Bitwise | expects integral type for left and right hand side");
        } break;
    case BIN_OP_BitXor:
        {
            if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                break;
            Error(ctx, node, "Bitwise ^ expects integral type for left and right hand side");
        } break;

    case BIN_OP_And:
        {
            if (TypeIsBoolean(ltype) && TypeIsBoolean(rtype))
                break;
            Error(ctx, node, "Logical && expects boolean type for left and right hand side");
        } break;
    case BIN_OP_Or:
        {
            if (TypeIsBoolean(ltype) && TypeIsBoolean(rtype))
                break;
            Error(ctx, node, "Logical || expects boolean type for left and right hand side");
        } break;

    case BIN_OP_Equal:
        {
            if (CheckTypeCoercion(ltype, rtype))
                return GetBuiltinType(TYP_bool);
            if (CheckTypeCoercion(rtype, ltype))
                return GetBuiltinType(TYP_bool);
            Error(ctx, node, "Invalid operands for == operator");
        } break;
    case BIN_OP_NotEqual:
        {
            if (CheckTypeCoercion(ltype, rtype))
                return GetBuiltinType(TYP_bool);
            if (CheckTypeCoercion(rtype, ltype))
                return GetBuiltinType(TYP_bool);
            Error(ctx, node, "Invalid operands for != operator");
        } break;
    case BIN_OP_Less:
        {
            if (CheckTypeCoercion(ltype, rtype))
                return GetBuiltinType(TYP_bool);
            if (CheckTypeCoercion(rtype, ltype))
                return GetBuiltinType(TYP_bool);
            Error(ctx, node, "Invalid operands for < operator");
        } break;
    case BIN_OP_LessEq:
        {
            if (CheckTypeCoercion(ltype, rtype))
                return GetBuiltinType(TYP_bool);
            if (CheckTypeCoercion(rtype, ltype))
                return GetBuiltinType(TYP_bool);
            Error(ctx, node, "Invalid operands for <= operator");
        } break;
    case BIN_OP_Greater:
        {
            if (CheckTypeCoercion(ltype, rtype))
                return GetBuiltinType(TYP_bool);
            if (CheckTypeCoercion(rtype, ltype))
                return GetBuiltinType(TYP_bool);
            Error(ctx, node, "Invalid operands for > operator");
        } break;
    case BIN_OP_GreaterEq:
        {
            if (CheckTypeCoercion(ltype, rtype))
                return GetBuiltinType(TYP_bool);
            if (CheckTypeCoercion(rtype, ltype))
                return GetBuiltinType(TYP_bool);
            Error(ctx, node, "Invalid operands for >= operator");
        } break;

    case BIN_OP_Range:
        NOT_IMPLEMENTED("Binary expression ops");
        break;

    case BIN_OP_Subscript:
        NOT_IMPLEMENTED("Binary expression ops");
        break;
    }
    return ltype;
}

static Type* CheckAssignmentExpr(Sem_Check_Context *ctx, Ast_Node *node, Value_Type *vt)
{
    Assignment_Op op = node->expression.assignment.op;
    Ast_Node *left = node->expression.assignment.left;
    Ast_Node *right = node->expression.assignment.right;

    Value_Type lvt, rvt;
    Type *ltype = CheckExpression(ctx, left, &lvt);
    Type *rtype = CheckExpression(ctx, right, &rvt);

    if (lvt != VT_Assignable)
    {
        Error(ctx, left, "Assignment to non-l-value expression");
    }
    else
    {
        switch (op)
        {
        case AS_OP_Assign:
            {
                if (!CheckTypeCoercion(ltype, rtype))
                    Error(ctx, node, "Operands of assignment are incompatible");
            } break;
        case AS_OP_AddAssign:
            {
                if (CheckTypeCoercion(ltype, rtype))
                    break;
                else if (TypeIsPointer(ltype) && TypeIsIntegral(rtype))
                    break;
                Error(ctx, node, "Operands of += are incompatible");
            } break;
        case AS_OP_SubtractAssign:
            {
                if (CheckTypeCoercion(ltype, rtype))
                    break;
                else if (TypeIsPointer(ltype) && TypeIsIntegral(rtype))
                    break;
                Error(ctx, node, "Operands of -= are incompatible");
            } break;
        case AS_OP_MultiplyAssign:
            {
                if (TypeIsNumeric(ltype) && TypeIsNumeric(rtype))
                    break;
                Error(ctx, node, "Operands of *= are incompatible");
            } break;
        case AS_OP_DivideAssign:
            {
                if (TypeIsNumeric(ltype) && TypeIsNumeric(rtype))
                    break;
                Error(ctx, node, "Operands of /= are incompatible");
            } break;
        case AS_OP_ModuloAssign:
            {
                // TODO(henrik): Should modulo work for floats too?
                if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                    break;
                Error(ctx, node, "Operands of %= are incompatible");
            } break;

        case AS_OP_BitAndAssign:
            {
                if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                    break;
                Error(ctx, node, "Operands of &= must be integral");
            } break;
        case AS_OP_BitOrAssign:
            {
                if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                    break;
                Error(ctx, node, "Operands of |= must be integral");
            } break;
        case AS_OP_BitXorAssign:
            {
                if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                    break;
                Error(ctx, node, "Operands of ^= must be integral");
            } break;
        }
    }

    *vt = lvt;
    return ltype;
}

static Type* CheckExpression(Sem_Check_Context *ctx, Ast_Node *node, Value_Type *vt)
{
    switch (node->type)
    {
        case AST_Null:
            *vt = VT_NonAssignable;
            return GetBuiltinType(TYP_null);
        case AST_BoolLiteral:
            *vt = VT_NonAssignable;
            return GetBuiltinType(TYP_bool);
        case AST_CharLiteral:
            *vt = VT_NonAssignable;
            return GetBuiltinType(TYP_char);
        case AST_IntLiteral:
            *vt = VT_NonAssignable;
            return GetBuiltinType(TYP_int_lit);
        case AST_Float32Literal:
            *vt = VT_NonAssignable;
            return GetBuiltinType(TYP_f32);
        case AST_Float64Literal:
            *vt = VT_NonAssignable;
            return GetBuiltinType(TYP_f64);
        case AST_StringLiteral:
            *vt = VT_NonAssignable;
            return GetBuiltinType(TYP_string);

        case AST_VariableRef:
            *vt = VT_Assignable;
            return CheckVariableRef(ctx, node);
        case AST_FunctionCall:
            *vt = VT_NonAssignable;
            return CheckFunctionCall(ctx, node);

        case AST_AssignmentExpr:
            return CheckAssignmentExpr(ctx, node, vt);
        case AST_BinaryExpr:
            return CheckBinaryExpr(ctx, node, vt);
        case AST_UnaryExpr:
            return CheckUnaryExpr(ctx, node, vt);
        case AST_TernaryExpr:
            NOT_IMPLEMENTED("ternary expression checking");
            break;
        case AST_AccessExpr:
            return CheckAccessExpr(ctx, node, vt);
        default:
            INVALID_CODE_PATH;
    }
    return nullptr;
}

static void CheckVariableDecl(Sem_Check_Context *ctx, Ast_Node *node)
{
    Type *type = CheckType(ctx, node->variable_decl.type);
    Value_Type vt;
    Type *init_type = CheckExpression(ctx, node->variable_decl.init, &vt);
    ASSERT(type || init_type);

    if (!type) type = init_type;
    if (!CheckTypeCoercion(type, init_type))
    {
        Error(ctx, node, "Variable initializer expression is incompatible");
    }
    AddSymbol(ctx->env, SYM_Variable, node->variable_decl.name, type);
}

static void CheckStatement(Sem_Check_Context *ctx, Ast_Node *node);

static void CheckIfStatement(Sem_Check_Context *ctx, Ast_Node *node)
{
    Ast_Node *cond_expr = node->if_stmt.condition_expr;
    Ast_Node *true_stmt = node->if_stmt.true_stmt;
    Ast_Node *false_stmt = node->if_stmt.false_stmt;

    Value_Type vt;
    Type *ctype = CheckExpression(ctx, cond_expr, &vt);

    if (!TypeIsBoolean(ctype))
        Error(ctx, cond_expr, "If condition must be boolean");
    CheckStatement(ctx, true_stmt);
    if (false_stmt)
        CheckStatement(ctx, false_stmt);
}

static void CheckReturnStatement(Sem_Check_Context *ctx, Ast_Node *node)
{
    Ast_Node *rexpr = node->return_stmt.expression;
    if (rexpr)
    {
        Value_Type vt;
        Type *rtype = CheckExpression(ctx, rexpr, &vt);
        if (!ctx->env->return_type)
        {
            ctx->env->return_type = rtype;
        }
        else
        {
            if (!CheckTypeCoercion(rtype, ctx->env->return_type))
            {
                ErrorTypeMismatch(ctx, rexpr, rtype, ctx->env->return_type);
            }
        }
    }
    else
    {
        if (ctx->env->return_type)
        {
            Error(ctx, node, "Return value expceted");
        }
    }
}

static void CheckBlockStatement(Sem_Check_Context *ctx, Ast_Node *node);

static void CheckStatement(Sem_Check_Context *ctx, Ast_Node *node)
{
    switch (node->type)
    {
        case AST_BlockStmt:
            CheckBlockStatement(ctx, node);
            break;
        case AST_IfStmt:
            CheckIfStatement(ctx, node);
            break;
        case AST_ForStmt:
            //CheckForStatement(ctx, node);
            NOT_IMPLEMENTED("For statement");
            break;
        case AST_WhileStmt:
            //CheckWhileStatement(ctx, node);
            NOT_IMPLEMENTED("While statement");
            break;
        case AST_ReturnStmt:
            CheckReturnStatement(ctx, node);
            break;
        case AST_VariableDecl:
            CheckVariableDecl(ctx, node);
            break;
        default:
            Value_Type vt;
            CheckExpression(ctx, node, &vt);
            break;

        case AST_TopLevel:
        case AST_Import:
        case AST_ForeignBlock:
        case AST_FunctionDef:
        case AST_StructDef:
        case AST_Parameter:
            INVALID_CODE_PATH;
    }
}

static void CheckBlockStatement(Sem_Check_Context *ctx, Ast_Node *node)
{
    OpenScope(ctx->env);
    s64 count = node->block.statements.count;
    for (s64 i = 0; i < count && ContinueChecking(ctx); i++)
    {
        Ast_Node *stmt = node->block.statements.nodes[i];
        CheckStatement(ctx, stmt);
    }
    CloseScope(ctx->env);
}

static void CheckForeignFunctionParameters(Sem_Check_Context *ctx, Ast_Node *node, Type *ftype)
{
    s64 count = node->function.parameters.count;
    for (s64 i = 0; i < count && ContinueChecking(ctx); i++)
    {
        Ast_Node *param = node->function.parameters.nodes[i];
        Symbol *old_sym = LookupSymbolInCurrentScope(
                            ctx->env,
                            param->parameter.name);
        if (old_sym)
        {
            ASSERT(old_sym->sym_type == SYM_Parameter);
            Error(ctx, param, "Parameter already declared");
        }

        Type *param_type = CheckType(ctx, param->parameter.type);
        ftype->function_type.parameter_types[i] = param_type;
    }
}

static void CheckParameters(Sem_Check_Context *ctx, Ast_Node *node, Type *ftype)
{
    s64 count = node->function.parameters.count;
    for (s64 i = 0; i < count && ContinueChecking(ctx); i++)
    {
        Ast_Node *param = node->function.parameters.nodes[i];
        Symbol *old_sym = LookupSymbolInCurrentScope(
                            ctx->env,
                            param->parameter.name);
        if (old_sym)
        {
            ASSERT(old_sym->sym_type == SYM_Parameter);
            Error(ctx, param, "Parameter already declared");
        }

        Type *param_type = CheckType(ctx, param->parameter.type);
        AddSymbol(ctx->env, SYM_Parameter, param->parameter.name, param_type);

        ftype->function_type.parameter_types[i] = param_type;
    }
}

static b32 FunctionTypesAmbiguous(Sem_Check_Context *ctx, Type *a, Type *b)
{
    ASSERT(a->tag == TYP_Function && b->tag == TYP_Function);
    if (a == b) return true;

    Function_Type *ft_a = &a->function_type;
    Function_Type *ft_b = &b->function_type;

    if (ft_a->parameter_count != ft_b->parameter_count)
        return false;
    for (s64 i = 0; i < ft_a->parameter_count; i++)
    {
        if (!TypesEqual(ft_a->parameter_types[i], ft_b->parameter_types[i]))
        {
            return false;
        }
    }
    return true;
}

static void CheckFunction(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(node->function.name.str.data);

    s64 param_count = node->function.parameters.count;
    Type *ftype = PushType(ctx->env, TYP_Function);
    ftype->function_type.parameter_count = param_count;
    ftype->function_type.parameter_types = PushArray<Type*>(&ctx->env->arena, param_count);

    ftype->function_type.return_type = CheckType(ctx, node->function.return_type);

    // TODO(henrik): Should the names be copied to env->arena?
    Name name = node->function.name;
    Symbol *symbol = AddFunction(ctx->env, name, ftype);
    if (symbol->sym_type != SYM_Function)
    {
        ErrorDeclaredEarlierAs(ctx, node, name, symbol);
    }
    // NOTE(henrik): Lookup only in current scope, before opening the
    // function scope.
    Symbol *overload = LookupSymbolInCurrentScope(ctx->env, name);

    OpenFunctionScope(ctx->env, ftype->function_type.return_type);

    // NOTE(henrik): Check parameters after opening the function scope
    CheckParameters(ctx, node, ftype);

    while (overload && overload != symbol)
    {
        if (FunctionTypesAmbiguous(ctx, overload->type, symbol->type))
        {
            Error(ctx, node, "Ambiguous function definition");
            break;
        }
        overload = overload->next_overload;
    }

    CheckBlockStatement(ctx, node->function.body);

    ftype->function_type.return_type = ctx->env->return_type;
    CloseFunctionScope(ctx->env);
}

static void CheckStructMember(Sem_Check_Context *ctx,
        Struct_Member *struct_member, Ast_Node *node)
{
    ASSERT(node);
    ASSERT(node->type == AST_StructMember);

    struct_member->name = node->struct_member.name;
    struct_member->type = CheckType(ctx, node->struct_member.type);
}

static void CheckStruct(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(node->struct_def.name.str.data);

    Ast_Struct_Def *struct_def = &node->struct_def;
    s64 member_count = struct_def->members.count;

    Type *type = PushType(ctx->env, TYP_Struct);
    type->struct_type.name = struct_def->name;
    type->struct_type.member_count = member_count;
    type->struct_type.members = PushArray<Struct_Member>(&ctx->env->arena, member_count);
    AddSymbol(ctx->env, SYM_Struct, struct_def->name, type);

    for (s64 i = 0; i < member_count && ContinueChecking(ctx); i++)
    {
        CheckStructMember(ctx,
                &type->struct_type.members[i],
                struct_def->members.nodes[i]);
    }
}

static void CheckForeignFunction(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(node->function.name.str.data);

    s64 param_count = node->function.parameters.count;
    Type *ftype = PushType(ctx->env, TYP_Function);
    ftype->function_type.parameter_count = param_count;
    ftype->function_type.parameter_types = PushArray<Type*>(&ctx->env->arena, param_count);

    ftype->function_type.return_type = CheckType(ctx, node->function.return_type);

    Name name = node->function.name;
    Symbol *old_symbol = LookupSymbolInCurrentScope(ctx->env, name);
    if (old_symbol)
    {
        if (old_symbol->sym_type == SYM_ForeignFunction)
        {
            Error(ctx, node, "Foreign functions cannot have overloads");
        }
        else
        {
            ErrorDeclaredEarlierAs(ctx, node, name, old_symbol);
        }
        return;
    }
    AddSymbol(ctx->env, SYM_ForeignFunction, name, ftype);

    CheckForeignFunctionParameters(ctx, node, ftype);

    //PrintString(stderr, name.str);
    //PrintType(stderr, ftype);
    //fprintf(stderr, "\n");
}

static void CheckForeignBlockStmt(Sem_Check_Context *ctx, Ast_Node *node)
{
    switch (node->type)
    {
        case AST_FunctionDef:   CheckForeignFunction(ctx, node); break;
        case AST_StructDef:     CheckStruct(ctx, node); break;
        default:
            INVALID_CODE_PATH;
    }
}

static void CheckForeignBlock(Sem_Check_Context *ctx, Ast_Node *node)
{
    s64 count = node->foreign.statements.count;
    for (s64 i = 0; i < count && ContinueChecking(ctx); i++)
    {
        Ast_Node *stmt = node->foreign.statements.nodes[i];
        CheckForeignBlockStmt(ctx, stmt);
    }
}

static void CheckTopLevelStmt(Sem_Check_Context *ctx, Ast_Node *node)
{
    switch (node->type)
    {
        case AST_Import:        CheckImport(ctx, node); break;
        case AST_ForeignBlock:  CheckForeignBlock(ctx, node); break;
        case AST_FunctionDef:   CheckFunction(ctx, node); break;
        case AST_StructDef:     CheckStruct(ctx, node); break;
        case AST_VariableDecl:  CheckVariableDecl(ctx, node); break;
        default:
            INVALID_CODE_PATH;
    }
}

b32 Check(Sem_Check_Context *ctx)
{
    Ast_Node *root = ctx->ast->root;
    ASSERT(root);

    Ast_Node_List *statements = &root->top_level.statements;
    for (s64 index = 0;
        index < statements->count && ContinueChecking(ctx);
        index++)
    {
        Ast_Node *node = statements->nodes[index];
        CheckTopLevelStmt(ctx, node);
    }
    return HasError(ctx->comp_ctx);
}

} // hplang
