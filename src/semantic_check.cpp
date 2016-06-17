
#include "semantic_check.h"
#include "common.h"
#include "ast_types.h"
#include "compiler.h"
#include "error.h"
#include "compiler_options.h"
#include "assert.h"

#include <cstdio>
#include <cinttypes>

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
    array::Free(ctx->pending_exprs);
}

// Semantic check
// --------------

static b32 ContinueChecking(Sem_Check_Context *ctx)
{
    return ContinueCompiling(ctx->comp_ctx);
}

static void ShowLocation(Sem_Check_Context *ctx, File_Location file_loc, const char *message)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf((FILE*)err_ctx->file, "- %s\n", message);
    PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
}

static void Error(Sem_Check_Context *ctx, File_Location file_loc, const char *message)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, file_loc);
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf((FILE*)err_ctx->file, "%s\n", message);
    PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
}

static void ErrorSymbolNotTypename(Sem_Check_Context *ctx, Ast_Node *node, Name name)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->file_loc);
    PrintFileLocation(err_ctx->file, node->file_loc);
    fprintf((FILE*)err_ctx->file, "Symbol '");
    PrintString(err_ctx->file, name.str);
    fprintf((FILE*)err_ctx->file, "' is not a typename\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, node->file_loc);
}

static void ErrorFuncCallNoOverload(Sem_Check_Context *ctx,
        File_Location file_loc, Name func_name, s64 arg_count, Type **arg_types)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, file_loc);
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf((FILE*)err_ctx->file, "No function overload '");
    PrintString(err_ctx->file, func_name.str);
    PrintFunctionType(err_ctx->file, nullptr, arg_count, arg_types);
    fprintf((FILE*)err_ctx->file, "' found\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
}

static void ErrorNotCallable(Sem_Check_Context *ctx,
        File_Location file_loc, Type *fexpr_type)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, file_loc);
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf((FILE*)err_ctx->file, "Expression of type '");
    PrintType(err_ctx->file, fexpr_type);
    fprintf((FILE*)err_ctx->file, "' is not callable\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
}

static void ErrorReturnTypeMismatch(Sem_Check_Context *ctx,
        File_Location file_loc, Type *a, Type *b, Ast_Node *rt_inferred)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, file_loc);
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf((FILE*)err_ctx->file, "Return type '");
    PrintType(err_ctx->file, a);
    fprintf((FILE*)err_ctx->file, "' does not match '");
    PrintType(err_ctx->file, b);
    fprintf((FILE*)err_ctx->file, "'\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
    if (rt_inferred)
    {
        PrintFileLocation(err_ctx->file, rt_inferred->file_loc);
        fprintf((FILE*)err_ctx->file, "The return type was inferred here:\n");
        PrintSourceLineAndArrow(ctx->comp_ctx, rt_inferred->file_loc);
    }
}

static void ErrorReturnTypeInferFail(Sem_Check_Context *ctx,
        File_Location file_loc, Name func_name)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, file_loc);
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf((FILE*)err_ctx->file, "Could not infer return type for function '");
    PrintName(err_ctx->file, func_name);
    fprintf((FILE*)err_ctx->file, "'");
    PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
}

static void ErrorBinaryOperands(Sem_Check_Context *ctx,
        File_Location file_loc, const char *op_str, Type *ltype, Type *rtype)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, file_loc);
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf((FILE*)err_ctx->file, "Invalid operands for '%s' operator: '", op_str);
    PrintType(err_ctx->file, ltype);
    fprintf((FILE*)err_ctx->file, "' and '");
    PrintType(err_ctx->file, rtype);
    fprintf((FILE*)err_ctx->file, "'\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
}

static void ErrorTypecast(Sem_Check_Context *ctx, File_Location file_loc, Type *from_type, Type *to_type)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, file_loc);
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf((FILE*)err_ctx->file, "Type '");
    PrintType(err_ctx->file, from_type);
    fprintf((FILE*)err_ctx->file, "' cannot be casted to '");
    PrintType(err_ctx->file, to_type);
    fprintf((FILE*)err_ctx->file, "'\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
}


static void ErrorImport(Sem_Check_Context *ctx, Ast_Node *node, String filename)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->file_loc);
    PrintFileLocation(err_ctx->file, node->file_loc);
    fprintf((FILE*)err_ctx->file, "Could not open file '");
    PrintString(err_ctx->file, filename);
    fprintf((FILE*)err_ctx->file, "'\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, node->file_loc);
}

static void ErrorUndefinedReference(Sem_Check_Context *ctx, File_Location file_loc, Name name)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, file_loc);
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf((FILE*)err_ctx->file, "Undefined reference to '");
    PrintString(err_ctx->file, name.str);
    fprintf((FILE*)err_ctx->file, "'\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
}

static void ErrorDeclaredEarlierAs(Sem_Check_Context *ctx,
        File_Location file_loc, Symbol *symbol)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, file_loc);
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf((FILE*)err_ctx->file, "'");
    PrintString(err_ctx->file, symbol->name.str);

    const char *sym_type = "";
    switch (symbol->sym_type)
    {
        case SYM_Module:            sym_type = "module"; break;
        case SYM_Function:          sym_type = "function"; break;
        case SYM_ForeignFunction:   sym_type = "foreign function"; break;
        case SYM_Constant:          sym_type = "constant"; break;
        case SYM_Variable:          sym_type = "variable"; break;
        case SYM_Parameter:         sym_type = "parameter"; break;
        case SYM_Struct:            sym_type = "struct"; break;
        case SYM_Typealias:         sym_type = "typealias"; break;
        case SYM_PrimitiveType:     sym_type = "primitive type"; break;
    }
    fprintf((FILE*)err_ctx->file, "' was declared as %s earlier\n", sym_type);

    PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
}

static void ErrorVaribleShadowsParam(Sem_Check_Context *ctx,
        File_Location file_loc, Name name)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, file_loc);
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf((FILE*)err_ctx->file, "Variable '");
    PrintString(err_ctx->file, name.str);
    fprintf((FILE*)err_ctx->file, "' shadows a parameter with the same name\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
}

static void ErrorInvalidSubscriptOf(Sem_Check_Context *ctx,
        File_Location file_loc, Type *type)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, file_loc);
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf((FILE*)err_ctx->file, "Invalid subscript of type '");
    PrintType(err_ctx->file, type);
    fprintf((FILE*)err_ctx->file, "'\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
}


static b32 CheckTypeCoercion(Type *from, Type *to)
{
    if (from == to) return true;
    //if (!from || !to) return false;
    //if (!from || !to) return true; // To suppress extra errors after type error
    switch (from->tag)
    {
        default: break;
        case TYP_none:
            INVALID_CODE_PATH;
            return false;

        case TYP_pending:
            if (from->base_type)
            {
                return CheckTypeCoercion(from->base_type, to);
            }
            return true; // NOTE(henrik): suppress some errors, eh?
        case TYP_null:
            return TypeIsPointer(to);

        case TYP_pointer:
            if (to->tag == TYP_pointer)
            {
                return TypesEqual(from->base_type, to->base_type);
            }
            return false;

        case TYP_u8:
            switch (to->tag)
            {
                case TYP_u8: case TYP_u16: case TYP_u32: case TYP_u64:
                case TYP_s16: case TYP_s32: case TYP_s64:
                    return true;
                default:
                    return false;
            }
        case TYP_s8:
            switch (to->tag)
            {
                case TYP_s8: case TYP_u16: case TYP_u32: case TYP_u64:
                case TYP_s16: case TYP_s32: case TYP_s64:
                    return true;
                default:
                    return false;
            }
        case TYP_u16:
            switch (to->tag)
            {
                case TYP_u16: case TYP_u32: case TYP_u64:
                case TYP_s32: case TYP_s64:
                    return true;
                default:
                    return false;
            }
        case TYP_s16:
            switch (to->tag)
            {
                case TYP_s16: case TYP_u32: case TYP_u64:
                case TYP_s32: case TYP_s64:
                    return true;
                default:
                    return false;
            }
        case TYP_u32:
            switch (to->tag)
            {
                case TYP_u32: case TYP_u64:
                case TYP_s64:
                    return true;
                default:
                    return false;
            }
        case TYP_s32:
            switch (to->tag)
            {
                case TYP_u64:
                case TYP_s32: case TYP_s64:
                    return true;
                default:
                    return false;
            }
        case TYP_u64:
            switch (to->tag)
            {
                case TYP_u64:
                    return true;
                default:
                    return false;
            }
        case TYP_s64:
            switch (to->tag)
            {
                case TYP_s64:
                    return true;
                default:
                    return false;
            }
    }
    return false;
}

