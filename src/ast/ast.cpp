#include "ast/ast.h"

#include <sstream>

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

namespace {

void indent(std::ostringstream& out, int level) {
  for (int i = 0; i < level; ++i) {
    out << "  ";
  }
}

void printNode(const ASTNode* node, std::ostringstream& out, int depth) {
  if (node == nullptr) {
    indent(out, depth);
    out << "<null>\n";
    return;
  }

  if (const auto* tu = dynamic_cast<const TranslationUnit*>(node)) {
    indent(out, depth);
    out << "TranslationUnit\n";
    for (const auto& decl : tu->decls) {
      printNode(decl.get(), out, depth + 1);
    }
    return;
  }

  if (const auto* fn = dynamic_cast<const FunctionDecl*>(node)) {
    indent(out, depth);
    out << "FunctionDecl " << fn->name << " -> " << fn->return_type.name << "\n";
    for (const auto& param : fn->params) {
      indent(out, depth + 1);
      out << "Param " << param.name << ":" << param.type.name << "\n";
    }
    printNode(fn->body.get(), out, depth + 1);
    return;
  }

  if (const auto* var = dynamic_cast<const VarDecl*>(node)) {
    indent(out, depth);
    out << "VarDecl " << var->name << ":" << var->type.name << "\n";
    if (var->init) {
      printNode(var->init.get(), out, depth + 1);
    }
    return;
  }

  if (const auto* st = dynamic_cast<const StructDecl*>(node)) {
    indent(out, depth);
    out << "StructDecl " << st->name << "\n";
    for (const auto& field : st->fields) {
      indent(out, depth + 1);
      out << "Field " << field.name << ":" << field.type.name << "\n";
    }
    return;
  }

  if (const auto* cs = dynamic_cast<const CompoundStmt*>(node)) {
    indent(out, depth);
    out << "CompoundStmt\n";
    for (const auto& stmt : cs->stmts) {
      printNode(stmt.get(), out, depth + 1);
    }
    return;
  }

  if (const auto* ifs = dynamic_cast<const IfStmt*>(node)) {
    indent(out, depth);
    out << "IfStmt\n";
    printNode(ifs->cond.get(), out, depth + 1);
    printNode(ifs->then_branch.get(), out, depth + 1);
    if (ifs->else_branch) {
      printNode(ifs->else_branch.get(), out, depth + 1);
    }
    return;
  }

  if (const auto* wh = dynamic_cast<const WhileStmt*>(node)) {
    indent(out, depth);
    out << "WhileStmt\n";
    printNode(wh->cond.get(), out, depth + 1);
    printNode(wh->body.get(), out, depth + 1);
    return;
  }

  if (const auto* fs = dynamic_cast<const ForStmt*>(node)) {
    indent(out, depth);
    out << "ForStmt\n";
    printNode(fs->init.get(), out, depth + 1);
    printNode(fs->cond.get(), out, depth + 1);
    printNode(fs->incr.get(), out, depth + 1);
    printNode(fs->body.get(), out, depth + 1);
    return;
  }

  if (const auto* rs = dynamic_cast<const ReturnStmt*>(node)) {
    indent(out, depth);
    out << "ReturnStmt\n";
    printNode(rs->value.get(), out, depth + 1);
    return;
  }

  if (const auto* es = dynamic_cast<const ExprStmt*>(node)) {
    indent(out, depth);
    out << "ExprStmt\n";
    printNode(es->expr.get(), out, depth + 1);
    return;
  }

  if (const auto* be = dynamic_cast<const BinaryExpr*>(node)) {
    indent(out, depth);
    out << "BinaryExpr " << be->op << "\n";
    printNode(be->lhs.get(), out, depth + 1);
    printNode(be->rhs.get(), out, depth + 1);
    return;
  }

  if (const auto* ue = dynamic_cast<const UnaryExpr*>(node)) {
    indent(out, depth);
    out << "UnaryExpr " << ue->op << "\n";
    printNode(ue->operand.get(), out, depth + 1);
    return;
  }

  if (const auto* ce = dynamic_cast<const CallExpr*>(node)) {
    indent(out, depth);
    out << "CallExpr " << ce->callee << "\n";
    for (const auto& arg : ce->args) {
      printNode(arg.get(), out, depth + 1);
    }
    return;
  }

  if (const auto* me = dynamic_cast<const MemberExpr*>(node)) {
    indent(out, depth);
    out << "MemberExpr " << (me->is_arrow ? "->" : ".") << me->member << "\n";
    printNode(me->object.get(), out, depth + 1);
    return;
  }

  if (const auto* as = dynamic_cast<const ArraySubscript*>(node)) {
    indent(out, depth);
    out << "ArraySubscript\n";
    printNode(as->array.get(), out, depth + 1);
    printNode(as->index.get(), out, depth + 1);
    return;
  }

  if (const auto* il = dynamic_cast<const IntLiteral*>(node)) {
    indent(out, depth);
    out << "IntLiteral " << il->value << "\n";
    return;
  }

  if (const auto* fl = dynamic_cast<const FloatLiteral*>(node)) {
    indent(out, depth);
    out << "FloatLiteral " << fl->value << "\n";
    return;
  }

  if (const auto* cl = dynamic_cast<const CharLiteral*>(node)) {
    indent(out, depth);
    out << "CharLiteral " << static_cast<int>(cl->value) << "\n";
    return;
  }

  if (const auto* sl = dynamic_cast<const StringLiteral*>(node)) {
    indent(out, depth);
    out << "StringLiteral \"" << sl->value << "\"\n";
    return;
  }

  if (const auto* vr = dynamic_cast<const VarRef*>(node)) {
    indent(out, depth);
    out << "VarRef " << vr->name << "\n";
    return;
  }

  indent(out, depth);
  out << "<unknown-node>\n";
}

}  // namespace

std::string prettyPrint(const ASTNode& node) {
  std::ostringstream out;
  printNode(&node, out, 0);
  return out.str();
}

}  // namespace compiler::ast
