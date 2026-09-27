#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "ast/ast.h"
#include "sema/symbol_table.h"

namespace compiler::sema {

/** Resolves names and checks types; stores expression types in ASTNode::expr_type. */
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
  void visit(ast::TensorLiteral&) override;
  void visit(ast::IntLiteral&) override;
  void visit(ast::FloatLiteral&) override;
  void visit(ast::CharLiteral&) override;
  void visit(ast::StringLiteral&) override;
  void visit(ast::VarRef&) override;

 private:
  void error(int line, const std::string& message);
  std::string typeOf(ast::ASTNode& node);
  void checkType(const ast::TypeInfo& type, int line);
  void checkCondition(ast::ASTNode& expr, int line);
  std::string tensorArith(int line, const std::string& op, const std::string& l, const std::string& r);
  std::string builtinType(ast::CallExpr& call, const std::vector<std::string>& arg_types);

  SymbolTable symbols_;
  std::unordered_map<std::string, const ast::FunctionDecl*> funcs_;
  std::unordered_map<std::string, std::vector<ast::FieldDecl>> structs_;  // key: "struct X"
  std::string current_return_;
  std::vector<std::string> diagnostics_;
};

}  // namespace compiler::sema
