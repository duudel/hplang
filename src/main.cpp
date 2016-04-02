
#include "lexer.h"

#include <cstdio>

int main(int argc, char **argv)
{
    using namespace hplang;
    LexerContext lexer_ctx = NewLexerContext();

    const char text[] = "if (test) return;\nelse a + b;\n    /*+-*/ ++ += . :: import for (bool) while string 0.0 3.12f, 43.0d @ #";
    s64 text_length = sizeof(text);
    Lex(&lexer_ctx, text, text_length);

    return 0;
}