static Type* CheckType(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(node != nullptr);
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
                        //ErrorSymbolNotTypename(ctx, node, node->type_node.plain.name);
                        break;

                    case SYM_Struct:
                        return symbol->type;
                    //case SYM_Enum:
                    //    NOT_IMPLEMENTED("enum and typealias in CheckType");
                    //    break;
                    case SYM_Typealias:
                        return symbol->type;

                    case SYM_PrimitiveType:
                        return symbol->type;
                }
            }
            ErrorSymbolNotTypename(ctx, node, node->type_node.plain.name);
            return GetBuiltinType(TYP_none);
        } break;
    case AST_Type_Pointer:
        {
            Type *pointer_type = nullptr;
            s64 indirection = node->type_node.pointer.indirection;
            ASSERT(indirection > 0);
            while (indirection > 0)
            {
                Type *base_type = CheckType(ctx, node->type_node.pointer.base_type);
                pointer_type = GetPointerType(ctx->env, base_type);
                indirection--;
            }
            return pointer_type;
        } break;
    case AST_Type_Array:
        NOT_IMPLEMENTED("Array type check");
        return CheckType(ctx, node->type_node.array.base_type);
    case AST_Type_Function:
        {
            s64 param_count = node->type_node.function.param_count;
            Ast_Param_Type *param_type = node->type_node.function.param_types;

            Type *ftype = PushFunctionType(ctx->env, TYP_Function, param_count);
            s64 i = 0;
            while (param_type)
            {
                ftype->function_type.parameter_types[i] = CheckType(ctx, param_type->type);
                param_type = param_type->next;
                i++;
            }

            Ast_Node *return_type = node->type_node.function.return_type;
            ftype->function_type.return_type = CheckType(ctx, return_type);
            return ftype;
        } break;
    default:
        INVALID_CODE_PATH;
    }
    return nullptr;
}

static Ast_Expr* MakeTypecast(Sem_Check_Context *ctx,
        Ast_Expr *oper, Type *type)
{
    Ast_Expr *cast_expr = PushAstExpr<Ast_Typecast_Expr>(ctx->ast, AST_TypecastExpr, oper->file_loc);
    cast_expr->typecast_expr.expr = oper;
    cast_expr->typecast_expr.type = nullptr;
    cast_expr->expr_type = type;
    return cast_expr;
}

const s32 MAX_INT_32 = 0x7fffffff;
const s64 MAX_INT_64 = 0x7fffffffffffffff;

const u32 MAX_UINT_32 = 0xffffffff;
const u64 MAX_UINT_64 = 0xffffffffffffffff;

static Type* GetSignedIntLiteralType(Ast_Expr *expr)
{
    ASSERT(expr->type == AST_IntLiteral);
    if (expr->int_literal.value <= MAX_INT_32)
        return GetBuiltinType(TYP_s32);
    if (expr->int_literal.value <= MAX_INT_64)
        return GetBuiltinType(TYP_s64);
    return GetBuiltinType(TYP_u64);
}

static Type* GetUnsignedIntLiteralType(Ast_Expr *expr)
{
    ASSERT(expr->type == AST_UIntLiteral);
    if (expr->int_literal.value <= MAX_UINT_32)
        return GetBuiltinType(TYP_u32);
    return GetBuiltinType(TYP_u64);
}


static Type* CheckExpr(Sem_Check_Context *ctx, Ast_Expr *expr, Value_Type *vt);

static void CoerceFunctionArgs(Sem_Check_Context *ctx,
        Type *ftype, Ast_Function_Call *function_call)
{
    s64 arg_count = function_call->args.count;
    for (s64 i = 0; i < arg_count; i++)
    {
        Type *param_type = ftype->function_type.parameter_types[i];
        Ast_Expr *arg = array::At(function_call->args, i);
        if (!TypesEqual(arg->expr_type, param_type))
        {
            Ast_Expr *arg_cast = MakeTypecast(ctx, arg, param_type);
            array::Set(function_call->args, i, arg_cast);
        }
    }
}

// Returns -1, if not compatible.
// Otherwise returns >= 0, if compatible.
static s64 CheckFunctionArgs(Type *ftype, s64 arg_count, Type **arg_types)
{
    // TODO(henrik): Make the check so that we can report the argument type
    // mismatch of the best matching overload

    s64 param_count = ftype->function_type.parameter_count;
    if (param_count != arg_count)
    {
        return -1;
    }
    s64 score = 0;
    for (s64 i = 0; i < param_count; i++)
    {
        Type *arg_type = arg_types[i];
        Type *param_type = ftype->function_type.parameter_types[i];
        ASSERT(arg_type);
        ASSERT(param_type);

        // TODO(henrik): Make the check and overload resolution more robust and
        // intuitive for the user.
        //score <<= 2; // TODO(henrik): should this be done?
        if (TypesEqual(arg_type, param_type))
        {
            score += 3;
        }
        else if (CheckTypeCoercion(arg_type, param_type))
        {
            if (TypeIsIntegral(arg_type) && TypeIsIntegral(param_type))
            {
                if (TypeIsSigned(arg_type) == TypeIsSigned(param_type))
                    score += 2;
            }
            else
            {
                score += 1;
            }
        }
        else
        {
            //fprintf(stderr, "Function arg %d not compatible: \n", i);
            //PrintType(stderr, arg_type); fprintf(stderr, " != ");
            //PrintType(stderr, param_type); fprintf(stderr, "\n");
            return -1;
        }
    }
    return score;
}

static Type* CheckFunctionCall(Sem_Check_Context *ctx, Ast_Expr *expr)
{
    Ast_Function_Call *function_call = &expr->function_call;
    Ast_Expr *fexpr = function_call->fexpr;

    Value_Type vt;
    Type *fexpr_type = CheckExpr(ctx, fexpr, &vt);
    ASSERT(fexpr_type != nullptr);

    // TODO(henrik): Should not make an array for arg types as they are now
    // stored int the arg->expr_type and could be looked up from there.
    s64 arg_count = function_call->args.count;
    Type **arg_types = PushArray<Type*>(&ctx->env->arena, arg_count);
    b32 args_have_none_type = false;
    for (s64 i = 0; i < arg_count && ContinueChecking(ctx); i++)
    {
        Ast_Expr *arg = array::At(function_call->args, i);
        arg_types[i] = CheckExpr(ctx, arg, &vt);

        if (TypeIsNone(arg_types[i]))
            args_have_none_type = true;
    }

    // NOTE(henrik): If the invoked expression or any of the arguments had
    // errors, return quietly and propagate TYP_none to avoid extranous errors.
    if (!ContinueChecking(ctx) ||
        args_have_none_type ||
        TypeIsNone(fexpr_type))
    {
        return GetBuiltinType(TYP_none);
    }

    if (fexpr_type->tag == TYP_Function)
    {
        if (fexpr->type == AST_VariableRef)
        {
            Name func_name = fexpr->variable_ref.name;
            Symbol *func = LookupSymbol(ctx->env, func_name);

            if (func->sym_type == SYM_Function ||
                func->sym_type == SYM_ForeignFunction)
            {
                s64 best_score = -1;
                Symbol *best_overload = nullptr;
                Symbol *ambiguous = nullptr;
                while (func)
                {
                    s64 score = CheckFunctionArgs(func->type, arg_count, arg_types);
                    if (score > best_score)
                    {
                        best_score = score;
                        best_overload = func;
                        ambiguous = nullptr;
                    }
                    else if (score > 0 && score == best_score)
                    {
                        ambiguous = func;
                    }
                    func = func->next_overload;
                }
                if (!best_overload)
                {
                    ErrorFuncCallNoOverload(ctx, expr->file_loc,
                            func_name, arg_count, arg_types);
                    return GetBuiltinType(TYP_none);
                }
                else if (ambiguous)
                {
                    Error(ctx, expr->file_loc, "Function call is ambiguous; atleast two overloads match");
                    ShowLocation(ctx, best_overload->define_loc, "First overload here:");
                    ShowLocation(ctx, ambiguous->define_loc, "Second overload here:");
                }
                else
                {
                    CoerceFunctionArgs(ctx, best_overload->type, function_call);
                }
                fexpr->variable_ref.symbol = best_overload;
                return best_overload->type->function_type.return_type;
            }
            else if (func->sym_type == SYM_Variable ||
                    func->sym_type == SYM_Parameter ||
                    func->sym_type == SYM_Constant)
            {
                s64 score = CheckFunctionArgs(func->type, arg_count, arg_types);
                if (score < 0)
                {
                    // TODO(henrik): Improve this error message
                    Error(ctx, expr->file_loc, "Calling with invalid arguments");
                    return GetBuiltinType(TYP_none);
                }
                return func->type->function_type.return_type;
            }
        }
        else
        {
            s64 score = CheckFunctionArgs(fexpr_type, arg_count, arg_types);
            if (score < 0)
            {
                // TODO(henrik): Improve this error message
                Error(ctx, expr->file_loc, "Calling with invalid arguments");
                return GetBuiltinType(TYP_none);
            }
            return fexpr_type->function_type.return_type;
        }
    }
    ErrorNotCallable(ctx, fexpr->file_loc, fexpr_type);
    return GetBuiltinType(TYP_none);
}

