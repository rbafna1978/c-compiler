#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "ast/ast.h"
#include "parser/parser.h"

namespace {

using compiler::ast::BinaryExpr;
using compiler::ast::CompoundStmt;
using compiler::ast::ForStmt;
using compiler::ast::FunctionDecl;
using compiler::ast::IfStmt;
using compiler::ast::ReturnStmt;
using compiler::ast::StructDecl;
using compiler::ast::TranslationUnit;
using compiler::ast::VarDecl;
using compiler::parser::Parser;

const FunctionDecl* findFunction(const TranslationUnit& tu, const std::string& name) {
  for (const auto& decl : tu.decls) {
    if (const auto* fn = dynamic_cast<const FunctionDecl*>(decl.get()); fn && fn->name == name) {
      return fn;
    }
  }
  return nullptr;
}

}  // namespace

TEST(ParserTest, ParsesDeclarationForms) {
  Parser parser;
  const std::string src =
      "struct Point { int x; int y; };\n"
      "int g = 10;\n"
      "int add(int a, int b) { return a + b; }\n";

  auto unit = parser.parse(src, "decls.c");
  ASSERT_NE(unit, nullptr);
  ASSERT_TRUE(parser.errors().empty());
  ASSERT_EQ(unit->decls.size(), 3U);

  EXPECT_NE(dynamic_cast<StructDecl*>(unit->decls[0].get()), nullptr);
  EXPECT_NE(dynamic_cast<VarDecl*>(unit->decls[1].get()), nullptr);
  EXPECT_NE(dynamic_cast<FunctionDecl*>(unit->decls[2].get()), nullptr);
}

TEST(ParserTest, HonorsExpressionPrecedenceAndAssociativity) {
  Parser parser;
  auto unit = parser.parse("int main() { return 1 + 2 * 3; }", "precedence.c");
  ASSERT_NE(unit, nullptr);
  ASSERT_TRUE(parser.errors().empty());

  const auto* fn = findFunction(*unit, "main");
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->body, nullptr);
  ASSERT_EQ(fn->body->stmts.size(), 1U);

  const auto* ret = dynamic_cast<ReturnStmt*>(fn->body->stmts[0].get());
  ASSERT_NE(ret, nullptr);
  const auto* add = dynamic_cast<BinaryExpr*>(ret->value.get());
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->op, "+");
  const auto* mul = dynamic_cast<BinaryExpr*>(add->rhs.get());
  ASSERT_NE(mul, nullptr);
  EXPECT_EQ(mul->op, "*");
}

TEST(ParserTest, ResolvesDanglingElseToNearestIf) {
  Parser parser;
  auto unit = parser.parse(
      "int main(){ if(a) if(b) return 1; else return 2; return 3; }", "ifelse.c");
  ASSERT_NE(unit, nullptr);
  ASSERT_TRUE(parser.errors().empty());

  const auto* fn = findFunction(*unit, "main");
  ASSERT_NE(fn, nullptr);
  ASSERT_EQ(fn->body->stmts.size(), 2U);

  const auto* outer_if = dynamic_cast<IfStmt*>(fn->body->stmts[0].get());
  ASSERT_NE(outer_if, nullptr);
  EXPECT_EQ(outer_if->else_branch, nullptr);

  const auto* inner_if = dynamic_cast<IfStmt*>(outer_if->then_branch.get());
  ASSERT_NE(inner_if, nullptr);
  EXPECT_NE(inner_if->else_branch, nullptr);
}

TEST(ParserTest, ParsesLoopsAndPostfixExpressions) {
  Parser parser;
  const std::string src =
      "int main(){\n"
      "  int i = 0;\n"
      "  for(i = 0; i < 10; i += 1) {\n"
      "    while(i < 5) { i = arr[i].x->y; }\n"
      "  }\n"
      "  return foo(i, 2);\n"
      "}";

  auto unit = parser.parse(src, "loops.c");
  ASSERT_NE(unit, nullptr);
  ASSERT_TRUE(parser.errors().empty());

  const auto* fn = findFunction(*unit, "main");
  ASSERT_NE(fn, nullptr);
  ASSERT_GE(fn->body->stmts.size(), 3U);
  EXPECT_NE(dynamic_cast<ForStmt*>(fn->body->stmts[1].get()), nullptr);
}

TEST(ParserTest, ReportsMultipleErrorsViaRecovery) {
  Parser parser;
  const std::string src =
      "int main( {\n"
      "  int x = ;\n"
      "  return ;\n"
      "}\n";

  auto unit = parser.parse(src, "bad.c");
  EXPECT_EQ(unit, nullptr);
  EXPECT_GE(parser.errors().size(), 2U);
}

TEST(ParserTest, PrettyPrintsDistanceProgram) {
  Parser parser;
  const std::string src =
      "int distance(int x1, int y1, int x2, int y2) {\n"
      "  int dx = x2 - x1;\n"
      "  int dy = y2 - y1;\n"
      "  return dx * dx + dy * dy;\n"
      "}";

  auto unit = parser.parse(src, "distance.c");
  ASSERT_NE(unit, nullptr);
  ASSERT_TRUE(parser.errors().empty());

  const std::string printed = compiler::ast::prettyPrint(*unit);
  EXPECT_NE(printed.find("FunctionDecl distance"), std::string::npos);
  EXPECT_NE(printed.find("VarDecl dx:int"), std::string::npos);
  EXPECT_NE(printed.find("BinaryExpr +"), std::string::npos);
}
