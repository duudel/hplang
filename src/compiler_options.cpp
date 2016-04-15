
#include "compiler_options.h"

namespace hplang
{

Compiler_Options DefaultCompilerOptions()
{
    Compiler_Options result = { };
    result.max_error_count = 6;
    return result;
}

} // hplang