static Type* CheckVariableRef(Sem_Check_Context *ctx, Ast_Expr *expr)
{
    Symbol *symbol = LookupSymbol(ctx->env, expr->variable_ref.name);
    if (!symbol)
    {
        ErrorUndefinedReference(ctx, expr->file_loc, expr->variable_ref.name);
        return GetBuiltinType(TYP_none);
    }
    expr->variable_ref.symbol = symbol;
    return symbol->type;
}

static Type* CheckTypecastExpr(Sem_Check_Context *ctx, Ast_Expr *expr, Value_Type *vt)
{
    Ast_Expr *oper = expr->typecast_expr.expr;
    Ast_Node *type_node = expr->typecast_expr.type;

    Value_Type evt;
    Type *oper_type = CheckExpr(ctx, oper, &evt);

    // NOTE(henrik): The typecast may be "synthetic", i.e. the node may have
    // been inserted into the tree by something other than the parser, thus
    // not having the type subtree describing the type. Instead,
    // expr->expr_type should have the type set previously.
    Type *to_type = expr->expr_type;
    if (!to_type)
    {
        ASSERT(type_node);
        to_type = CheckType(ctx, type_node);
    }

    *vt = VT_NonAssignable;

    if (TypeIsNone(oper_type) || TypeIsNone(to_type))
        return GetBuiltinType(TYP_none);

    if (TypeIsPointer(oper_type) && TypeIsPointer(to_type))
        return to_type;
    if (TypeIsNumeric(oper_type) && TypeIsNumeric(to_type))
        return to_type;
    if (TypeIsNumeric(oper_type) && TypeIsChar(to_type))
        return to_type;
    if (TypeIsChar(oper_type) && TypeIsNumeric(to_type))
        return to_type;

    ErrorTypecast(ctx, expr->file_loc, oper_type, to_type);
    return GetBuiltinType(TYP_none);
}

// TODO: Implement module.member
static Type* CheckAccessExpr(Sem_Check_Context *ctx, Ast_Expr *expr, Value_Type *vt)
{
    Ast_Expr *base_expr = expr->access_expr.base;
    Ast_Expr *member_expr = expr->access_expr.member;

    Value_Type base_vt;
    Type *base_type = CheckExpr(ctx, base_expr, &base_vt);

    *vt = VT_Assignable;
    if (TypeIsNone(base_type))
        return base_type;

    if (TypeIsNull(base_type))
    {
        Error(ctx, expr->file_loc, "Trying to access null with operator .");
        return GetBuiltinType(TYP_none);
    }

    if (TypeIsPointer(base_type))
    {
        base_type = base_type->base_type;
        ASSERT(base_type != nullptr);
    }
    if (!TypeIsStruct(base_type) && !TypeIsString(base_type))
    {
        Error(ctx, expr->file_loc, "Left hand side of operator . must be a struct or module");
        return GetBuiltinType(TYP_none);
    }
    if (member_expr->type != AST_VariableRef)
    {
        Error(ctx, expr->file_loc, "Right hand side of operator . must be a member name");
        return GetBuiltinType(TYP_none);
    }
    for (s64 i = 0; i < base_type->struct_type.member_count; i++)
    {
        Struct_Member *member = &base_type->struct_type.members[i];
        if (member->name == member_expr->variable_ref.name)
        {
            //fprintf(stderr, "access '");
            //PrintString(stderr, member->name.str);
            //fprintf(stderr, "' -> ");
            //PrintType(stderr, member->type);
            //fprintf(stderr, "\n");
            member_expr->expr_type = member->type;
            return member->type;
        }
    }
    Error(ctx, member_expr->file_loc, "Struct member not found");
    return GetBuiltinType(TYP_none);
}

static Type* CheckSubscriptExpr(Sem_Check_Context *ctx, Ast_Expr *expr, Value_Type *vt)
{
    Ast_Expr *base_expr = expr->subscript_expr.base;
    Ast_Expr *index_expr = expr->subscript_expr.index;

    Value_Type base_vt, index_vt;
    Type *base_type = CheckExpr(ctx, base_expr, &base_vt);
    Type *index_type = CheckExpr(ctx, index_expr, &index_vt);

    *vt = VT_Assignable;
    if (!TypeIsIntegral(index_type))
    {
        Error(ctx, expr->file_loc, "Invalid non-integral subscript operand");
        return GetBuiltinType(TYP_none);
    }
    if (TypeIsNull(base_type))
    {
        Error(ctx, expr->file_loc, "Invalid subscript of null");
        return GetBuiltinType(TYP_none);
    }
    if (TypeIsPointer(base_type))
    {
        return base_type->base_type;
    }
    //else if (TypeIsArray(base_type))
    //{
    //    // TODO(henrik): Static array check for negative or too large
    //    // subscript operands.
    //    // When implemented, static arrays have a known size.
    //    // If the subscript operand is a constant, we can catch the error
    //    // and give an error message.
    //    return base_type->base_type;
    //}
    ErrorInvalidSubscriptOf(ctx, expr->file_loc, base_type);
    Error(ctx, expr->file_loc, "Invalid subscript of type");
    return GetBuiltinType(TYP_none);
}

static Type* CheckTernaryExpr(Sem_Check_Context *ctx, Ast_Expr *expr, Value_Type *vt)
{
    Ast_Expr *cond_expr = expr->ternary_expr.cond_expr;
    Ast_Expr *true_expr = expr->ternary_expr.true_expr;
    Ast_Expr *false_expr = expr->ternary_expr.false_expr;

    Value_Type cvt;
    Type *cond_type = CheckExpr(ctx, cond_expr, &cvt);
    if (cond_type && !TypeIsBoolean(cond_type))
    {
        Error(ctx, cond_expr->file_loc,
                "Condition of ternary ?: expression must be boolean");
    }

    Value_Type tvt, fvt;
    Type *true_type = CheckExpr(ctx, true_expr, &tvt);
    Type *false_type = CheckExpr(ctx, false_expr, &fvt);

    if (tvt == VT_NonAssignable || fvt == VT_NonAssignable)
        *vt = VT_NonAssignable;
    else
        *vt = VT_Assignable;

    if (TypeIsNone(true_type) || TypeIsNone(false_type))
        return GetBuiltinType(TYP_none);

    if (!TypesEqual(true_type, false_type))
    {
        if (CheckTypeCoercion(true_type, false_type))
        {
            Ast_Expr *cast_expr = MakeTypecast(ctx, true_expr, false_type);
            expr->ternary_expr.true_expr = cast_expr;
            return false_type;
        }
        else if (CheckTypeCoercion(false_type, true_type))
        {
            Ast_Expr *cast_expr = MakeTypecast(ctx, false_expr, true_type);
            expr->ternary_expr.false_expr = cast_expr;
            return true_type;
        }
        else
        {
            Error(ctx, expr->file_loc,
                    "Both results of ternary ?: expression must be convertible to same type");
        }
    }
    return true_type;
}

/* Unary op type conversions/promotions
 * operators +, ~
 *  u8,u16,u32 -> u32
 *  s8,s16,s32 -> s32
 *  u64 -> u64
 *  s64 -> s64
 *  int literal -> u64
 * operator -
 *  u8,s8,u16,s16,s32 -> s32
 *  u32,s64 -> s64
 *  u64 -> u64
 *  int literal -> s64
 */
