#include "codegen/codegen.h"

#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

namespace compiler::codegen {

using llvm::BasicBlock;
using llvm::CmpInst;
using llvm::Value;

namespace {

bool isPtr(const std::string& t) { return !t.empty() && t.back() == '*'; }
std::string pointee(const std::string& t) { return t.substr(0, t.size() - 1); }

bool isConstExpr(const ast::ASTNode& n) {
  if (dynamic_cast<const ast::IntLiteral*>(&n) || dynamic_cast<const ast::FloatLiteral*>(&n) ||
      dynamic_cast<const ast::CharLiteral*>(&n)) {
    return true;
  }
  if (const auto* u = dynamic_cast<const ast::UnaryExpr*>(&n)) {
    return (u->op == "-" || u->op == "!") && isConstExpr(*u->operand);
  }
  if (const auto* b = dynamic_cast<const ast::BinaryExpr*>(&n)) {
    static const char* kOps[] = {"+", "-", "*", "/", "%", "==", "!=", "<", ">", "<=", ">="};
    for (const char* op : kOps) {
      if (b->op == op) {
        return isConstExpr(*b->lhs) && isConstExpr(*b->rhs);
      }
    }
  }
  return false;
}

bool isLvalueNode(const ast::ASTNode& n) {
  if (dynamic_cast<const ast::VarRef*>(&n) || dynamic_cast<const ast::MemberExpr*>(&n) ||
      dynamic_cast<const ast::ArraySubscript*>(&n)) {
    return true;
  }
  const auto* u = dynamic_cast<const ast::UnaryExpr*>(&n);
  return u != nullptr && u->op == "*";
}

CmpInst::Predicate predicate(const std::string& op, bool is_float, bool is_ptr) {
  const int k = op == "==" ? 0 : op == "!=" ? 1 : op == "<" ? 2 : op == ">" ? 3 : op == "<=" ? 4 : 5;
  static const CmpInst::Predicate kFloat[] = {CmpInst::FCMP_OEQ, CmpInst::FCMP_UNE, CmpInst::FCMP_OLT,
                                              CmpInst::FCMP_OGT, CmpInst::FCMP_OLE, CmpInst::FCMP_OGE};
  static const CmpInst::Predicate kSigned[] = {CmpInst::ICMP_EQ, CmpInst::ICMP_NE, CmpInst::ICMP_SLT,
                                               CmpInst::ICMP_SGT, CmpInst::ICMP_SLE, CmpInst::ICMP_SGE};
  static const CmpInst::Predicate kUnsigned[] = {CmpInst::ICMP_EQ, CmpInst::ICMP_NE, CmpInst::ICMP_ULT,
                                                 CmpInst::ICMP_UGT, CmpInst::ICMP_ULE, CmpInst::ICMP_UGE};
  return is_float ? kFloat[k] : is_ptr ? kUnsigned[k] : kSigned[k];
}

}  // namespace

CodeGenerator::CodeGenerator(const std::string& module_name)
    : module_(std::make_unique<llvm::Module>(module_name, ctx_)), builder_(ctx_) {}

bool CodeGenerator::generate(ast::TranslationUnit& unit) {
  diagnostics_.clear();
  unit.accept(*this);
  std::string msg;
  llvm::raw_string_ostream os(msg);
  if (llvm::verifyModule(*module_, &os)) {
    diagnostics_.push_back("internal error: invalid IR: " + os.str());
  }
  return diagnostics_.empty();
}

std::string CodeGenerator::ir() const {
  std::string text;
  llvm::raw_string_ostream os(text);
  module_->print(os, nullptr);
  return os.str();
}

const std::vector<std::string>& CodeGenerator::diagnostics() const { return diagnostics_; }

void CodeGenerator::error(int line, const std::string& message) {
  diagnostics_.push_back("line " + std::to_string(line) + ": " + message);
}

bool CodeGenerator::terminated() const { return builder_.GetInsertBlock()->getTerminator() != nullptr; }

llvm::Type* CodeGenerator::llvmType(const std::string& t) {
  if (t == "int") return builder_.getInt32Ty();
  if (t == "char") return builder_.getInt8Ty();
  if (t == "float") return builder_.getFloatTy();
  if (t == "void") return builder_.getVoidTy();
  if (isPtr(t)) return builder_.getPtrTy();
  return structs_.at(t).type;
}

Value* CodeGenerator::eval(ast::ASTNode& node) {
  value_ = nullptr;
  node.accept(*this);
  return value_;
}

llvm::AllocaInst* CodeGenerator::entryAlloca(llvm::Type* type, const std::string& name) {
  llvm::IRBuilder<> entry(&fn_->getEntryBlock(), fn_->getEntryBlock().begin());
  return entry.CreateAlloca(type, nullptr, name);
}

Value* CodeGenerator::lookupVar(const std::string& name) {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    if (auto found = it->find(name); found != it->end()) {
      return found->second;
    }
  }
  return module_->getNamedGlobal(name);
}

