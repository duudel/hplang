
#include "compiler_options.h"

namespace hplang
{

Compiler_Options DefaultCompilerOptions()
{
    Compiler_Options result = { };
    result.max_error_count = 6;
    result.max_line_arrow_error_count = 4;
    result.stop_after = CP_CodeGen;
    return result;
}

} // hplang
