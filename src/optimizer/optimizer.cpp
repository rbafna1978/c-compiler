#include "optimizer/optimizer.h"

#include <llvm/Passes/PassBuilder.h>

namespace compiler::optimizer {

void optimize(llvm::Module& module) {
  llvm::LoopAnalysisManager lam;
  llvm::FunctionAnalysisManager fam;
  llvm::CGSCCAnalysisManager cgam;
  llvm::ModuleAnalysisManager mam;

  llvm::PassBuilder pb;
  pb.registerModuleAnalyses(mam);
  pb.registerCGSCCAnalyses(cgam);
  pb.registerFunctionAnalyses(fam);
  pb.registerLoopAnalyses(lam);
  pb.crossRegisterProxies(lam, fam, cgam, mam);

  pb.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2).run(module, mam);
}

}  // namespace compiler::optimizer
