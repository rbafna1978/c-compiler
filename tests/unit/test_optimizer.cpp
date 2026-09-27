#include <gtest/gtest.h>

#include <llvm/IR/Verifier.h>

#include <string>

#include "codegen/codegen.h"
#include "optimizer/optimizer.h"
#include "parser/parser.h"
#include "sema/sema.h"

namespace {

// Compiles `src`, optionally optimizes, returns the IR text.
std::string compile(const std::string& src, bool optimize) {
  compiler::parser::Parser parser;
  auto unit = parser.parse(src, "opt.c");
  compiler::sema::SemanticAnalyzer sema;
  EXPECT_TRUE(sema.analyze(*unit));
  compiler::codegen::CodeGenerator cg("test");
  EXPECT_TRUE(cg.generate(*unit));
  if (optimize) {
    compiler::optimizer::optimize(cg.module());
    EXPECT_FALSE(llvm::verifyModule(cg.module()));
  }
  return cg.ir();
}

bool has(const std::string& ir, const std::string& s) { return ir.find(s) != std::string::npos; }

}  // namespace

TEST(OptimizerTest, PromotesLocalsToRegisters) {
  const std::string src = "int f(int a) { int b = a + 1; return b * 2; }";
  EXPECT_TRUE(has(compile(src, false), "alloca"));
  EXPECT_FALSE(has(compile(src, true), "alloca"));
}

TEST(OptimizerTest, FoldsConstantsThroughCallsAndLoops) {
  const auto ir = compile(
      "int sq(int x) { return x * x; }"
      "int main() { int s = 0; for (int i = 0; i < 4; i += 1) { s += sq(i); } return s; }",
      true);
  EXPECT_TRUE(has(ir, "ret i32 14"));  // 0 + 1 + 4 + 9
}
