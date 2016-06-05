#ifndef H_HPLANG_ARGS_UTIL_H

namespace hplang
{

struct Arg_Option
{
    const char *long_name;
    char short_name;
    const char **long_args;
    const char *short_args;
    const char *description;
    const char *accepts_arg;
};

struct Arg_Option_Result
{
    const Arg_Option *option;
    const char *arg;
    const char *short_args;
    const char *unrecognized;
};

struct Arg_Options_Context
{
    int argc;
    int arg_index;
    char **argv;
    const Arg_Option *options;
};

Arg_Options_Context NewArgOptionsCtx(const Arg_Option *options, int argc, char *argv[]);
bool GetNextOption(Arg_Options_Context *ctx, Arg_Option_Result *result);
void PrintOptions(Arg_Options_Context *ctx);

} // hplang

#define H_HPLANG_ARGS_UTIL_H
#endif
