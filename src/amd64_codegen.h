#ifndef H_HPLANG_AMD64_CODEGEN_H

#include "types.h"
#include "codegen.h"

namespace hplang
{

void InitializeCodegen_Amd64(Codegen_Context *ctx, Codegen_Target cg_target);
void GenerateCode_Amd64(Codegen_Context *ctx, Ir_Routine_List routines);

void OutputCode_Amd64(Codegen_Context *ctx);

} // hplang

#define H_HPLANG_AMD64_CODEGEN_H
#endif
