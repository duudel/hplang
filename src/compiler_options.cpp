
#include "hplang.h"
#include "compiler_options.h"

namespace hplang
{

Compiler_Options DefaultCompilerOptions()
{
    Compiler_Options result = { };

    result.target =
#ifdef HP_WIN
        CGT_AMD64_Windows;
#elif defined(HP_UNIX)
        CGT_AMD64_Unix;
#else
        CGT_AMD64_Unix;
#endif

    result.max_error_count = 6;
    result.max_line_arrow_error_count = 4;
    result.stop_after = CP_Linking;

    result.diagnose_memory = false;
    return result;
}

} // hplang
