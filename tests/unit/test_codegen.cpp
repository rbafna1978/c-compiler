#include <gtest/gtest.h>

#include <string>

#include "codegen/codegen.h"
#include "parser/parser.h"
#include "sema/sema.h"

namespace {

// Runs the full frontend + codegen; returns the IR (empty on failure).
std::string compile(const std::string& src) {
  compiler::parser::Parser parser;
  auto unit = parser.parse(src, "cg.c");
  EXPECT_NE(unit, nullptr);
  EXPECT_TRUE(parser.errors().empty());
  compiler::sema::SemanticAnalyzer sema;
  EXPECT_TRUE(sema.analyze(*unit));
  compiler::codegen::CodeGenerator cg("test");
  const bool ok = cg.generate(*unit);
  for (const auto& d : cg.diagnostics()) ADD_FAILURE() << d;
  return ok ? cg.ir() : "";
}

bool has(const std::string& ir, const std::string& s) { return ir.find(s) != std::string::npos; }

}  // namespace

TEST(CodegenTest, EmitsFunctionsAndArithmetic) {
  const auto ir = compile("int add(int a, int b) { return a + b; }");
  EXPECT_TRUE(has(ir, "define i32 @add(i32 %a, i32 %b)"));
  EXPECT_TRUE(has(ir, "add i32"));
}

TEST(CodegenTest, EmitsControlFlow) {
  const auto ir = compile(
      "int f(int n) { int s = 0; for (int i = 0; i < n; i += 1) { if (i % 2 == 0) { s += i; } }"
      " while (s > 100) { s -= 1; } return s; }");
  EXPECT_TRUE(has(ir, "for.cond"));
  EXPECT_TRUE(has(ir, "while.body"));
  EXPECT_TRUE(has(ir, "br i1"));
}

TEST(CodegenTest, EmitsFloatConversionsAndShortCircuit) {
  const auto ir = compile("float f(int a) { if (a > 0 && a < 9) { return a * 1.5; } return 0; }");
  EXPECT_TRUE(has(ir, "sitofp"));
  EXPECT_TRUE(has(ir, "phi i1"));
}

TEST(CodegenTest, EmitsStructsGlobalsAndCalls) {
  const auto ir = compile(
      "struct P { int x; int y; };\n"
      "int g = 2 + 3;\n"
      "int sum(struct P p) { return p.x + p.y; }\n"
      "int main() { struct P p; p.x = g; p.y = 1; return sum(p); }");
  EXPECT_TRUE(has(ir, "%struct.P = type { i32, i32 }"));
  EXPECT_TRUE(has(ir, "@g = global i32 5"));
  EXPECT_TRUE(has(ir, "call i32 @sum"));
}

TEST(CodegenTest, RejectsNonConstantGlobalInit) {
  compiler::parser::Parser parser;
  auto unit = parser.parse("int f() { return 1; } int g = f();", "g.c");
  compiler::sema::SemanticAnalyzer sema;
  ASSERT_TRUE(sema.analyze(*unit));
  compiler::codegen::CodeGenerator cg;
  EXPECT_FALSE(cg.generate(*unit));
}

TEST(CodegenTest, LowersTensorsToFlatStorageAndLoops) {
  const auto ir = compile(
      "tensor<float, 2, 2> f(tensor<float, 2, 2> a) { return matmul(a, a) + a; }\n"
      "int main() { tensor<float, 2, 2> m = [[1, 2], [3, 4]]; print(sum(f(m))); return 0; }");
  EXPECT_TRUE(has(ir, "define void @f(ptr %retval, ptr %a)"));  // tensor result via out-pointer
  EXPECT_TRUE(has(ir, "[4 x float]"));                          // 2x2 stored flat, row-major
  EXPECT_TRUE(has(ir, "loop.body"));
  EXPECT_TRUE(has(ir, "call i32 (ptr, ...) @printf"));
}