Value* CodeGenerator::lvalue(ast::ASTNode& n) {
  if (auto* v = dynamic_cast<ast::VarRef*>(&n)) {
    return lookupVar(v->name);
  }
  if (auto* m = dynamic_cast<ast::MemberExpr*>(&n)) {
    const std::string& obj_type = m->object->expr_type.name;
    Value* base;
    if (m->is_arrow) {
      base = eval(*m->object);
    } else if (isLvalueNode(*m->object)) {
      base = lvalue(*m->object);
    } else {  // rvalue struct (e.g. call result): spill to a temporary
      base = entryAlloca(llvmType(obj_type), "tmp");
      builder_.CreateStore(eval(*m->object), base);
    }
    const auto& info = structs_.at(m->is_arrow ? pointee(obj_type) : obj_type);
    for (unsigned i = 0; i < info.fields.size(); ++i) {
      if (info.fields[i].name == m->member) {
        return builder_.CreateStructGEP(info.type, base, i);
      }
    }
  }
  if (auto* s = dynamic_cast<ast::ArraySubscript*>(&n)) {
    Value* base = eval(*s->array);
    Value* idx = convert(eval(*s->index), s->index->expr_type.name, "int");
    return builder_.CreateGEP(llvmType(s->expr_type.name), base, idx);
  }
  if (auto* u = dynamic_cast<ast::UnaryExpr*>(&n); u && u->op == "*") {
    return eval(*u->operand);
  }
  error(n.line, "expression is not addressable");
  return llvm::UndefValue::get(builder_.getPtrTy());
}

Value* CodeGenerator::convert(Value* v, const std::string& from, const std::string& to) {
  if (from == to) return v;
  if (to == "float") return builder_.CreateSIToFP(v, builder_.getFloatTy());
  if (from == "float") return builder_.CreateFPToSI(v, llvmType(to));
  return builder_.CreateIntCast(v, llvmType(to), /*isSigned=*/true);
}

Value* CodeGenerator::toBool(Value* v, const std::string& type) {
  if (type == "float") return builder_.CreateFCmpUNE(v, llvm::ConstantFP::get(v->getType(), 0.0));
  return builder_.CreateICmpNE(v, llvm::Constant::getNullValue(v->getType()));
}

