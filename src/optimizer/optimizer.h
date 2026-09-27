#pragma once

#include <llvm/IR/Module.h>

namespace compiler::optimizer {

/** Runs LLVM's standard O2 pipeline (mem2reg, instcombine, GVN, simplifycfg, ...) on the module. */
void optimize(llvm::Module& module);

}  // namespace compiler::optimizer
