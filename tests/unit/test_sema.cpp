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