Value* CodeGenerator::binaryOp(const std::string& op, Value* l, const std::string& lt, Value* r,
                               const std::string& rt) {
  const bool is_cmp = op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=";
  auto as_int = [&](Value* cmp) { return builder_.CreateZExt(cmp, builder_.getInt32Ty()); };

  if (isPtr(lt) || isPtr(rt)) {
    if (is_cmp) {
      return as_int(builder_.CreateCmp(predicate(op, false, true), l, r));
    }
    const bool lhs_ptr = isPtr(lt);  // ptr + int, int + ptr, ptr - int
    Value* idx = convert(lhs_ptr ? r : l, lhs_ptr ? rt : lt, "int");
    if (op == "-") idx = builder_.CreateNeg(idx);
    return builder_.CreateGEP(llvmType(pointee(lhs_ptr ? lt : rt)), lhs_ptr ? l : r, idx);
  }

  const bool f = lt == "float" || rt == "float";
  const std::string common = f ? "float" : "int";
  l = convert(l, lt, common);
  r = convert(r, rt, common);
  if (is_cmp) return as_int(builder_.CreateCmp(predicate(op, f, false), l, r));
  if (op == "+") return f ? builder_.CreateFAdd(l, r) : builder_.CreateAdd(l, r);
  if (op == "-") return f ? builder_.CreateFSub(l, r) : builder_.CreateSub(l, r);
  if (op == "*") return f ? builder_.CreateFMul(l, r) : builder_.CreateMul(l, r);
  if (op == "/") return f ? builder_.CreateFDiv(l, r) : builder_.CreateSDiv(l, r);
  return builder_.CreateSRem(l, r);  // "%": sema guarantees integer operands
}

void CodeGenerator::visit(ast::TranslationUnit& n) {
  for (auto& d : n.decls) {
    d->accept(*this);
  }
}

void CodeGenerator::visit(ast::StructDecl& n) {
  auto* type = llvm::StructType::create(ctx_, "struct." + n.name);
  std::vector<llvm::Type*> fields;
  for (const auto& f : n.fields) {
    fields.push_back(llvmType(f.type.name));
  }
  type->setBody(fields);
  structs_["struct " + n.name] = {type, n.fields};
}

void CodeGenerator::visit(ast::FunctionDecl& n) {
  std::vector<llvm::Type*> params;
  for (const auto& p : n.params) {
    params.push_back(llvmType(p.type.name));
  }
  auto* fty = llvm::FunctionType::get(llvmType(n.return_type.name), params, false);
  fn_ = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, n.name, *module_);
  funcs_[n.name] = &n;
  current_return_ = n.return_type.name;

  builder_.SetInsertPoint(BasicBlock::Create(ctx_, "entry", fn_));
  scopes_.emplace_back();
  size_t i = 0;
  for (auto& arg : fn_->args()) {
    const auto& p = n.params[i++];
    arg.setName(p.name);
    auto* slot = entryAlloca(arg.getType(), p.name);
    builder_.CreateStore(&arg, slot);
    scopes_.back()[p.name] = slot;
  }
  n.body->accept(*this);
  if (!terminated()) {  // implicit return: 0 / zeroinitializer (matches C's main)
    if (fn_->getReturnType()->isVoidTy()) {
      builder_.CreateRetVoid();
    } else {
      builder_.CreateRet(llvm::Constant::getNullValue(fn_->getReturnType()));
    }
  }
  scopes_.pop_back();
  builder_.ClearInsertionPoint();
  fn_ = nullptr;
}

void CodeGenerator::visit(ast::VarDecl& n) {
  llvm::Type* type = llvmType(n.type.name);
  if (fn_ == nullptr) {
    llvm::Constant* init = llvm::Constant::getNullValue(type);
    if (n.init) {
      if (!isConstExpr(*n.init)) {
        error(n.line, "initializer of global '" + n.name + "' is not a constant");
        return;
      }
      init = llvm::cast<llvm::Constant>(convert(eval(*n.init), n.init->expr_type.name, n.type.name));
    }
    new llvm::GlobalVariable(*module_, type, false, llvm::GlobalValue::ExternalLinkage, init, n.name);
    return;
  }
  auto* slot = entryAlloca(type, n.name);
  if (n.init) {
    builder_.CreateStore(convert(eval(*n.init), n.init->expr_type.name, n.type.name), slot);
  }
  scopes_.back()[n.name] = slot;
}

