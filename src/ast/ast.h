#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace compiler::ast {

struct ASTVisitor;

/** Base AST node. */
struct ASTNode {
  virtual ~ASTNode() = default;
  virtual void accept(ASTVisitor& visitor) = 0;
  int line = 1;
};

struct TypeInfo {
  std::string name;
};

struct ParamDecl {
  std::string name;
  TypeInfo type;
};

struct FieldDecl {
  std::string name;
  TypeInfo type;
};

struct CompoundStmt;

struct TranslationUnit : ASTNode {
  std::vector<std::unique_ptr<ASTNode>> decls;
  void accept(ASTVisitor& visitor) override;
};

struct FunctionDecl : ASTNode {
  std::string name;
  TypeInfo return_type;
  std::vector<ParamDecl> params;
  std::unique_ptr<CompoundStmt> body;
  void accept(ASTVisitor& visitor) override;
};

struct VarDecl : ASTNode {
  std::string name;
  TypeInfo type;
  std::unique_ptr<ASTNode> init;
  void accept(ASTVisitor& visitor) override;
};

struct StructDecl : ASTNode {
  std::string name;
  std::vector<FieldDecl> fields;
  void accept(ASTVisitor& visitor) override;
};

struct CompoundStmt : ASTNode {
  std::vector<std::unique_ptr<ASTNode>> stmts;
  void accept(ASTVisitor& visitor) override;
};

struct IfStmt : ASTNode {
  std::unique_ptr<ASTNode> cond;
  std::unique_ptr<ASTNode> then_branch;
  std::unique_ptr<ASTNode> else_branch;
  void accept(ASTVisitor& visitor) override;
};

struct WhileStmt : ASTNode {
  std::unique_ptr<ASTNode> cond;
  std::unique_ptr<ASTNode> body;
  void accept(ASTVisitor& visitor) override;
};

struct ForStmt : ASTNode {
  std::unique_ptr<ASTNode> init;
  std::unique_ptr<ASTNode> cond;
  std::unique_ptr<ASTNode> incr;
  std::unique_ptr<ASTNode> body;
  void accept(ASTVisitor& visitor) override;
};

struct ReturnStmt : ASTNode {
  std::unique_ptr<ASTNode> value;
  void accept(ASTVisitor& visitor) override;
};

struct ExprStmt : ASTNode {
  std::unique_ptr<ASTNode> expr;
  void accept(ASTVisitor& visitor) override;
};

struct BinaryExpr : ASTNode {
  std::string op;
  std::unique_ptr<ASTNode> lhs;
  std::unique_ptr<ASTNode> rhs;
  void accept(ASTVisitor& visitor) override;
};

struct UnaryExpr : ASTNode {
  std::string op;
  std::unique_ptr<ASTNode> operand;
  void accept(ASTVisitor& visitor) override;
};

struct CallExpr : ASTNode {
  std::string callee;
  std::vector<std::unique_ptr<ASTNode>> args;
  void accept(ASTVisitor& visitor) override;
};

struct MemberExpr : ASTNode {
  std::unique_ptr<ASTNode> object;
  std::string member;
  bool is_arrow = false;
  void accept(ASTVisitor& visitor) override;
};

struct ArraySubscript : ASTNode {
  std::unique_ptr<ASTNode> array;
  std::unique_ptr<ASTNode> index;
  void accept(ASTVisitor& visitor) override;
};

struct IntLiteral : ASTNode {
  long long value = 0;
  void accept(ASTVisitor& visitor) override;
};

struct FloatLiteral : ASTNode {
  double value = 0.0;
  void accept(ASTVisitor& visitor) override;
};

struct CharLiteral : ASTNode {
  char value = '\0';
  void accept(ASTVisitor& visitor) override;
};

struct StringLiteral : ASTNode {
  std::string value;
  void accept(ASTVisitor& visitor) override;
};

struct VarRef : ASTNode {
  std::string name;
  void accept(ASTVisitor& visitor) override;
};

/** AST visitor interface. */
struct ASTVisitor {
  virtual ~ASTVisitor() = default;
  virtual void visit(TranslationUnit&) = 0;
  virtual void visit(FunctionDecl&) = 0;
  virtual void visit(VarDecl&) = 0;
  virtual void visit(StructDecl&) = 0;
  virtual void visit(CompoundStmt&) = 0;
  virtual void visit(IfStmt&) = 0;
  virtual void visit(WhileStmt&) = 0;
  virtual void visit(ForStmt&) = 0;
  virtual void visit(ReturnStmt&) = 0;
  virtual void visit(ExprStmt&) = 0;
  virtual void visit(BinaryExpr&) = 0;
  virtual void visit(UnaryExpr&) = 0;
  virtual void visit(CallExpr&) = 0;
  virtual void visit(MemberExpr&) = 0;
  virtual void visit(ArraySubscript&) = 0;
  virtual void visit(IntLiteral&) = 0;
  virtual void visit(FloatLiteral&) = 0;
  virtual void visit(CharLiteral&) = 0;
  virtual void visit(StringLiteral&) = 0;
  virtual void visit(VarRef&) = 0;
};

}  // namespace compiler::ast
