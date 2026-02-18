#pragma once

#include <string>
#include <vector>

#include "ast/ast.h"
#include "sema/symbol_table.h"

namespace compiler::sema {

/** Semantic analyzer stub. */
class SemanticAnalyzer : public ast::ASTVisitor {
 public:
  /** Analyzes the translation unit and collects diagnostics. */
  bool analyze(ast::TranslationUnit& unit);

  /** Returns diagnostics accumulated during analysis. */
  const std::vector<std::string>& diagnostics() const;

  void visit(ast::TranslationUnit&) override;
  void visit(ast::FunctionDecl&) override;
  void visit(ast::VarDecl&) override;
  void visit(ast::StructDecl&) override;
  void visit(ast::CompoundStmt&) override;
  void visit(ast::IfStmt&) override;
  void visit(ast::WhileStmt&) override;
  void visit(ast::ForStmt&) override;
  void visit(ast::ReturnStmt&) override;
  void visit(ast::ExprStmt&) override;
  void visit(ast::BinaryExpr&) override;
  void visit(ast::UnaryExpr&) override;
  void visit(ast::CallExpr&) override;
  void visit(ast::MemberExpr&) override;
  void visit(ast::ArraySubscript&) override;
  void visit(ast::IntLiteral&) override;
  void visit(ast::FloatLiteral&) override;
  void visit(ast::CharLiteral&) override;
  void visit(ast::StringLiteral&) override;
  void visit(ast::VarRef&) override;

 private:
  SymbolTable symbols_;
  std::vector<std::string> diagnostics_;
};

}  // namespace compiler::sema
