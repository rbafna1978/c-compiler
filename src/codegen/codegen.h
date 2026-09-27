#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "ast/ast.h"

namespace compiler::codegen {

/** Lowers a type-checked AST (run sema first: it fills ASTNode::expr_type) to LLVM IR. */
class CodeGenerator : public ast::ASTVisitor {
 public:
  explicit CodeGenerator(const std::string& module_name = "module");

  /** Emits IR for the unit and verifies it. Returns false if diagnostics were produced. */
  bool generate(ast::TranslationUnit& unit);

  /** Returns the textual LLVM IR of the module. */
  std::string ir() const;

  const std::vector<std::string>& diagnostics() const;
  llvm::Module& module() { return *module_; }

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
  struct StructInfo {
    llvm::StructType* type;
    std::vector<ast::FieldDecl> fields;
  };

  llvm::Type* llvmType(const std::string& name);
  llvm::Value* eval(ast::ASTNode& node);
  llvm::Value* lvalue(ast::ASTNode& node);
  llvm::Value* lookupVar(const std::string& name);
  llvm::Value* convert(llvm::Value* v, const std::string& from, const std::string& to);
  llvm::Value* toBool(llvm::Value* v, const std::string& type);
  llvm::Value* binaryOp(const std::string& op, llvm::Value* l, const std::string& lt,
                        llvm::Value* r, const std::string& rt);
  llvm::AllocaInst* entryAlloca(llvm::Type* type, const std::string& name);
  // Tensors live in flat row-major storage; a tensor-typed expression evaluates to its address.
  void store(llvm::Value* dst, const std::string& dst_type, llvm::Value* src, const std::string& src_type);
  void assignTensor(llvm::Value* dst, const std::string& dst_type, llvm::Value* src,
                    const std::string& src_type);
  void forRange(long long n, const std::function<void(llvm::Value*)>& body);
  llvm::Value* elemPtr(llvm::Value* base, const std::string& elem, llvm::Value* index);
  llvm::Value* loadElem(llvm::Value* base, const std::string& elem, llvm::Value* index);
  llvm::Value* tensorOperand(llvm::Value* v, const std::string& type, llvm::Value* index,
                             const std::string& to_elem);
  llvm::Value* tensorBinary(const std::string& op, llvm::Value* l, const std::string& lt, llvm::Value* r,
                            const std::string& rt, const std::string& result);
  llvm::Value* emitBuiltin(ast::CallExpr& call);
  void emitPrint(ast::CallExpr& call);
  bool terminated() const;
  void error(int line, const std::string& message);

  llvm::LLVMContext ctx_;
  std::unique_ptr<llvm::Module> module_;
  llvm::IRBuilder<> builder_;
  llvm::Function* fn_ = nullptr;
  llvm::Value* value_ = nullptr;
  llvm::Value* sret_ = nullptr;  // out-pointer of the current function if it returns a tensor
  std::string current_return_;
  std::vector<std::unordered_map<std::string, llvm::Value*>> scopes_;
  std::unordered_map<std::string, StructInfo> structs_;  // key: "struct X"
  std::unordered_map<std::string, const ast::FunctionDecl*> funcs_;
  std::vector<std::string> diagnostics_;
};

}  // namespace compiler::codegen
