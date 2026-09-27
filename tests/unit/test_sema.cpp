#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "parser/parser.h"
#include "sema/sema.h"

namespace {

// Parses `src` (must parse cleanly) and returns semantic diagnostics.
std::vector<std::string> diagnose(const std::string& src) {
  compiler::parser::Parser parser;
  auto unit = parser.parse(src, "sema.c");
  EXPECT_NE(unit, nullptr);
  EXPECT_TRUE(parser.errors().empty());
  compiler::sema::SemanticAnalyzer sema;
  sema.analyze(*unit);
  return sema.diagnostics();
}

void expectError(const std::string& src, const std::string& fragment) {
  const auto diags = diagnose(src);
  ASSERT_FALSE(diags.empty()) << src;
  EXPECT_NE(diags[0].find(fragment), std::string::npos) << diags[0];
}

}  // namespace

TEST(SemaTest, AcceptsValidProgram) {
  EXPECT_EQ(diagnose(
                  "struct P { int x; float y; };\n"
                  "int g = 3;\n"
                  "int fact(int n) { if (n <= 1) { return 1; } return n * fact(n - 1); }\n"
                  "int main() {\n"
                  "  struct P p; p.x = fact(4); p.y = 1.5;\n"
                                    "  for (int i = 0; i < 3; i += 1) { g = g + i; }\n"
                  "  return p.x + g;\n"
                  "}\n"),
            std::vector<std::string>{});
}

TEST(SemaTest, ReportsScopeErrors) {
  expectError("int main() { return x; }", "undeclared identifier 'x'");
  expectError("int main() { int a; int a; return 0; }", "redeclaration of 'a'");
  expectError("int main() { { int a = 1; } return a; }", "undeclared identifier 'a'");
  expectError("int f() { return 0; } int f() { return 1; }", "redefinition of function 'f'");
}

TEST(SemaTest, ReportsTypeErrors) {
  expectError("void f() {} int main() { return f() + 1; }", "invalid operands");
  expectError("int main() { return 1.5 % 2; }", "invalid operands");
  expectError("int main() { 1 = 2; return 0; }", "not assignable");
  expectError("void f() { return 1; }", "should not return a value");
  expectError("int f() { return; }", "should return a value");
  expectError("int main() { void v; return 0; }", "void type");
  expectError("struct S { int a; }; int main() { struct S s; int x = s; return 0; }",
              "cannot initialize");
}

TEST(SemaTest, ChecksCallsAndStructs) {
  expectError("int main() { return g(); }", "undeclared function 'g'");
  expectError("int f(int a) { return a; } int main() { return f(1, 2); }", "expects 1 argument");
  expectError("struct S { int a; }; int main() { struct S s; return s.b; }", "no member 'b'");
  expectError("int main() { struct T t; return 0; }", "undeclared type 'struct T'");
  expectError("int main() { int a; return a[0]; }", "invalid subscript");
  expectError("int main() { int a; return *a; }", "unary '*'");
}

TEST(SemaTest, RecordsExpressionTypes) {
  compiler::parser::Parser parser;
  auto unit = parser.parse("float f(int a) { return a * 2.0; }", "t.c");
  ASSERT_NE(unit, nullptr);
  compiler::sema::SemanticAnalyzer sema;
  ASSERT_TRUE(sema.analyze(*unit));
  const auto* fn = dynamic_cast<compiler::ast::FunctionDecl*>(unit->decls[0].get());
  const auto* ret = dynamic_cast<compiler::ast::ReturnStmt*>(fn->body->stmts[0].get());
  EXPECT_EQ(ret->value->expr_type.name, "float");
}

TEST(SemaTensorTest, AcceptsValidTensorPrograms) {
  EXPECT_EQ(diagnose(
                "int main() {\n"
                "  tensor<float, 2, 3> a = [[1, 2, 3], [4, 5, 6]];\n"
                "  tensor<float, 3, 2> b = transpose(a);\n"
                "  tensor<float, 2, 2> c = matmul(a, b);\n"
                "  tensor<float, 2, 2> d = c + c * 2 - c / 4;\n"
                "  d += c;\n"
                "  tensor<float, 3> row = a[1];\n"
                "  a[0][1] = 9;\n"
                "  float s = sum(d) + row[2];\n"
                "  return -sum(a) > 0;\n"
                "}\n"),
            std::vector<std::string>{});
}

TEST(SemaTensorTest, InfersTensorTypes) {
  compiler::parser::Parser parser;
  auto unit = parser.parse(
      "float f(tensor<float, 4, 5> a, tensor<float, 5, 2> b) { return sum(matmul(a, b)); }", "t.c");
  ASSERT_NE(unit, nullptr);
  compiler::sema::SemanticAnalyzer sema;
  ASSERT_TRUE(sema.analyze(*unit));
  const auto* fn = dynamic_cast<compiler::ast::FunctionDecl*>(unit->decls[0].get());
  const auto* ret = dynamic_cast<compiler::ast::ReturnStmt*>(fn->body->stmts[0].get());
  const auto* call = dynamic_cast<compiler::ast::CallExpr*>(ret->value.get());
  EXPECT_EQ(call->args[0]->expr_type.name, "tensor<float,4,2>");
  EXPECT_EQ(ret->value->expr_type.name, "float");
}

TEST(SemaTensorTest, RejectsShapeErrors) {
  const std::string h = "tensor<float, 2, 3> a; tensor<float, 3, 2> b; ";
  expectError(h + "int main() { tensor<float, 2, 3> c = a + b; return 0; }", "shape mismatch in '+'");
  expectError(h + "int main() { tensor<float, 2, 2> c = matmul(a, a); return 0; }", "inner dimensions differ");
  expectError(h + "int main() { tensor<float, 2, 3> c = b; return 0; }", "cannot initialize");
  expectError(h + "int main() { a += b; return 0; }", "shape mismatch in '+'");
  expectError(h + "int main() { return a[2][0]; }", "out of bounds");
  expectError("int main() { tensor<float, 2, 2> m = [[1, 2], [3]]; return 0; }", "different shapes");
  expectError("int main() { tensor<float, 2> m = [1, [2]]; return 0; }", "all numbers or all tensors");
  expectError("tensor<float, 0> z;", "dimensions must be positive");
  expectError(h + "int main() { return sum(a, b); }", "'sum' expects 1 argument");
  expectError("int main() { return sum(3); }", "must be a tensor");
  expectError(h + "int main() { tensor<float, 2, 3> c = a % a; return 0; }", "invalid operands");
  expectError(h + "int main() { if (a) { return 1; } return 0; }", "non-scalar");
}

TEST(SemaTensorTest, UserFunctionsShadowBuiltins) {
  EXPECT_EQ(diagnose("int sum(int a, int b) { return a + b; } int main() { return sum(1, 2); }"),
            std::vector<std::string>{});
}

TEST(SemaTensorTest, ReportsSourceLines) {
  const auto diags = diagnose("int main() {\n  return 1;\n  return y;\n}\n");
  ASSERT_FALSE(diags.empty());
  EXPECT_NE(diags[0].find("line 3"), std::string::npos) << diags[0];
}
