#include "ast/ast.h"

namespace compiler::ast {

void TranslationUnit::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void FunctionDecl::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void VarDecl::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void StructDecl::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void CompoundStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void IfStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void WhileStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ForStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ReturnStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ExprStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void BinaryExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void UnaryExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void CallExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void MemberExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ArraySubscript::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void IntLiteral::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void FloatLiteral::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void CharLiteral::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void StringLiteral::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void VarRef::accept(ASTVisitor& visitor) { visitor.visit(*this); }

}  // namespace compiler::ast
