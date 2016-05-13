#ifndef H_HPLANG_IR_EXEC_H

namespace hplang
{

struct Ir_Exec_Env
{
};

void Run(Ir_Exec_Env *env, Ir_Routine *routine);

} // hplang

#define H_HPLANG_IR_EXEC_H
#endif