void CodeGenerator::visit(ast::CompoundStmt& n) {
  scopes_.emplace_back();
  for (auto& s : n.stmts) {
    if (terminated()) break;  // dead code after return
    s->accept(*this);
  }
  scopes_.pop_back();
}

void CodeGenerator::visit(ast::IfStmt& n) {
  Value* cond = toBool(eval(*n.cond), n.cond->expr_type.name);
  auto* then_bb = BasicBlock::Create(ctx_, "then", fn_);
  auto* else_bb = n.else_branch ? BasicBlock::Create(ctx_, "else", fn_) : nullptr;
  auto* end_bb = BasicBlock::Create(ctx_, "endif", fn_);
  builder_.CreateCondBr(cond, then_bb, else_bb ? else_bb : end_bb);

  builder_.SetInsertPoint(then_bb);
  n.then_branch->accept(*this);
  if (!terminated()) builder_.CreateBr(end_bb);
  if (else_bb) {
    builder_.SetInsertPoint(else_bb);
    n.else_branch->accept(*this);
    if (!terminated()) builder_.CreateBr(end_bb);
  }
  builder_.SetInsertPoint(end_bb);
}

void CodeGenerator::visit(ast::WhileStmt& n) {
  auto* cond_bb = BasicBlock::Create(ctx_, "while.cond", fn_);
  auto* body_bb = BasicBlock::Create(ctx_, "while.body", fn_);
  auto* end_bb = BasicBlock::Create(ctx_, "while.end", fn_);
  builder_.CreateBr(cond_bb);
  builder_.SetInsertPoint(cond_bb);
  builder_.CreateCondBr(toBool(eval(*n.cond), n.cond->expr_type.name), body_bb, end_bb);
  builder_.SetInsertPoint(body_bb);
  n.body->accept(*this);
  if (!terminated()) builder_.CreateBr(cond_bb);
  builder_.SetInsertPoint(end_bb);
}

void CodeGenerator::visit(ast::ForStmt& n) {
  scopes_.emplace_back();
  if (n.init) n.init->accept(*this);
  auto* cond_bb = BasicBlock::Create(ctx_, "for.cond", fn_);
  auto* body_bb = BasicBlock::Create(ctx_, "for.body", fn_);
  auto* incr_bb = BasicBlock::Create(ctx_, "for.incr", fn_);
  auto* end_bb = BasicBlock::Create(ctx_, "for.end", fn_);
  builder_.CreateBr(cond_bb);
  builder_.SetInsertPoint(cond_bb);
  if (n.cond) {
    builder_.CreateCondBr(toBool(eval(*n.cond), n.cond->expr_type.name), body_bb, end_bb);
  } else {
    builder_.CreateBr(body_bb);
  }
  builder_.SetInsertPoint(body_bb);
  n.body->accept(*this);
  if (!terminated()) builder_.CreateBr(incr_bb);
  builder_.SetInsertPoint(incr_bb);
  if (n.incr) eval(*n.incr);
  builder_.CreateBr(cond_bb);
  builder_.SetInsertPoint(end_bb);
  scopes_.pop_back();
}

void CodeGenerator::visit(ast::ReturnStmt& n) {
  if (!n.value) {
    builder_.CreateRetVoid();
    return;
  }
  builder_.CreateRet(convert(eval(*n.value), n.value->expr_type.name, current_return_));
}

void CodeGenerator::visit(ast::ExprStmt& n) {
  if (n.expr) eval(*n.expr);
}

