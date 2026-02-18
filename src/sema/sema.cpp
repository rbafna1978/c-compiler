#include "sema/sema.h"

namespace compiler::sema {

bool SemanticAnalyzer::analyze(ast::TranslationUnit& unit) {
  diagnostics_.clear();
  unit.accept(*this);
  return diagnostics_.empty();
}

const std::vector<std::string>& SemanticAnalyzer::diagnostics() const {
  return diagnostics_;
}

void SemanticAnalyzer::visit(ast::TranslationUnit&) {}
void SemanticAnalyzer::visit(ast::FunctionDecl&) {}
void SemanticAnalyzer::visit(ast::VarDecl&) {}
void SemanticAnalyzer::visit(ast::StructDecl&) {}
void SemanticAnalyzer::visit(ast::CompoundStmt&) {}
void SemanticAnalyzer::visit(ast::IfStmt&) {}
void SemanticAnalyzer::visit(ast::WhileStmt&) {}
void SemanticAnalyzer::visit(ast::ForStmt&) {}
void SemanticAnalyzer::visit(ast::ReturnStmt&) {}
void SemanticAnalyzer::visit(ast::ExprStmt&) {}
void SemanticAnalyzer::visit(ast::BinaryExpr&) {}
void SemanticAnalyzer::visit(ast::UnaryExpr&) {}
void SemanticAnalyzer::visit(ast::CallExpr&) {}
void SemanticAnalyzer::visit(ast::MemberExpr&) {}
void SemanticAnalyzer::visit(ast::ArraySubscript&) {}
void SemanticAnalyzer::visit(ast::IntLiteral&) {}
void SemanticAnalyzer::visit(ast::FloatLiteral&) {}
void SemanticAnalyzer::visit(ast::CharLiteral&) {}
void SemanticAnalyzer::visit(ast::StringLiteral&) {}
void SemanticAnalyzer::visit(ast::VarRef&) {}

}  // namespace compiler::sema
