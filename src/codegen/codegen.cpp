#include "codegen/codegen.h"

#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <stdexcept>

#include "ast/tensor_type.h"

namespace compiler::codegen {

using llvm::BasicBlock;
using llvm::CmpInst;
using llvm::Value;
using ast::isTensor;
using ast::parseTensor;
using ast::promote;
using ast::Shape;
using ast::tensorName;

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

CodeGenerator::CodeGenerator(const std::string& module_name, bool fuse, bool tile)
    : module_(std::make_unique<llvm::Module>(module_name, ctx_)), builder_(ctx_), fuse_(fuse), tile_(tile) {}

bool CodeGenerator::generate(ast::TranslationUnit& unit) {
  diagnostics_.clear();
  try {
    unit.accept(*this);
  } catch (const std::runtime_error& e) {  // features sema accepts but codegen does not lower yet
    diagnostics_.push_back(e.what());
    return false;
  }
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
  if (isTensor(t)) {
    const Shape s = parseTensor(t);
    return llvm::ArrayType::get(llvmType(s.elem), s.size());
  }
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
    const std::string& array_type = s->array->expr_type.name;
    if (isTensor(array_type)) {  // A[i]: skip i rows of the remaining dimensions
      const Shape a = parseTensor(array_type);
      long long stride = 1;
      for (size_t d = 1; d < a.dims.size(); ++d) stride *= a.dims[d];
      idx = builder_.CreateMul(idx, builder_.getInt32(static_cast<uint32_t>(stride)));
      return builder_.CreateGEP(llvmType(a.elem), base, idx);
    }
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

Value* CodeGenerator::elemPtr(Value* base, const std::string& elem, Value* index) {
  return builder_.CreateGEP(llvmType(elem), base, index);
}

Value* CodeGenerator::loadElem(Value* base, const std::string& elem, Value* index) {
  return builder_.CreateLoad(llvmType(elem), elemPtr(base, elem, index));
}

// Loads element `index` of tensor `v`, or passes a scalar through, converted to `to_elem`.
Value* CodeGenerator::tensorOperand(Value* v, const std::string& type, Value* index,
                                    const std::string& to_elem) {
  if (isTensor(type)) {
    const std::string elem = parseTensor(type).elem;
    return convert(loadElem(v, elem, index), elem, to_elem);
  }
  return convert(v, type, to_elem);
}

void CodeGenerator::assignTensor(Value* dst, const std::string& dst_type, Value* src,
                                 const std::string& src_type) {
  const Shape d = parseTensor(dst_type), s = parseTensor(src_type);
  if (d.elem == s.elem) {  // int and float are both 4 bytes
    builder_.CreateMemMove(dst, llvm::MaybeAlign(4), src, llvm::MaybeAlign(4),
                           builder_.getInt64(static_cast<uint64_t>(d.size()) * 4));
    return;
  }
  forRange(d.size(), [&](Value* i) {
    builder_.CreateStore(convert(loadElem(src, s.elem, i), s.elem, d.elem), elemPtr(dst, d.elem, i));
  });
}

void CodeGenerator::store(Value* dst, const std::string& dst_type, Value* src, const std::string& src_type) {
  if (isTensor(dst_type)) {
    assignTensor(dst, dst_type, src, src_type);
  } else {
    builder_.CreateStore(convert(src, src_type, dst_type), dst);
  }
}

// Elementwise op over same-shape tensors; a scalar operand broadcasts.
Value* CodeGenerator::tensorBinary(const std::string& op, Value* l, const std::string& lt, Value* r,
                                   const std::string& rt, const std::string& result) {
  const Shape rs = parseTensor(result);
  auto* out = tensorAlloc(result, "t");
  forRange(rs.size(), [&](Value* i) {
    Value* v = binaryOp(op, tensorOperand(l, lt, i, rs.elem), rs.elem, tensorOperand(r, rt, i, rs.elem), rs.elem);
    builder_.CreateStore(v, elemPtr(out, rs.elem, i));
  });
  return out;
}

Value* CodeGenerator::emitBuiltin(ast::CallExpr& n) {
  const std::string& name = n.callee;
  if (name == "print") {
    emitPrint(n);
    return nullptr;
  }
  if (name == "exp" || name == "log" || name == "tanh") {  // libm's expf / logf / tanhf
    auto* f32 = builder_.getFloatTy();
    auto fn = module_->getOrInsertFunction(name + "f", llvm::FunctionType::get(f32, {f32}, false));
    const std::string& t = n.args[0]->expr_type.name;
    Value* v = eval(*n.args[0]);
    if (!isTensor(t)) return builder_.CreateCall(fn, {convert(v, t, "float")});
    const Shape s = parseTensor(t);
    auto* out = tensorAlloc(n.expr_type.name, "t");
    forRange(s.size(), [&](Value* i) {
      Value* x = convert(loadElem(v, s.elem, i), s.elem, "float");
      builder_.CreateStore(builder_.CreateCall(fn, {x}), elemPtr(out, "float", i));
    });
    return out;
  }
  Value* a = eval(*n.args[0]);
  const std::string& at = n.args[0]->expr_type.name;
  const Shape as = parseTensor(at);

  if (name == "sum") {
    auto* acc = entryAlloca(llvmType(as.elem), "acc");
    builder_.CreateStore(llvm::Constant::getNullValue(llvmType(as.elem)), acc);
    forRange(as.size(), [&](Value* i) {
      Value* cur = builder_.CreateLoad(llvmType(as.elem), acc);
      builder_.CreateStore(binaryOp("+", cur, as.elem, loadElem(a, as.elem, i), as.elem), acc);
    });
    return builder_.CreateLoad(llvmType(as.elem), acc);
  }

  const std::string& rt = n.expr_type.name;
  const Shape rs = parseTensor(rt);

  if (name == "transpose") {
    auto* out = tensorAlloc(rt, "t");
    forRange(as.dims[0], [&](Value* i) {
      forRange(as.dims[1], [&](Value* j) {
        builder_.CreateStore(loadElem(a, as.elem, flatIndex(i, as.dims[1], j)), elemPtr(out, as.elem, flatIndex(j, as.dims[0], i)));
      });
    });
    return out;
  }

  if (name == "matmul") {
    Value* b = eval(*n.args[1]);
    const Shape bs = parseTensor(n.args[1]->expr_type.name);
    const long long m = as.dims[0], k = as.dims[1], nn = bs.dims[1];
    constexpr long long kTileBlock = 32;
    if (tile_ && m % kTileBlock == 0 && k % kTileBlock == 0 && nn % kTileBlock == 0 && m >= kTileBlock &&
        k >= kTileBlock && nn >= kTileBlock) {
      return emitTiledMatmul(a, as.elem, b, bs.elem, m, k, nn, rt, kTileBlock);
    }
    auto* out = tensorAlloc(rt, "t");
    auto* acc = entryAlloca(llvmType(rs.elem), "acc");
    forRange(m, [&](Value* i) {
      forRange(nn, [&](Value* j) {
        builder_.CreateStore(llvm::Constant::getNullValue(llvmType(rs.elem)), acc);
        forRange(k, [&](Value* p) {
          Value* x = convert(loadElem(a, as.elem, flatIndex(i, k, p)), as.elem, rs.elem);
          Value* y = convert(loadElem(b, bs.elem, flatIndex(p, nn, j)), bs.elem, rs.elem);
          Value* cur = builder_.CreateLoad(llvmType(rs.elem), acc);
          builder_.CreateStore(binaryOp("+", cur, rs.elem, binaryOp("*", x, rs.elem, y, rs.elem), rs.elem), acc);
        });
        builder_.CreateStore(builder_.CreateLoad(llvmType(rs.elem), acc), elemPtr(out, rs.elem, flatIndex(i, nn, j)));
      });
    });
    return out;
  }
  throw std::runtime_error("builtin '" + name + "' is not implemented yet");
}

// print(x): ints as %d, floats as %g, strings as %s; tensors one innermost row per line.
void CodeGenerator::emitPrint(ast::CallExpr& n) {
  auto printf_fn = module_->getOrInsertFunction(
      "printf", llvm::FunctionType::get(builder_.getInt32Ty(), {builder_.getPtrTy()}, /*isVarArg=*/true));
  auto cstr = [&](const char* text) { return builder_.CreateGlobalStringPtr(text, "fmt", 0, module_.get()); };
  const std::string& t = n.args[0]->expr_type.name;
  Value* v = eval(*n.args[0]);

  if (t == "char*") {
    builder_.CreateCall(printf_fn, {cstr("%s\n"), v});
  } else if (t == "float") {
    builder_.CreateCall(printf_fn, {cstr("%g\n"), builder_.CreateFPExt(v, builder_.getDoubleTy())});
  } else if (!isTensor(t)) {
    builder_.CreateCall(printf_fn, {cstr("%d\n"), convert(v, t, "int")});
  } else {
    const Shape s = parseTensor(t);
    const bool is_float = s.elem == "float";
    Value* fmt = cstr(is_float ? "%s%g%s" : "%s%d%s");
    Value* space = cstr(" ");
    Value* empty = cstr("");
    Value* newline = cstr("\n");
    const auto row = builder_.getInt32(static_cast<uint32_t>(s.dims.back()));
    forRange(s.size(), [&](Value* i) {
      Value* x = loadElem(v, s.elem, i);
      if (is_float) x = builder_.CreateFPExt(x, builder_.getDoubleTy());
      Value* sep = builder_.CreateSelect(builder_.CreateICmpNE(builder_.CreateSRem(i, row), builder_.getInt32(0)), space, empty);
      Value* next = builder_.CreateAdd(i, builder_.getInt32(1));
      Value* tail = builder_.CreateSelect(builder_.CreateICmpEQ(builder_.CreateSRem(next, row), builder_.getInt32(0)), newline, empty);
      builder_.CreateCall(printf_fn, {fmt, sep, x, tail});
    });
  }
}

Value* CodeGenerator::tensorAlloc(const std::string& tensor_type, const std::string& name) {
  const Shape s = parseTensor(tensor_type);
  const uint64_t bytes = static_cast<uint64_t>(s.size()) * 4;
  // ponytail: above this size a tensor moves to the heap (never freed) so a benchmark-sized
  // matrix (e.g. 256x256 floats = 256 KiB) doesn't overflow the stack; every allocation here is a
  // temporary or local of a batch-style, run-once-and-exit program. Upgrade to a per-call arena
  // (free everything when the function returns) if this ever backs a long-running service.
  constexpr uint64_t kHeapThreshold = 16384;  // 16 KiB
  if (bytes <= kHeapThreshold) return entryAlloca(llvmType(tensor_type), name);
  auto malloc_fn = module_->getOrInsertFunction(
      "malloc", llvm::FunctionType::get(builder_.getPtrTy(), {builder_.getInt64Ty()}, false));
  return builder_.CreateCall(malloc_fn, {builder_.getInt64(bytes)}, name);
}

Value* CodeGenerator::flatIndex(Value* i, long long stride, Value* j) {
  return builder_.CreateAdd(builder_.CreateMul(i, builder_.getInt32(static_cast<uint32_t>(stride))), j);
}

// Emits `for (i = 0; i < n; i += step) body(i)`. The counter lives in the entry block (mem2reg
// cleans it up). `step` > 1 drives the outer loops of a tiled iteration space.
void CodeGenerator::forRange(long long n, const std::function<void(Value*)>& body, long long step) {
  auto* counter = entryAlloca(builder_.getInt32Ty(), "i");
  builder_.CreateStore(builder_.getInt32(0), counter);
  auto* cond_bb = BasicBlock::Create(ctx_, "loop.cond", fn_);
  auto* body_bb = BasicBlock::Create(ctx_, "loop.body", fn_);
  auto* end_bb = BasicBlock::Create(ctx_, "loop.end", fn_);
  builder_.CreateBr(cond_bb);
  builder_.SetInsertPoint(cond_bb);
  Value* i = builder_.CreateLoad(builder_.getInt32Ty(), counter);
  builder_.CreateCondBr(builder_.CreateICmpSLT(i, builder_.getInt32(static_cast<uint32_t>(n))), body_bb, end_bb);
  builder_.SetInsertPoint(body_bb);
  body(i);
  builder_.CreateStore(builder_.CreateAdd(i, builder_.getInt32(static_cast<uint32_t>(step))), counter);
  builder_.CreateBr(cond_bb);
  builder_.SetInsertPoint(end_bb);
}

// elem type of a leaf/node's expr_type: itself if scalar, its element type if a tensor.
namespace {
std::string elemOf(const std::string& t) { return isTensor(t) ? parseTensor(t).elem : t; }
}  // namespace

bool CodeGenerator::isFusableOp(const ast::ASTNode& e) const {
  if (const auto* b = dynamic_cast<const ast::BinaryExpr*>(&e)) {
    return b->op == "+" || b->op == "-" || b->op == "*" || b->op == "/";
  }
  if (const auto* u = dynamic_cast<const ast::UnaryExpr*>(&e)) {
    return u->op == "-";
  }
  if (const auto* c = dynamic_cast<const ast::CallExpr*>(&e); c != nullptr && !funcs_.count(c->callee)) {
    return c->callee == "exp" || c->callee == "log" || c->callee == "tanh";
  }
  return false;
}

// Evaluates every non-fusable leaf of `e` exactly once (a tensor's address, or a scalar's value)
// and records it, so buildFusedElem can read each leaf many times (once per loop iteration)
// without re-evaluating or re-running any side effects.
void CodeGenerator::prepareFusionLeaves(ast::ASTNode& e, std::unordered_map<const ast::ASTNode*, Value*>& leaves) {
  if (isFusableOp(e)) {
    if (auto* b = dynamic_cast<ast::BinaryExpr*>(&e)) {
      prepareFusionLeaves(*b->lhs, leaves);
      prepareFusionLeaves(*b->rhs, leaves);
      return;
    }
    if (auto* u = dynamic_cast<ast::UnaryExpr*>(&e)) {
      prepareFusionLeaves(*u->operand, leaves);
      return;
    }
    auto* c = static_cast<ast::CallExpr*>(&e);
    prepareFusionLeaves(*c->args[0], leaves);
    return;
  }
  leaves[&e] = eval(e);
}

// Builds the scalar value of `e` at flat index `index`, in e's own element type (its caller
// promotes as needed) -- mirrors exactly what the per-op path (tensorOperand + binaryOp) would
// compute for `e` alone, so fusing changes nothing about int-truncation or float-promotion order.
Value* CodeGenerator::buildFusedElem(ast::ASTNode& e, Value* index,
                                    const std::unordered_map<const ast::ASTNode*, Value*>& leaves) {
  const std::string node_elem = elemOf(e.expr_type.name);
  if (!isFusableOp(e)) {
    Value* v = leaves.at(&e);
    return isTensor(e.expr_type.name) ? loadElem(v, node_elem, index) : v;
  }
  if (auto* b = dynamic_cast<ast::BinaryExpr*>(&e)) {
    Value* l = convert(buildFusedElem(*b->lhs, index, leaves), elemOf(b->lhs->expr_type.name), node_elem);
    Value* r = convert(buildFusedElem(*b->rhs, index, leaves), elemOf(b->rhs->expr_type.name), node_elem);
    return binaryOp(b->op, l, node_elem, r, node_elem);
  }
  if (auto* u = dynamic_cast<ast::UnaryExpr*>(&e)) {
    Value* x = buildFusedElem(*u->operand, index, leaves);  // unary '-' preserves the element type
    return node_elem == "float" ? builder_.CreateFNeg(x) : builder_.CreateNeg(x);
  }
  auto* c = static_cast<ast::CallExpr*>(&e);  // exp/log/tanh: node_elem is always "float"
  Value* x = convert(buildFusedElem(*c->args[0], index, leaves), elemOf(c->args[0]->expr_type.name), "float");
  auto fn = module_->getOrInsertFunction(c->callee + "f",
                                        llvm::FunctionType::get(builder_.getFloatTy(), {builder_.getFloatTy()}, false));
  return builder_.CreateCall(fn, {x});
}

Value* CodeGenerator::tryFuse(ast::ASTNode& n) {
  if (!fuse_ || !isTensor(n.expr_type.name) || !isFusableOp(n)) return nullptr;
  std::unordered_map<const ast::ASTNode*, Value*> leaves;
  prepareFusionLeaves(n, leaves);
  const Shape rs = parseTensor(n.expr_type.name);
  auto* out = tensorAlloc(n.expr_type.name, "t");
  forRange(rs.size(), [&](Value* i) { builder_.CreateStore(buildFusedElem(n, i, leaves), elemPtr(out, rs.elem, i)); });
  return out;
}

// Classic 6-nested-loop blocked GEMM: block the reduction dimension outermost so each (i, j)
// output cell accumulates its partial sum across k-blocks (read-modify-write on `out`), and block
// i and j so the a/b tiles touched by an (ii, jj, kk) block stay cache-resident. Requires every
// dimension to be a multiple of `block` (checked by the caller); this is the whole optimization,
// so there is no partial-tile remainder path to get subtly wrong.
Value* CodeGenerator::emitTiledMatmul(Value* a, const std::string& a_elem, Value* b, const std::string& b_elem,
                                      long long m, long long k, long long n, const std::string& result_type,
                                      long long block) {
  const std::string result_elem = elemOf(result_type);
  auto* out = tensorAlloc(result_type, "t");
  builder_.CreateMemSet(out, builder_.getInt8(0), static_cast<uint64_t>(m * n) * 4, llvm::MaybeAlign(4));
  forRange(k, [&](Value* kk) {
    forRange(m, [&](Value* ii) {
      forRange(n, [&](Value* jj) {
        forRange(block, [&](Value* di) {
          Value* i = builder_.CreateAdd(ii, di);
          forRange(block, [&](Value* dj) {
            Value* j = builder_.CreateAdd(jj, dj);
            auto* acc = entryAlloca(llvmType(result_elem), "acc");
            builder_.CreateStore(loadElem(out, result_elem, flatIndex(i, n, j)), acc);
            forRange(block, [&](Value* dk) {
              Value* p = builder_.CreateAdd(kk, dk);
              Value* x = convert(loadElem(a, a_elem, flatIndex(i, k, p)), a_elem, result_elem);
              Value* y = convert(loadElem(b, b_elem, flatIndex(p, n, j)), b_elem, result_elem);
              Value* cur = builder_.CreateLoad(llvmType(result_elem), acc);
              builder_.CreateStore(binaryOp("+", cur, result_elem, binaryOp("*", x, result_elem, y, result_elem), result_elem), acc);
            });
            builder_.CreateStore(builder_.CreateLoad(llvmType(result_elem), acc), elemPtr(out, result_elem, flatIndex(i, n, j)));
          });
        });
      }, block);
    }, block);
  }, block);
  return out;
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
  // Tensors cross calls by pointer: params point at a caller-made copy, and a tensor result is
  // written through a hidden leading out-pointer.
  const bool returns_tensor = isTensor(n.return_type.name);
  std::vector<llvm::Type*> params;
  if (returns_tensor) params.push_back(builder_.getPtrTy());
  for (const auto& p : n.params) {
    params.push_back(isTensor(p.type.name) ? builder_.getPtrTy() : llvmType(p.type.name));
  }
  auto* fty = llvm::FunctionType::get(returns_tensor ? builder_.getVoidTy() : llvmType(n.return_type.name),
                                      params, false);
  fn_ = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, n.name, *module_);
  funcs_[n.name] = &n;
  current_return_ = n.return_type.name;

  builder_.SetInsertPoint(BasicBlock::Create(ctx_, "entry", fn_));
  scopes_.emplace_back();
  auto arg = fn_->arg_begin();
  sret_ = nullptr;
  if (returns_tensor) {
    sret_ = &*arg++;
    sret_->setName("retval");
  }
  for (size_t i = 0; arg != fn_->arg_end(); ++arg, ++i) {
    const auto& p = n.params[i];
    arg->setName(p.name);
    if (isTensor(p.type.name)) {
      scopes_.back()[p.name] = &*arg;
      continue;
    }
    auto* slot = entryAlloca(arg->getType(), p.name);
    builder_.CreateStore(&*arg, slot);
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
  sret_ = nullptr;
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
  auto* slot = isTensor(n.type.name) ? tensorAlloc(n.type.name, n.name) : entryAlloca(type, n.name);
  if (!n.init && isTensor(n.type.name)) {  // tensors start zeroed
    builder_.CreateMemSet(slot, builder_.getInt8(0), static_cast<uint64_t>(parseTensor(n.type.name).size()) * 4,
                          llvm::MaybeAlign(4));
  }
  if (n.init) {
    store(slot, n.type.name, eval(*n.init), n.init->expr_type.name);
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
  if (sret_ != nullptr) {
    assignTensor(sret_, current_return_, eval(*n.value), n.value->expr_type.name);
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
    if (isTensor(lt)) {
      if (op == "=") {
        store(addr, lt, r, rt);
      } else {  // A += B, A -= 1, ... in place
        const Shape ls = parseTensor(lt);
        forRange(ls.size(), [&](Value* i) {
          Value* cur = loadElem(addr, ls.elem, i);
          builder_.CreateStore(binaryOp(op.substr(0, 1), cur, ls.elem, tensorOperand(r, rt, i, ls.elem), ls.elem),
                               elemPtr(addr, ls.elem, i));
        });
      }
      value_ = addr;
      return;
    }
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

  if (isTensor(n.expr_type.name)) {
    if (Value* fused = tryFuse(n)) {
      value_ = fused;
      return;
    }
  }
  Value* l = eval(*n.lhs);
  Value* r = eval(*n.rhs);
  if (isTensor(lt) || isTensor(rt)) {
    value_ = tensorBinary(op, l, lt, r, rt, n.expr_type.name);
    return;
  }
  value_ = binaryOp(op, l, lt, r, rt);
}

void CodeGenerator::visit(ast::UnaryExpr& n) {
  if (n.op == "&") {
    value_ = lvalue(*n.operand);
    return;
  }
  Value* v = eval(*n.operand);
  const std::string& t = n.operand->expr_type.name;
  if (isTensor(t)) {  // only "-" passes sema
    if (Value* fused = tryFuse(n)) {
      value_ = fused;
      return;
    }
    const Shape s = parseTensor(t);
    auto* out = tensorAlloc(t, "t");
    forRange(s.size(), [&](Value* i) {
      Value* x = loadElem(v, s.elem, i);
      builder_.CreateStore(s.elem == "float" ? builder_.CreateFNeg(x) : builder_.CreateNeg(x), elemPtr(out, s.elem, i));
    });
    value_ = out;
  } else if (n.op == "*") {
    value_ = builder_.CreateLoad(llvmType(n.expr_type.name), v);
  } else if (n.op == "!") {
    value_ = builder_.CreateZExt(builder_.CreateNot(toBool(v, t)), builder_.getInt32Ty());
  } else {  // "-"
    value_ = t == "float" ? builder_.CreateFNeg(v) : builder_.CreateNeg(convert(v, t, "int"));
  }
}

void CodeGenerator::visit(ast::CallExpr& n) {
  if (!funcs_.count(n.callee)) {  // user functions shadow builtins
    value_ = emitBuiltin(n);
    return;
  }
  const auto* decl = funcs_.at(n.callee);
  std::vector<Value*> args;
  Value* result_slot = nullptr;
  if (isTensor(decl->return_type.name)) {
    result_slot = tensorAlloc(decl->return_type.name, "ret");
    args.push_back(result_slot);
  }
  for (size_t i = 0; i < n.args.size(); ++i) {
    const std::string& param_type = decl->params[i].type.name;
    const std::string& arg_type = n.args[i]->expr_type.name;
    Value* v = eval(*n.args[i]);
    if (isTensor(param_type)) {  // pass a private copy: tensors have value semantics
      auto* copy = tensorAlloc(param_type, "arg");
      assignTensor(copy, param_type, v, arg_type);
      args.push_back(copy);
    } else {
      args.push_back(convert(v, arg_type, param_type));
    }
  }
  Value* call = builder_.CreateCall(module_->getFunction(n.callee), args);
  value_ = result_slot != nullptr ? result_slot : call;
}

void CodeGenerator::visit(ast::MemberExpr& n) {
  Value* addr = lvalue(n);
  value_ = isTensor(n.expr_type.name) ? addr : builder_.CreateLoad(llvmType(n.expr_type.name), addr);
}

void CodeGenerator::visit(ast::ArraySubscript& n) {
  Value* addr = lvalue(n);
  value_ = isTensor(n.expr_type.name) ? addr : builder_.CreateLoad(llvmType(n.expr_type.name), addr);
}

void CodeGenerator::visit(ast::VarRef& n) {
  Value* addr = lvalue(n);
  value_ = isTensor(n.expr_type.name) ? addr : builder_.CreateLoad(llvmType(n.expr_type.name), addr);
}

void CodeGenerator::visit(ast::TensorLiteral& n) {
  const std::string& type = n.expr_type.name;
  const Shape s = parseTensor(type);
  auto* out = tensorAlloc(type, "lit");
  long long unit = 1;  // elements contributed by each entry of the literal
  for (size_t d = 1; d < s.dims.size(); ++d) unit *= s.dims[d];
  for (size_t k = 0; k < n.elements.size(); ++k) {
    Value* v = eval(*n.elements[k]);
    const std::string& et = n.elements[k]->expr_type.name;
    Value* pos = builder_.getInt32(static_cast<uint32_t>(k * unit));
    if (isTensor(et)) {
      store(elemPtr(out, s.elem, pos), tensorName(s.elem, parseTensor(et).dims), v, et);
    } else {
      builder_.CreateStore(convert(v, et, s.elem), elemPtr(out, s.elem, pos));
    }
  }
  value_ = out;
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