void CodeGenerator::visit(ast::BinaryExpr& n) {
  const std::string& op = n.op;
  const std::string& lt = n.lhs->expr_type.name;
  const std::string& rt = n.rhs->expr_type.name;

  if (op == "&&" || op == "||") {
    const bool is_and = op == "&&";
    Value* l = toBool(eval(*n.lhs), lt);
    auto* lhs_end = builder_.GetInsertBlock();
    auto* rhs_bb = BasicBlock::Create(ctx_, "logic.rhs", fn_);
    auto* end_bb = BasicBlock::Create(ctx_, "logic.end", fn_);
    builder_.CreateCondBr(l, is_and ? rhs_bb : end_bb, is_and ? end_bb : rhs_bb);
    builder_.SetInsertPoint(rhs_bb);
    Value* r = toBool(eval(*n.rhs), rt);
    auto* rhs_end = builder_.GetInsertBlock();
    builder_.CreateBr(end_bb);
    builder_.SetInsertPoint(end_bb);
    auto* phi = builder_.CreatePHI(builder_.getInt1Ty(), 2);
    phi->addIncoming(builder_.getInt1(!is_and), lhs_end);
    phi->addIncoming(r, rhs_end);
    value_ = builder_.CreateZExt(phi, builder_.getInt32Ty());
    return;
  }

  if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=") {
    Value* addr = lvalue(*n.lhs);
    Value* r = eval(*n.rhs);
    Value* v = r;
    if (op == "=") {
      v = convert(r, rt, lt);
    } else {
      Value* cur = builder_.CreateLoad(llvmType(lt), addr);
      const std::string common = (lt == "float" || rt == "float") ? "float" : "int";
      v = convert(binaryOp(op.substr(0, 1), cur, lt, r, rt), common, lt);
    }
    builder_.CreateStore(v, addr);
    value_ = v;
    return;
  }

  Value* l = eval(*n.lhs);
  Value* r = eval(*n.rhs);
  value_ = binaryOp(op, l, lt, r, rt);
}

void CodeGenerator::visit(ast::UnaryExpr& n) {
  if (n.op == "&") {
    value_ = lvalue(*n.operand);
    return;
  }
  Value* v = eval(*n.operand);
  const std::string& t = n.operand->expr_type.name;
  if (n.op == "*") {
    value_ = builder_.CreateLoad(llvmType(n.expr_type.name), v);
  } else if (n.op == "!") {
    value_ = builder_.CreateZExt(builder_.CreateNot(toBool(v, t)), builder_.getInt32Ty());
  } else {  // "-"
    value_ = t == "float" ? builder_.CreateFNeg(v) : builder_.CreateNeg(convert(v, t, "int"));
  }
}

void CodeGenerator::visit(ast::CallExpr& n) {
  const auto* decl = funcs_.at(n.callee);
  std::vector<Value*> args;
  for (size_t i = 0; i < n.args.size(); ++i) {
    args.push_back(convert(eval(*n.args[i]), n.args[i]->expr_type.name, decl->params[i].type.name));
  }
  value_ = builder_.CreateCall(module_->getFunction(n.callee), args);
}

void CodeGenerator::visit(ast::MemberExpr& n) {
  Value* addr = lvalue(n);
  value_ = builder_.CreateLoad(llvmType(n.expr_type.name), addr);
}

void CodeGenerator::visit(ast::ArraySubscript& n) {
  Value* addr = lvalue(n);
  value_ = builder_.CreateLoad(llvmType(n.expr_type.name), addr);
}

void CodeGenerator::visit(ast::VarRef& n) {
  Value* addr = lvalue(n);
  value_ = builder_.CreateLoad(llvmType(n.expr_type.name), addr);
}

void CodeGenerator::visit(ast::IntLiteral& n) { value_ = builder_.getInt32(static_cast<uint32_t>(n.value)); }
void CodeGenerator::visit(ast::FloatLiteral& n) {
  value_ = llvm::ConstantFP::get(builder_.getFloatTy(), n.value);
}
void CodeGenerator::visit(ast::CharLiteral& n) { value_ = builder_.getInt8(static_cast<uint8_t>(n.value)); }
void CodeGenerator::visit(ast::StringLiteral& n) {
  value_ = builder_.CreateGlobalStringPtr(n.value, "str", 0, module_.get());
}

}  // namespace compiler::codegen
