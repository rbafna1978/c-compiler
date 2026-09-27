#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "autodiff/autodiff.h"
#include "parser/parser.h"

namespace {

struct Result {
  bool ok;
  std::vector<std::string> diagnostics;
  std::unique_ptr<compiler::ast::TranslationUnit> unit;
};

Result run(const std::string& src) {
  compiler::parser::Parser parser;
  Result r;
  r.unit = parser.parse(src, "ad.c");
  EXPECT_NE(r.unit, nullptr);
  EXPECT_TRUE(parser.errors().empty());
  r.ok = compiler::autodiff::analyze(*r.unit, r.diagnostics);
  return r;
}

void expectError(const std::string& src, const std::string& fragment) {
  const auto r = run(src);
  ASSERT_FALSE(r.ok) << src;
  ASSERT_FALSE(r.diagnostics.empty());
  EXPECT_NE(r.diagnostics[0].find(fragment), std::string::npos) << r.diagnostics[0];
}

const std::string kF = "float f(float x, float y) { float t = x * y; return t + exp(x); }\n";

}  // namespace

TEST(AutodiffTest, GeneratesGradientFunctionAndRewritesCalls) {
  const auto r = run(kF + "int main() { print(grad(f, 1, 2.0, 3.0)); return 0; }");
  ASSERT_TRUE(r.ok) << (r.diagnostics.empty() ? "" : r.diagnostics[0]);
  ASSERT_EQ(r.unit->decls.size(), 3U);  // f, __grad_f_1, main
  const auto* grad = dynamic_cast<compiler::ast::FunctionDecl*>(r.unit->decls[1].get());
  ASSERT_NE(grad, nullptr);
  EXPECT_EQ(grad->name, "__grad_f_1");
  EXPECT_EQ(grad->return_type.name, "float");
  const std::string ast = compiler::ast::prettyPrint(*r.unit);
  EXPECT_NE(ast.find("CallExpr __grad_f_1"), std::string::npos);
  EXPECT_EQ(ast.find("CallExpr grad\n"), std::string::npos);
}

TEST(AutodiffTest, ReusesOneGeneratedFunctionPerParameter) {
  const auto r = run(kF + "int main() { print(grad(f, 0, 1.0, 2.0) + grad(f, 0, 3.0, 4.0)); return 0; }");
  ASSERT_TRUE(r.ok);
  EXPECT_EQ(r.unit->decls.size(), 3U);
}

TEST(AutodiffTest, GradientOfTensorParameterHasTensorType) {
  const auto r = run(
      "float f(tensor<float, 2, 2> a) { return sum(a * a); }\n"
      "int main() { tensor<float, 2, 2> g = grad(f, 0, [[1, 2], [3, 4]]); return 0; }");
  EXPECT_TRUE(r.ok) << (r.diagnostics.empty() ? "" : r.diagnostics[0]);
}

TEST(AutodiffTest, RejectsBadGradCalls) {
  expectError("int main() { return grad(g, 0, 1.0); }", "must be a function defined earlier");
  expectError(kF + "int main() { return grad(f, 2, 1.0, 2.0); }", "constant parameter index");
  expectError(kF + "int main() { return grad(f, 0, 1.0); }", "expects 2 argument(s) after the index");
  expectError(kF + "int main() { tensor<float, 2> t; return grad(f, 0, t, 2.0); }", "cannot pass");
}

TEST(AutodiffTest, RejectsFunctionsItCannotDifferentiate) {
  expectError("float f(float x) { float s = 0; for (int i = 0; i < 3; i += 1) { s += x; } return s; }\n"
              "int main() { return grad(f, 0, 1.0); }",
              "not straight-line");
  expectError("int f(float x) { return 1; } int main() { return grad(f, 0, 1.0); }", "must return a float");
  expectError("float f(int n) { return 1.0; } int main() { return grad(f, 0, 3); }", "not a float or float tensor");
  expectError("float h(float x) { return x; }\n"
              "float f(float x) { return h(x); } int main() { return grad(f, 0, 3.0); }",
              "cannot differentiate call to 'h'");
}

TEST(AutodiffTest, TreatsNonFloatSubexpressionsAsConstants) {
  // A comparison is int-typed, so its derivative is zero (a step function's gradient is 0 a.e.).
  EXPECT_TRUE(run("float f(float x) { return (x < 1.0) * x; } int main() { return grad(f, 0, 3.0); }").ok);
}

TEST(SemaMathTest, ChecksMathBuiltins) {
  EXPECT_TRUE(run("int main() { float a = exp(1) + log(2.0) + tanh(0.5); return 0; }").ok);
  expectError("int main() { return exp(\"x\"); }", "expects a number or tensor");
  expectError("int main() { return exp(1, 2); }", "expects 1 argument");
}
