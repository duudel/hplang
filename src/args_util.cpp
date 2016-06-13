
#include "args_util.h"

#include <cstdio>
#include <cstring>

namespace hplang
{

Arg_Options_Context NewArgOptionsCtx(const Arg_Option *options, int argc, char *argv[])
{
    Arg_Options_Context ctx = { };
    ctx.argc = argc;
    ctx.argv = argv;
    ctx.arg_index = 1; // NOTE(henrik): To skip the name of the command
    ctx.options = options;
    return ctx;
}

static void print_padding(int padding)
{
    while (padding > 0)
    {
        putchar(' ');
        padding--;
    }
}

void PrintOptions(Arg_Options_Context *ctx)
{
    const Arg_Option *opt = ctx->options;
    while (opt->long_name)
    {
        int len = 0;
        if (opt->long_name)
        {
            len += printf("  --%s", opt->long_name);
            if (opt->long_args)
            {
                for (int i = 0; ; i++)
                {
                    if (!opt->long_args[i])
                        break;
                    len += printf("[%s]", opt->long_args[i]);
                }
            }
            else if (opt->accepts_arg)
            {
                len += printf(" <%s>", opt->accepts_arg);
            }
        }
        if (opt->short_name)
        {
            if (opt->long_name)
                len = printf("\n");
            len += printf("\t");
            len += printf("-%c", opt->short_name);
            if (opt->short_args)
            {
                len += printf("%s", opt->short_args);
            }
            else if (opt->accepts_arg)
            {
                len += printf(" <%s>", opt->accepts_arg);
            }
        }
        const int args_col_len = 28;
        if (len > args_col_len)
        {
            printf("\n");
            len = 0;
        }
        print_padding(args_col_len - len);
        printf("%s\n", opt->description);

        opt++;
    }
}

static const char* GetNextArg(Arg_Options_Context *ctx)
{
    if (ctx->arg_index >= ctx->argc) return nullptr;
    return ctx->argv[ctx->arg_index++];
}

bool GetNextOption(Arg_Options_Context *ctx, Arg_Option_Result *result)
{
    const char *opt_name = GetNextArg(ctx);
    if (!opt_name) return false;

    *result = { };

    bool use_long_name = false;
    if (opt_name[0] == '-')
    {
        opt_name++;
        if (opt_name[0] == '-')
        {
            opt_name++;
            use_long_name = true;
        }
    }
    else
    {
        result->arg = opt_name;
        return true;
    }

    const Arg_Option *opt = ctx->options;
    if (use_long_name)
    {
        while (opt->long_name)
        {
            if (strcmp(opt->long_name, opt_name) == 0)
            {
                const char *arg = nullptr;
                if (opt->long_args)
                {
                    arg = GetNextArg(ctx);
                    if (arg)
                    {
                        const char *long_arg = nullptr;
                        for (int i = 0; ; i++)
                        {
                            long_arg = opt->long_args[i];
                            if (!long_arg) break;
                            if (strcmp(long_arg, arg)) break;
                        }
                        arg = long_arg;
                    }
                }
                else if (opt->accepts_arg)
                {
                    arg = GetNextArg(ctx);
                }
                result->option = opt;
                result->arg = arg;
                return true;
            }
            opt++;
        }
    }
    else
    {
        while (opt->long_name)
        {
            if (opt->short_name == opt_name[0])
            {
                const char *arg = nullptr;
                const char *short_args = nullptr;
                if (opt->short_args)
                {
                    short_args = opt_name + 1;
                }
                else if (opt->accepts_arg)
                {
                    arg = GetNextArg(ctx);
                }
                result->option = opt;
                result->arg = arg;
                result->short_args = short_args;
                return true;
            }
            opt++;
        }
    }
    result->unrecognized = opt_name;
    return true;
}

} // hplang