static Type* CheckUnaryExpr(Sem_Check_Context *ctx, Ast_Expr *expr, Value_Type *vt)
{
    Unary_Op op = expr->unary_expr.op;
    Ast_Expr *oper = expr->unary_expr.expr;

    Value_Type evt;
    Type *type = CheckExpr(ctx, oper, &evt);

    if (TypeIsNone(type))
        return type;

    if (TypeIsPending(type))
    {
        if (!type->base_type)
            return type;
        type = type->base_type;
    }

    *vt = VT_NonAssignable;
    switch (op)
    {
    case UN_OP_Positive:
        {
            if (!TypeIsNumeric(type))
            {
                Error(ctx, expr->file_loc, "Invalid operand for unary +");
                return GetBuiltinType(TYP_s64);
            }
            else
            {
                switch (type->tag)
                {
                case TYP_u8:
                case TYP_u16:
                    {
                        type = GetBuiltinType(TYP_u32);
                        Ast_Expr *cast_expr = MakeTypecast(ctx, oper, type);
                        expr->unary_expr.expr = cast_expr;
                        return type;
                    }

                case TYP_u32:
                    // No cast needed
                    return type;
                case TYP_s8:
                case TYP_s16:
                    {
                        type = GetBuiltinType(TYP_s32);
                        Ast_Expr *cast_expr = MakeTypecast(ctx, oper, type);
                        expr->unary_expr.expr = cast_expr;
                        return type;
                    }

                case TYP_s32:
                    // No cast needed
                    return type;
                default:
                    return type;
                }
            }
        } break;
    case UN_OP_Negative:
        {
            if (!TypeIsNumeric(type))
            {
                Error(ctx, expr->file_loc, "Invalid operand for unary -");
                return GetBuiltinType(TYP_s64);
            }
            else
            {
                switch (type->tag)
                {
                case TYP_u8: case TYP_s8:
                case TYP_u16: case TYP_s16:
                    {
                        type = GetBuiltinType(TYP_s32);
                        Ast_Expr *cast_expr = MakeTypecast(ctx, oper, type);
                        expr->unary_expr.expr = cast_expr;
                        return type;
                    }

                case TYP_s32:
                    // No cast needed
                    return type;
                case TYP_u32:
                    {
                        type = GetBuiltinType(TYP_s64);
                        Ast_Expr *cast_expr = MakeTypecast(ctx, oper, type);
                        expr->unary_expr.expr = cast_expr;
                        return type;
                    }

                default:
                    return type;
                }
            }
        } break;
    case UN_OP_Complement:
        {
            if (!TypeIsIntegral(type))
            {
                Error(ctx, expr->file_loc, "Invalid operand for unary ~");
                return GetBuiltinType(TYP_s64);
            }
            else
            {
                switch (type->tag)
                {
                case TYP_u8:
                case TYP_u16:
                    {
                        type = GetBuiltinType(TYP_u32);
                        Ast_Expr *cast_expr = MakeTypecast(ctx, oper, type);
                        expr->unary_expr.expr = cast_expr;
                        return type;
                    }

                case TYP_u32:
                    // No cast needed
                    return type;
                case TYP_s8:
                case TYP_s16:
                    {
                        type = GetBuiltinType(TYP_s32);
                        Ast_Expr *cast_expr = MakeTypecast(ctx, oper, type);
                        expr->unary_expr.expr = cast_expr;
                        return type;
                    }

                case TYP_s32:
                    // No cast needed
                    return type;
                default:
                    return type;
                }
            }
        } break;
    case UN_OP_Not:
        {
            if (!TypeIsBoolean(type))
            {
                Error(ctx, expr->file_loc, "Invalid operand for logical !");
                return GetBuiltinType(TYP_bool);
            }
            return type;
        } break;
    case UN_OP_Address:
        {
            if (evt != VT_Assignable)
            {
                Error(ctx, expr->file_loc, "Taking address of non-l-value");
                return type;
            }
            return GetPointerType(ctx->env, type);
        } break;
    case UN_OP_Deref:
        {
            *vt = VT_Assignable;
            if (!TypeIsPointer(type))
            {
                Error(ctx, expr->file_loc, "Dereferencing non-pointer type");
                return type;
            }
            type = type->base_type;
            if (TypeIsVoid(type))
                Error(ctx, expr->file_loc, "Dereferencing void pointer");
            return type;
        } break;
    }
    INVALID_CODE_PATH;
    return type;
}

// Type for binary operations with numerical operands.
// Returns the type of a binary operation. Note that this procedure alone does
// not determine that the combination of types and the operation is valid.
// See CheckBinaryExpr for more checks.
//
// Rules for numerical types, in order:
// f64 <*> T -> f64
// f32 <*> T -> f32
// u64 <*> T -> u64; if T is signed -> error, no type can hold the result
// s64 <*> T -> s64
// u32 <*> T -> s64, when T is signed
// u32 <*> T -> u32
//   T <*> U -> s32
//
// where:
// T and U are types
// <*> is the binary operator
// the types are commutative, so the first types can be switched
static Type* CoerceBinaryExprType(Sem_Check_Context *ctx,
        Ast_Expr *expr, Ast_Expr *left, Ast_Expr *right,
        Type *ltype, Type *rtype)
{
    if (TypeIsNone(ltype) || TypeIsNone(rtype))
        return GetBuiltinType(TYP_none);

    if (TypeIsPending(ltype))
    {
        if (!ltype->base_type)
            return ltype;
        ltype = ltype->base_type;
    }
    if (TypeIsPending(rtype))
    {
        if (!rtype->base_type)
            return rtype;
        rtype = rtype->base_type;
    }

    if (!TypeIsNumeric(ltype) || !TypeIsNumeric(rtype))
        return nullptr;

    // From here, both types are numeric

    if (ltype->tag == TYP_f64)
    {
        if (rtype->tag == TYP_f64)
        {
            return ltype;
        }
        Ast_Expr *cast_expr = MakeTypecast(ctx, right, ltype);
        expr->binary_expr.right = cast_expr;
        return ltype;
    }
    else if (rtype->tag == TYP_f64)
    {
        Ast_Expr *cast_expr = MakeTypecast(ctx, left, rtype);
        expr->binary_expr.left = cast_expr;
        return rtype;
    }

    if (ltype->tag == TYP_f32)
    {
        if (rtype->tag == TYP_f32)
        {
            return ltype;
        }
        Ast_Expr *cast_expr = MakeTypecast(ctx, right, ltype);
        expr->binary_expr.right = cast_expr;
        return ltype;
    }
    else if (rtype->tag == TYP_f32)
    {
        Ast_Expr *cast_expr = MakeTypecast(ctx, left, rtype);
        expr->binary_expr.left = cast_expr;
        return rtype;
    }

    // From here, both types are integral

    if (ltype->tag == TYP_u64)
    {
        if (rtype->tag == TYP_u64)
        {
            return ltype;
        }
        else if (TypeIsSigned(rtype))
        {
            Error(ctx, expr->file_loc,
                "Invalid operation on unsigned and signed operands");
        }
        Ast_Expr *cast_expr = MakeTypecast(ctx, right, ltype);
        expr->binary_expr.right = cast_expr;
        return ltype;
    }
    else if (rtype->tag == TYP_u64)
    {
        if (TypeIsSigned(ltype))
        {
            Error(ctx, expr->file_loc,
                "Invalid operation on signed and unsigned operands");
        }
        Ast_Expr *cast_expr = MakeTypecast(ctx, left, rtype);
        expr->binary_expr.left = cast_expr;
        return rtype;
    }

    // From here, both types in {s64, u32, s32, u16, s16, u8, s8}

    if (ltype->tag == TYP_s64)
    {
        if (rtype->tag == TYP_s64)
        {
            return ltype;
        }
        Ast_Expr *cast_expr = MakeTypecast(ctx, right, ltype);
        expr->binary_expr.right = cast_expr;
        return ltype;
    }
    else if (rtype->tag == TYP_s64)
    {
        Ast_Expr *cast_expr = MakeTypecast(ctx, left, rtype);
        expr->binary_expr.left = cast_expr;
        return rtype;
    }

    // From here, both types in {u32, s32, u16, s16, u8, s8}

    if (ltype->tag == TYP_u32)
    {
        if (rtype->tag == TYP_u32)
        {
            return ltype;
        }
        else if (TypeIsSigned(rtype))
        {
            Type *type = GetBuiltinType(TYP_s64);
            Ast_Expr *cast_left = MakeTypecast(ctx, left, type);
            Ast_Expr *cast_right = MakeTypecast(ctx, right, type);
            expr->binary_expr.left = cast_left;
            expr->binary_expr.right = cast_right;
            return type;
        }
        Ast_Expr *cast_expr = MakeTypecast(ctx, right, ltype);
        expr->binary_expr.right = cast_expr;
        return ltype;
    }
    else if (rtype->tag == TYP_u32)
    {
        if (TypeIsSigned(ltype))
        {
            Type *type = GetBuiltinType(TYP_s64);
            Ast_Expr *cast_left = MakeTypecast(ctx, left, type);
            Ast_Expr *cast_right = MakeTypecast(ctx, right, type);
            expr->binary_expr.left = cast_left;
            expr->binary_expr.right = cast_right;
            return type;
        }
        Ast_Expr *cast_expr = MakeTypecast(ctx, left, rtype);
        expr->binary_expr.left = cast_expr;
        return rtype;
    }

    // From here, both types in {s32, u16, s16, u8, s8}

    if (ltype->tag == TYP_s32)
    {
        if (rtype->tag == TYP_s32)
        {
            return ltype;
        }
        Ast_Expr *cast_expr = MakeTypecast(ctx, right, ltype);
        expr->binary_expr.right = cast_expr;
        return ltype;
    }
    else if (rtype->tag == TYP_s32)
    {
        Ast_Expr *cast_expr = MakeTypecast(ctx, left, rtype);
        expr->binary_expr.left = cast_expr;
        return rtype;
    }

    // Otherwise, just cast both operands to s32.
    Type *type = GetBuiltinType(TYP_s32);
    Ast_Expr *cast_left = MakeTypecast(ctx, left, type);
    Ast_Expr *cast_right = MakeTypecast(ctx, right, type);
    expr->binary_expr.left = cast_left;
    expr->binary_expr.right = cast_right;
    return type;
}

