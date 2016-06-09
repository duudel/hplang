
#include "compiler_options.h"

namespace hplang
{

Compiler_Options DefaultCompilerOptions()
{
    Compiler_Options result = { };

    result.target =
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
        CGT_AMD64_Windows;
#elif defined(linux) || defined(__linux) || defined(__linux__)
        CGT_AMD64_Unix;
#elif defined(__unix__)
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