static Type* CheckBinaryExpr(Sem_Check_Context *ctx, Ast_Expr *expr, Value_Type *vt)
{
    Binary_Op op = expr->binary_expr.op;
    Ast_Expr *left = expr->binary_expr.left;
    Ast_Expr *right = expr->binary_expr.right;

    Value_Type lvt, rvt;
    Type *ltype = CheckExpr(ctx, left, &lvt);
    Type *rtype = CheckExpr(ctx, right, &rvt);

    *vt = VT_NonAssignable;
    if (TypeIsNone(ltype) || TypeIsNone(rtype))
        return GetBuiltinType(TYP_none);

    if (TypeIsPending(ltype))
    {
        if (!ltype->base_type)
        {
            return ltype;
        }
    }
    else if (TypeIsPending(rtype))
    {
        if (!rtype->base_type)
        {
            return rtype;
        }
    }

    ASSERT(ltype && rtype);

    Type *type = nullptr;
    type = ltype;
    switch (op)
    {
    case BIN_OP_Add:
        {
            if (TypeIsPointer(ltype) && TypeIsIntegral(rtype))
            {
                if (TypeIsNull(ltype))
                {
                    Error(ctx, expr->file_loc, "Invalid null operand");
                    return GetBuiltinType(TYP_none);
                }
                return ltype;
            }
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type)
            {
                ErrorBinaryOperands(ctx, expr->file_loc, "+", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            ASSERT(TypesEqual(expr->binary_expr.left->expr_type,
                        expr->binary_expr.right->expr_type));
            return type;
        } break;
    case BIN_OP_Subtract:
        {
            if (TypeIsPointer(ltype) && TypeIsIntegral(rtype))
            {
                if (TypeIsNull(ltype))
                {
                    Error(ctx, expr->file_loc, "Invalid null operand");
                    return GetBuiltinType(TYP_none);
                }
                return ltype;
            }
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type)
            {
                ErrorBinaryOperands(ctx, expr->file_loc, "-", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            ASSERT(TypesEqual(expr->binary_expr.left->expr_type,
                        expr->binary_expr.right->expr_type));
            return type;
        } break;
    case BIN_OP_Multiply:
        {
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type)
            {
                ErrorBinaryOperands(ctx, expr->file_loc, "*", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            ASSERT(TypesEqual(expr->binary_expr.left->expr_type,
                        expr->binary_expr.right->expr_type));
            return type;
        } break;
    case BIN_OP_Divide:
        {
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type)
            {
                ErrorBinaryOperands(ctx, expr->file_loc, "/", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            ASSERT(TypesEqual(expr->binary_expr.left->expr_type,
                        expr->binary_expr.right->expr_type));
            return type;
        } break;
    case BIN_OP_Modulo:
        {
            // TODO(henrik): Should modulo work for floats too?
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type)
            {
                ErrorBinaryOperands(ctx, expr->file_loc, "%", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            else if (!TypeIsIntegral(ltype) || !TypeIsIntegral(rtype))
            {
                ErrorBinaryOperands(ctx, expr->file_loc, "%", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            return type;
        } break;

    case BIN_OP_LeftShift:
        {
            if (!TypeIsIntegral(ltype) || !TypeIsIntegral(rtype))
            {
                ErrorBinaryOperands(ctx, expr->file_loc, "<<", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            return type;
        } break;
    case BIN_OP_RightShift:
        {
            if (!TypeIsIntegral(ltype) || !TypeIsIntegral(rtype))
            {
                ErrorBinaryOperands(ctx, expr->file_loc, ">>", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            return type;
        } break;

    case BIN_OP_BitAnd:
        {
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type)
            {
                ErrorBinaryOperands(ctx, expr->file_loc, "&", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            else if (!TypeIsIntegral(ltype) || !TypeIsIntegral(rtype))
            {
                ErrorBinaryOperands(ctx, expr->file_loc, "&", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            return type;
        } break;
    case BIN_OP_BitOr:
        {
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type)
            {
                ErrorBinaryOperands(ctx, expr->file_loc, "|", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            else if (!TypeIsIntegral(ltype) || !TypeIsIntegral(rtype))
            {
                ErrorBinaryOperands(ctx, expr->file_loc, "|", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            return type;
        } break;
    case BIN_OP_BitXor:
        {
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type)
            {
                ErrorBinaryOperands(ctx, expr->file_loc, "^", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            else if (!TypeIsIntegral(ltype) || !TypeIsIntegral(rtype))
            {
                ErrorBinaryOperands(ctx, expr->file_loc, "^", ltype, rtype);
                return GetBuiltinType(TYP_none);
            }
            return type;
        } break;

    case BIN_OP_And:
        {
            if (!TypeIsBoolean(ltype) || !TypeIsBoolean(rtype))
                Error(ctx, expr->file_loc, "Logical && expects boolean type for left and right hand side");
            return GetBuiltinType(TYP_bool);
        } break;
    case BIN_OP_Or:
        {
            if (!TypeIsBoolean(ltype) || !TypeIsBoolean(rtype))
                Error(ctx, expr->file_loc, "Logical || expects boolean type for left and right hand side");
            return GetBuiltinType(TYP_bool);
        } break;

    case BIN_OP_Equal:
        {
            if (TypesEqual(ltype, rtype))
                return GetBuiltinType(TYP_bool);
            if (TypeIsPointer(ltype) || TypeIsPointer(rtype))
            {
                if (TypeIsNull(ltype) || TypeIsNull(rtype))
                    return GetBuiltinType(TYP_bool);
            }
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type) ErrorBinaryOperands(ctx, expr->file_loc, "==", ltype, rtype);
            return GetBuiltinType(TYP_bool);
        } break;
    case BIN_OP_NotEqual:
        {
            if (TypesEqual(ltype, rtype))
                return GetBuiltinType(TYP_bool);
            if (TypeIsPointer(ltype) || TypeIsPointer(rtype))
            {
                if (TypeIsNull(ltype) || TypeIsNull(rtype))
                    return GetBuiltinType(TYP_bool);
            }
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type) ErrorBinaryOperands(ctx, expr->file_loc, "!=", ltype, rtype);
            return GetBuiltinType(TYP_bool);
        } break;
    case BIN_OP_Less:
        {
            if (TypesEqual(ltype, rtype))
                return GetBuiltinType(TYP_bool);
            if (TypeIsPointer(ltype) || TypeIsPointer(rtype))
            {
                if (TypeIsNull(ltype) || TypeIsNull(rtype))
                    return GetBuiltinType(TYP_bool);
            }
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type) ErrorBinaryOperands(ctx, expr->file_loc, "<", ltype, rtype);
            return GetBuiltinType(TYP_bool);
        } break;
    case BIN_OP_LessEq:
        {
            if (TypesEqual(ltype, rtype))
                return GetBuiltinType(TYP_bool);
            if (TypeIsPointer(ltype) || TypeIsPointer(rtype))
            {
                if (TypeIsNull(ltype) || TypeIsNull(rtype))
                    return GetBuiltinType(TYP_bool);
            }
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type) ErrorBinaryOperands(ctx, expr->file_loc, "<=", ltype, rtype);
            return GetBuiltinType(TYP_bool);
        } break;
    case BIN_OP_Greater:
        {
            if (TypesEqual(ltype, rtype))
                return GetBuiltinType(TYP_bool);
            if (TypeIsPointer(ltype) || TypeIsPointer(rtype))
            {
                if (TypeIsNull(ltype) || TypeIsNull(rtype))
                    return GetBuiltinType(TYP_bool);
            }
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type) ErrorBinaryOperands(ctx, expr->file_loc, ">", ltype, rtype);
            return GetBuiltinType(TYP_bool);
        } break;
    case BIN_OP_GreaterEq:
        {
            if (TypesEqual(ltype, rtype))
                return GetBuiltinType(TYP_bool);
            if (TypeIsPointer(ltype) || TypeIsPointer(rtype))
            {
                if (TypeIsNull(ltype) || TypeIsNull(rtype))
                    return GetBuiltinType(TYP_bool);
            }
            type = CoerceBinaryExprType(ctx, expr, left, right, ltype, rtype);
            if (!type) ErrorBinaryOperands(ctx, expr->file_loc, ">=", ltype, rtype);
            return GetBuiltinType(TYP_bool);
        } break;

    case BIN_OP_Range:
        NOT_IMPLEMENTED("Range expression semantic check");
        break;

    }
    return type;
}

static Type* CoerceAssignmentExprType(Sem_Check_Context *ctx,
        Ast_Expr *expr, Ast_Expr *left, Ast_Expr *right,
        Type *ltype, Type *rtype)
{
    (void)left;
    if (TypeIsNone(ltype) || TypeIsNone(rtype))
        return GetBuiltinType(TYP_none);

    if (TypeIsPending(ltype))
    {
        if (!ltype->base_type)
            return ltype;
        ltype = ltype->base_type;
    }
    if (TypeIsPending(rtype))
    {
        if (!rtype->base_type)
            return ltype;
        rtype = rtype->base_type;
    }

    if (!TypesEqual(ltype, rtype))
    {
        Ast_Expr *cast_expr = MakeTypecast(ctx, right, ltype);
        expr->binary_expr.right = cast_expr;
    }
    return ltype;
}

static Type* CheckAssignmentExpr(Sem_Check_Context *ctx, Ast_Expr *expr, Value_Type *vt)
{
    Assignment_Op op = expr->assignment.op;
    Ast_Expr *left = expr->assignment.left;
    Ast_Expr *right = expr->assignment.right;

    Value_Type lvt, rvt;
    Type *ltype = CheckExpr(ctx, left, &lvt);
    Type *rtype = CheckExpr(ctx, right, &rvt);
    ASSERT(ltype && rtype);

    *vt = lvt;

    if (TypeIsNone(ltype) || TypeIsNone(rtype))
        return GetBuiltinType(TYP_none);

    if (lvt != VT_Assignable)
    {
        Error(ctx, left->file_loc, "Assignment to non-l-value expression");
        return ltype;
    }

    switch (op)
    {
    case AS_OP_Assign:
        {
            if (CheckTypeCoercion(rtype, ltype))
                return CoerceAssignmentExprType(ctx, expr, left, right, ltype, rtype);
            ErrorBinaryOperands(ctx, expr->file_loc, "=", ltype, rtype);
        } break;
    case AS_OP_AddAssign:
        {
            if (CheckTypeCoercion(rtype, ltype))
                return CoerceAssignmentExprType(ctx, expr, left, right, ltype, rtype);
            else if (TypeIsPointer(ltype) && TypeIsIntegral(rtype))
                break;
            ErrorBinaryOperands(ctx, expr->file_loc, "+=", ltype, rtype);
        } break;
    case AS_OP_SubtractAssign:
        {
            if (CheckTypeCoercion(rtype, ltype))
                return CoerceAssignmentExprType(ctx, expr, left, right, ltype, rtype);
            else if (TypeIsPointer(ltype) && TypeIsIntegral(rtype))
                break;
            ErrorBinaryOperands(ctx, expr->file_loc, "-=", ltype, rtype);
        } break;
    case AS_OP_MultiplyAssign:
        {
            if (TypeIsNumeric(ltype) && TypeIsNumeric(rtype))
                return CoerceAssignmentExprType(ctx, expr, left, right, ltype, rtype);
            ErrorBinaryOperands(ctx, expr->file_loc, "*=", ltype, rtype);
        } break;
    case AS_OP_DivideAssign:
        {
            if (TypeIsNumeric(ltype) && TypeIsNumeric(rtype))
                return CoerceAssignmentExprType(ctx, expr, left, right, ltype, rtype);
            ErrorBinaryOperands(ctx, expr->file_loc, "/=", ltype, rtype);
        } break;
    case AS_OP_ModuloAssign:
        {
            // TODO(henrik): Should modulo work for floats too?
            if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                return CoerceAssignmentExprType(ctx, expr, left, right, ltype, rtype);
            ErrorBinaryOperands(ctx, expr->file_loc, "%%=", ltype, rtype);
        } break;

    case AS_OP_LeftShiftAssign:
        {
            if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                return ltype;
            ErrorBinaryOperands(ctx, expr->file_loc, "<<=", ltype, rtype);
        } break;
    case AS_OP_RightShiftAssign:
        {
            if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                return ltype;
            ErrorBinaryOperands(ctx, expr->file_loc, ">>=", ltype, rtype);
        } break;

    case AS_OP_BitAndAssign:
        {
            if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                return CoerceAssignmentExprType(ctx, expr, left, right, ltype, rtype);
            ErrorBinaryOperands(ctx, expr->file_loc, "&=", ltype, rtype);
        } break;
    case AS_OP_BitOrAssign:
        {
            if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                return CoerceAssignmentExprType(ctx, expr, left, right, ltype, rtype);
            ErrorBinaryOperands(ctx, expr->file_loc, "|=", ltype, rtype);
        } break;
    case AS_OP_BitXorAssign:
        {
            if (TypeIsIntegral(ltype) && TypeIsIntegral(rtype))
                return CoerceAssignmentExprType(ctx, expr, left, right, ltype, rtype);
            ErrorBinaryOperands(ctx, expr->file_loc, "^=", ltype, rtype);
        } break;
    }
    return ltype;
}

static Type* CheckExpr(Sem_Check_Context *ctx, Ast_Expr *expr, Value_Type *vt)
{
    Type *result_type = nullptr;
    switch (expr->type)
    {
        case AST_Null:
            *vt = VT_NonAssignable;
            result_type = GetBuiltinType(TYP_null);
            break;
        case AST_BoolLiteral:
            *vt = VT_NonAssignable;
            result_type = GetBuiltinType(TYP_bool);
            break;
        case AST_CharLiteral:
            *vt = VT_NonAssignable;
            result_type = GetBuiltinType(TYP_char);
            break;
        case AST_IntLiteral:
            *vt = VT_NonAssignable;
            result_type = GetSignedIntLiteralType(expr);
            break;
        case AST_UIntLiteral:
            *vt = VT_NonAssignable;
            result_type = GetUnsignedIntLiteralType(expr);
            break;
        case AST_Float32Literal:
            *vt = VT_NonAssignable;
            result_type = GetBuiltinType(TYP_f32);
            break;
        case AST_Float64Literal:
            *vt = VT_NonAssignable;
            result_type = GetBuiltinType(TYP_f64);
            break;
        case AST_StringLiteral:
            *vt = VT_NonAssignable;
            result_type = GetBuiltinType(TYP_string);
            break;

        case AST_VariableRef:
            *vt = VT_Assignable;
            result_type = CheckVariableRef(ctx, expr);
            break;
        case AST_FunctionCall:
            *vt = VT_NonAssignable;
            result_type = CheckFunctionCall(ctx, expr);
            break;

        case AST_AssignmentExpr:
            result_type = CheckAssignmentExpr(ctx, expr, vt);
            break;
        case AST_BinaryExpr:
            result_type = CheckBinaryExpr(ctx, expr, vt);
            break;
        case AST_UnaryExpr:
            result_type = CheckUnaryExpr(ctx, expr, vt);
            break;
        case AST_TernaryExpr:
            result_type = CheckTernaryExpr(ctx, expr, vt);
            break;
        case AST_AccessExpr:
            result_type = CheckAccessExpr(ctx, expr, vt);
            break;
        case AST_SubscriptExpr:
            result_type = CheckSubscriptExpr(ctx, expr, vt);
            break;
        case AST_TypecastExpr:
            result_type = CheckTypecastExpr(ctx, expr, vt);
            break;
    }
    expr->expr_type = result_type;
    return result_type;
}

static Type* CheckExpression(Sem_Check_Context *ctx, Ast_Expr *expr, Value_Type *vt)
{
    Type *type = CheckExpr(ctx, expr, vt);
    if (TypeIsPending(type)) //&& !type->base_type)
    {
        if (!type->base_type)
        {
            Pending_Expr pe = { };
            pe.expr = expr;
            pe.scope = CurrentScope(ctx->env);
            array::Push(ctx->pending_exprs, pe);
        }
        else
        {
            expr->expr_type = type->base_type;
        }
    }
    return type;
}

static void CheckVariableDecl(Sem_Check_Context *ctx, Ast_Node *node)
{
    Ast_Node *type_node = node->variable_decl.type;
    Ast_Expr *init_expr = node->variable_decl.init_expr;

    Type *type = nullptr;
    if (type_node)
    {
        type = CheckType(ctx, type_node);
    }

    Type *init_type = nullptr;
    if (init_expr)
    {
        Value_Type vt;
        init_type = CheckExpression(ctx, init_expr, &vt);
    }

    if (!type)
    {
        if (TypeIsNull(init_type))
            Error(ctx, node->file_loc, "Variable type cannot be inferred from null");
        else
            type = init_type;
    }
    else if (TypeIsVoid(type))
    {
        Error(ctx, node->file_loc, "Cannot declare variable of type void");
    }

    if (type && init_type)
    {
        if (!TypeIsNone(init_type) && !TypesEqual(init_type, type))
        {
            if (!CheckTypeCoercion(init_type, type))
            {
                Error(ctx, node->file_loc, "Variable initializer expression is incompatible");
            }
            else
            {
                Ast_Expr *cast_expr = MakeTypecast(ctx, init_expr, type);
                node->variable_decl.init_expr = cast_expr;
            }
        }
    }

#if 0
    Name name = node->variable_decl.name;
    Symbol *old_symbol = LookupSymbolInCurrentScope(ctx->env, name);
    if (old_symbol)
    {
        ErrorDeclaredEarlierAs(ctx, node->file_loc, old_symbol);
    }
    else
    {
        old_symbol = LookupSymbol(ctx->env, name);
        if (old_symbol && old_symbol->sym_type == SYM_Parameter)
            ErrorVaribleShadowsParam(ctx, node, name);
    }
    Symbol *symbol = AddSymbol(ctx->env, SYM_Variable, name, type, node->file_loc);
    // TODO(henrik): Make checking/marking a variable as global more elegant.
    // Maybe new symbol type SYM_GlobalVariable?
    if (!ctx->env->current->parent)
        symbol->flags |= SYMF_Global;
    node->variable_decl.symbol = symbol;
#else
    u32 sym_flags = 0;
    if (!ctx->env->current->parent)
        sym_flags |= SYMF_Global;
    Ast_Variable_Decl_Names *names = &node->variable_decl.names;
    while (names)
    {
        Symbol *old_symbol = LookupSymbolInCurrentScope(ctx->env, names->name);
        if (old_symbol)
        {
            ErrorDeclaredEarlierAs(ctx, names->file_loc, old_symbol);
        }
        else
        {
            old_symbol = LookupSymbol(ctx->env, names->name);
            if (old_symbol && old_symbol->sym_type == SYM_Parameter)
                ErrorVaribleShadowsParam(ctx, names->file_loc, names->name);
        }
        
        Symbol *symbol = AddSymbol(ctx->env, SYM_Variable,
                names->name, type, names->file_loc);
        symbol->flags |= sym_flags;
        names->symbol = symbol;

        names = names->next;
    }
#endif
}

static void CheckStatement(Sem_Check_Context *ctx, Ast_Node *node);

static void CheckIfStatement(Sem_Check_Context *ctx, Ast_Node *node)
{
    Ast_Expr *cond_expr = node->if_stmt.cond_expr;
    Ast_Node *then_stmt = node->if_stmt.then_stmt;
    Ast_Node *else_stmt = node->if_stmt.else_stmt;

    Value_Type vt;
    Type *cond_type = CheckExpression(ctx, cond_expr, &vt);

    if (!TypeIsNone(cond_type) && !TypeIsBoolean(cond_type))
        Error(ctx, cond_expr->file_loc, "If statement condition must be boolean");
    CheckStatement(ctx, then_stmt);
    if (else_stmt)
        CheckStatement(ctx, else_stmt);
}

static void CheckWhileStatement(Sem_Check_Context *ctx, Ast_Node *node)
{
    Ast_Expr *cond_expr = node->while_stmt.cond_expr;
    Ast_Node *loop_stmt = node->while_stmt.loop_stmt;

    Value_Type vt;
    Type *cond_type = CheckExpression(ctx, cond_expr, &vt);

    if (!TypeIsNone(cond_type) && !TypeIsBoolean(cond_type))
        Error(ctx, cond_expr->file_loc, "While loop condition must be boolean");
    
    ctx->breakables++;
    ctx->continuables++;
    
    CheckStatement(ctx, loop_stmt);

    ctx->breakables--;
    ctx->continuables--;
}

static void CheckForStatement(Sem_Check_Context *ctx, Ast_Node *node)
{
    Ast_Node *init_stmt = node->for_stmt.init_stmt;
    Ast_Expr *init_expr = node->for_stmt.init_expr;
    Ast_Expr *cond_expr = node->for_stmt.cond_expr;
    Ast_Expr *incr_expr = node->for_stmt.incr_expr;
    Ast_Node *loop_stmt = node->for_stmt.loop_stmt;

    OpenScope(ctx->env);
    if (init_expr)
    {
        Value_Type vt;
        CheckExpression(ctx, init_expr, &vt);
    }
    else
    {
        ASSERT(init_stmt != nullptr);
        CheckVariableDecl(ctx, init_stmt);
    }

    if (cond_expr)
    {
        Value_Type vt;
        Type *cond_type = CheckExpression(ctx, cond_expr, &vt);

        if (!TypeIsNone(cond_type) && !TypeIsBoolean(cond_type))
            Error(ctx, cond_expr->file_loc, "For loop condition must be boolean");
    }

    if (incr_expr)
    {
        Value_Type vt;
        CheckExpression(ctx, incr_expr, &vt);
    }

    ctx->breakables++;
    ctx->continuables++;

    CheckStatement(ctx, loop_stmt);

    ctx->breakables--;
    ctx->continuables--;

    CloseScope(ctx->env);
}

static void CheckReturnStatement(Sem_Check_Context *ctx, Ast_Node *node)
{
    IncReturnStatements(ctx->env);
    Ast_Expr *expr = node->return_stmt.expr;

    Type *type = nullptr;
    if (expr)
    {
        Value_Type vt;
        type = CheckExpression(ctx, expr, &vt);
        ASSERT(type);
    }

    Type *cur_return_type = GetCurrentReturnType(ctx->env);
    if (TypeIsPending(cur_return_type))
    {
        if (!cur_return_type->base_type)
        {
            if (!expr)
            {
                type = GetBuiltinType(TYP_void);
                InferReturnType(ctx->env, type, node);
            }
            else if (type != cur_return_type)
            {
                // NOTE(henrik): Infer only, if there was no dependency..
                if (!TypeIsNull(type))
                    InferReturnType(ctx->env, type, node);
            }
        }
        else
        {
            if (!expr)
            {
                if (!TypeIsVoid(cur_return_type))
                {
                    Error(ctx, node->file_loc, "Return value expected");
                }
                return;
            }

            if (TypeIsNull(cur_return_type))
            {
                if (TypeIsPointer(type))
                {
                    InferReturnType(ctx->env, type, node);
                }
            }

            if (!TypeIsNone(type) && !TypesEqual(type, cur_return_type))
            {
                if (!CheckTypeCoercion(type, cur_return_type))
                {
                    Ast_Node *infer_loc = GetCurrentReturnTypeInferLoc(ctx->env);
                    ErrorReturnTypeMismatch(ctx, expr->file_loc,
                            type, cur_return_type, infer_loc);
                }
                else
                {
                    Ast_Expr *cast_expr = MakeTypecast(ctx, expr, type);
                    node->return_stmt.expr = cast_expr;
                }
            }
        }
    }
    else
    {
        if (!expr)
        {
            if (!TypeIsVoid(cur_return_type))
            {
                Error(ctx, node->file_loc, "Return value expected");
            }
            return;
        }

        if (!TypeIsNone(type) && !TypesEqual(type, cur_return_type))
        {
            if (!CheckTypeCoercion(type, cur_return_type))
            {
                Ast_Node *infer_loc = GetCurrentReturnTypeInferLoc(ctx->env);
                ErrorReturnTypeMismatch(ctx, expr->file_loc,
                        type, cur_return_type, infer_loc);
            }
            else
            {
                Ast_Expr *cast_expr = MakeTypecast(ctx, expr, type);
                node->return_stmt.expr = cast_expr;
            }
        }
    }
}

static void CheckBreakStmt(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(ctx->breakables >= 0);
    if (ctx->breakables == 0)
    {
        Error(ctx, node->file_loc, "Stray break statement");
    }
}

static void CheckContinueStmt(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(ctx->continuables >= 0);
    if (ctx->continuables == 0)
    {
        Error(ctx, node->file_loc, "Stray continue statement");
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
        case AST_WhileStmt:
            CheckWhileStatement(ctx, node);
            break;
        case AST_ForStmt:
            CheckForStatement(ctx, node);
            break;
        case AST_RangeForStmt:
            //CheckRangeForStatement(ctx, node);
            NOT_IMPLEMENTED("Range for statement");
            break;
        case AST_ReturnStmt:
            CheckReturnStatement(ctx, node);
            break;
        case AST_BreakStmt:
            CheckBreakStmt(ctx, node);
            break;
        case AST_ContinueStmt:
            CheckContinueStmt(ctx, node);
            break;
        case AST_VariableDecl:
            CheckVariableDecl(ctx, node);
            break;
        case AST_ExpressionStmt:
            Value_Type vt;
            CheckExpression(ctx, node->expr_stmt.expr, &vt);
            break;

        case AST_TopLevel:
        case AST_Import:
        case AST_ForeignBlock:
        case AST_FunctionDef:
        case AST_FunctionDecl:
        case AST_Parameter:
        case AST_StructDef:
        case AST_StructMember:
        case AST_Typealias:
        case AST_Type_Plain:
        case AST_Type_Pointer:
        case AST_Type_Array:
        case AST_Type_Function:
            INVALID_CODE_PATH;
    }
}

static void CheckBlockStatement(Sem_Check_Context *ctx, Ast_Node *node)
{
    OpenScope(ctx->env);
    s64 count = node->block_stmt.statements.count;
    for (s64 i = 0; i < count && ContinueChecking(ctx); i++)
    {
        Ast_Node *stmt = array::At(node->block_stmt.statements, i);
        CheckStatement(ctx, stmt);
    }
    CloseScope(ctx->env);
}

static void CheckForeignFunctionParameters(Sem_Check_Context *ctx, Ast_Node *node, Type *ftype)
{
    s64 count = node->function_decl.parameters.count;
    for (s64 i = 0; i < count && ContinueChecking(ctx); i++)
    {
        Ast_Node *param = array::At(node->function_decl.parameters, i);
        Symbol *old_sym = LookupSymbolInCurrentScope(
                            ctx->env,
                            param->parameter.name);
        if (old_sym)
        {
            ASSERT(old_sym->sym_type == SYM_Parameter);
            Error(ctx, param->file_loc, "Parameter already declared");
        }

        Type *param_type = CheckType(ctx, param->parameter.type);
        ftype->function_type.parameter_types[i] = param_type;
    }
}

static void CheckParameters(Sem_Check_Context *ctx, Ast_Node *node, Type *ftype)
{
    s64 count = node->function_def.parameters.count;
    for (s64 i = 0; i < count && ContinueChecking(ctx); i++)
    {
        Ast_Node *param = array::At(node->function_def.parameters, i);
        Symbol *old_sym = LookupSymbolInCurrentScope(
                            ctx->env,
                            param->parameter.name);
        if (old_sym)
        {
            ASSERT(old_sym->sym_type == SYM_Parameter);
            Error(ctx, param->file_loc, "Parameter already declared");
        }

        Type *param_type = CheckType(ctx, param->parameter.type);
        ASSERT(param_type);
        Symbol *symbol = AddSymbol(ctx->env, SYM_Parameter,
                param->parameter.name, param_type, param->file_loc);
        param->parameter.symbol = symbol;

        ftype->function_type.parameter_types[i] = param_type;
    }
}

static b32 FunctionTypesAmbiguous(Type *a, Type *b)
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
    ASSERT(node->function_def.name.str.data);

    s64 param_count = node->function_def.parameters.count;
    Type *ftype = PushFunctionType(ctx->env, TYP_Function, param_count);
    Type *return_type = nullptr;
    if (node->function_def.return_type)
    {
        return_type = CheckType(ctx, node->function_def.return_type);
    }
    else
    {
        return_type = PushPendingType(ctx->env);
    }
    ftype->function_type.return_type = return_type;

    // TODO(henrik): Should the names be copied to env->arena?
    Name name = node->function_def.name;
    Symbol *symbol = AddFunction(ctx->env, name, ftype, node->file_loc);
    if (symbol->sym_type != SYM_Function)
    {
        ErrorDeclaredEarlierAs(ctx, node->file_loc, symbol);
    }

    node->function_def.symbol = symbol;

    // NOTE(henrik): Lookup only in current scope, before opening the
    // function scope.
    Symbol *overload = LookupSymbolInCurrentScope(ctx->env, name);

    OpenFunctionScope(ctx->env, name, return_type);

    // NOTE(henrik): Check parameters after opening the function scope
    CheckParameters(ctx, node, ftype);

    while (overload && overload != symbol)
    {
        if (FunctionTypesAmbiguous(overload->type, symbol->type))
        {
            Error(ctx, node->file_loc, "Duplicate function definition");
            ShowLocation(ctx, overload->define_loc, "Previous definition here");
            break;
        }
        overload = overload->next_overload;
    }

    CheckBlockStatement(ctx, node->function_def.body);

    // NOTE(henrik): Must be called before closing the scope
    Ast_Node *infer_loc = GetCurrentReturnTypeInferLoc(ctx->env);
    s64 return_stmt_count = GetReturnStatements(ctx->env);

    Type *inferred_return_type = CloseFunctionScope(ctx->env);
    if (return_type)
        ASSERT(inferred_return_type);

    if (!TypeIsVoid(return_type) && return_stmt_count == 0)
    {
        Error(ctx, node->file_loc, "No return statements in function returning non-void");
    }

    if (TypeIsPending(inferred_return_type) && !inferred_return_type->base_type)
    {
        //Error(ctx, node->file_loc, "Could not infer return type for function");
        ErrorReturnTypeInferFail(ctx, node->file_loc, name);
    }
    else if (TypeIsNull(inferred_return_type))
    {
        Error(ctx, infer_loc->file_loc, "Function type cannot be inferred from null");
    }
    ftype->function_type.return_type = inferred_return_type;

    if (!ftype->function_type.return_type)
    {
        // TODO(henrik): Should function return type be inferred later, save it
        // so we can try again. This also means that all other type checking
        // should cache their types and calculate them only, if the type was
        // not calculated earlier. (Needed to make rechecking more robust).
        //array::Push(pending_func_infers, {node, scope});
    }
}

static void CheckTypealias(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(node->typealias.type);

    Ast_Typealias *typealias = &node->typealias;
    Type *type = CheckType(ctx, typealias->type);

    Symbol *old_symbol = LookupSymbolInCurrentScope(ctx->env, typealias->name);
    if (old_symbol)
    {
        ErrorDeclaredEarlierAs(ctx, node->file_loc, old_symbol);
    }

    AddSymbol(ctx->env, SYM_Typealias, typealias->name, type, node->file_loc);
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

    Symbol *old_symbol = LookupSymbolInCurrentScope(ctx->env, struct_def->name);
    if (old_symbol)
    {
        ErrorDeclaredEarlierAs(ctx, node->file_loc, old_symbol);
    }

    AddSymbol(ctx->env, SYM_Struct, struct_def->name, type, node->file_loc);

    for (s64 i = 0; i < member_count && ContinueChecking(ctx); i++)
    {
        CheckStructMember(ctx,
                &type->struct_type.members[i],
                array::At(struct_def->members, i));
    }
}

static void CheckForeignFunction(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(node->function_decl.name.str.data);

    s64 param_count = node->function_decl.parameters.count;
    Type *ftype = PushType(ctx->env, TYP_Function);
    ftype->function_type.parameter_count = param_count;
    ftype->function_type.parameter_types = PushArray<Type*>(&ctx->env->arena, param_count);

    ftype->function_type.return_type = CheckType(ctx, node->function_decl.return_type);

    Name name = node->function_decl.name;
    Symbol *old_symbol = LookupSymbolInCurrentScope(ctx->env, name);
    if (old_symbol)
    {
        if (old_symbol->sym_type == SYM_ForeignFunction)
        {
            Error(ctx, node->file_loc, "Foreign functions cannot have overloads");
        }
        else
        {
            ErrorDeclaredEarlierAs(ctx, node->file_loc, old_symbol);
        }
        return;
    }
    AddSymbol(ctx->env, SYM_ForeignFunction, name, ftype, node->file_loc);

    CheckForeignFunctionParameters(ctx, node, ftype);
}

static void CheckForeignBlockStmt(Sem_Check_Context *ctx, Ast_Node *node)
{
    switch (node->type)
    {
        case AST_FunctionDecl:  CheckForeignFunction(ctx, node); break;
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
        Ast_Node *stmt = array::At(node->foreign.statements, i);
        CheckForeignBlockStmt(ctx, stmt);
    }
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
    CompileModule(ctx->comp_ctx, open_file);
}

static void CheckTopLevelStmt(Sem_Check_Context *ctx, Ast_Node *node)
{
    switch (node->type)
    {
        case AST_Import:        CheckImport(ctx, node); break;
        case AST_ForeignBlock:  CheckForeignBlock(ctx, node); break;
        case AST_FunctionDef:   CheckFunction(ctx, node); break;
        case AST_StructDef:     CheckStruct(ctx, node); break;
        case AST_Typealias:     CheckTypealias(ctx, node); break;
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
        Ast_Node *node = array::At(*statements, index);
        CheckTopLevelStmt(ctx, node);
    }

    s32 round = 0;
    do {
        Array<Pending_Expr> pending_exprs = ctx->pending_exprs;
        ctx->pending_exprs = { };
        if (pending_exprs.count > 0)
        {
            round++;
            // TODO(henrik): Remove this debug message
            fprintf(stdout, "DEBUG: round %d; pending exprs %" PRId64 "\n",
                    round, pending_exprs.count);
            for (s64 i = 0; i < pending_exprs.count; i++)
            {
                Pending_Expr pe = array::At(pending_exprs, i);
                SetCurrentScope(ctx->env, pe.scope);
                Value_Type vt;
                CheckExpression(ctx, pe.expr, &vt);
            }
            array::Free(pending_exprs);
        }
    } while (ctx->pending_exprs.count > 0 && round < 10);

    if (ctx->pending_exprs.count > 0)
    {
        Pending_Expr pe = array::At(ctx->pending_exprs, 0);
        Error(ctx, pe.expr->file_loc, "Could not infer the type of expression");
    }

    return HasError(ctx->comp_ctx);
}

} // hplang
