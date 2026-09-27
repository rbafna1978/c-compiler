#include "autodiff/autodiff.h"

#include <map>
#include <set>
#include <utility>

#include "ast/tensor_type.h"
#include "sema/sema.h"

namespace compiler::autodiff {

using ast::isTensor;
using ast::parseTensor;
using ast::tensorName;
using Node = std::unique_ptr<ast::ASTNode>;

namespace {

bool isDiff(const std::string& t) { return t == "float" || (isTensor(t) && parseTensor(t).elem == "float"); }

// Result "kind" of combining an adjoint of type `a` with an operand of type `b` (only tensor-ness matters).
std::string join(const std::string& a, const std::string& b) {
  return isTensor(a) ? a : isTensor(b) ? b : "float";
}

template <class T>
std::unique_ptr<T> make(int line, const ast::ASTNode* like = nullptr) {
  auto n = std::make_unique<T>();
  n->line = line;
  if (like != nullptr) n->expr_type = like->expr_type;
  return n;
}

Node clone(const ast::ASTNode& n) {
  const int line = n.line;
  if (const auto* v = dynamic_cast<const ast::VarRef*>(&n)) {
    auto c = make<ast::VarRef>(line, &n);
    c->name = v->name;
    return c;
  }
  if (const auto* v = dynamic_cast<const ast::IntLiteral*>(&n)) {
    auto c = make<ast::IntLiteral>(line, &n);
    c->value = v->value;
    return c;
  }
  if (const auto* v = dynamic_cast<const ast::FloatLiteral*>(&n)) {
    auto c = make<ast::FloatLiteral>(line, &n);
    c->value = v->value;
    return c;
  }
  if (const auto* v = dynamic_cast<const ast::CharLiteral*>(&n)) {
    auto c = make<ast::CharLiteral>(line, &n);
    c->value = v->value;
    return c;
  }
  if (const auto* v = dynamic_cast<const ast::StringLiteral*>(&n)) {
    auto c = make<ast::StringLiteral>(line, &n);
    c->value = v->value;
    return c;
  }
  if (const auto* v = dynamic_cast<const ast::BinaryExpr*>(&n)) {
    auto c = make<ast::BinaryExpr>(line, &n);
    c->op = v->op;
    c->lhs = clone(*v->lhs);
    c->rhs = clone(*v->rhs);
    return c;
  }
  if (const auto* v = dynamic_cast<const ast::UnaryExpr*>(&n)) {
    auto c = make<ast::UnaryExpr>(line, &n);
    c->op = v->op;
    c->operand = clone(*v->operand);
    return c;
  }
  if (const auto* v = dynamic_cast<const ast::CallExpr*>(&n)) {
    auto c = make<ast::CallExpr>(line, &n);
    c->callee = v->callee;
    for (const auto& a : v->args) c->args.push_back(clone(*a));
    return c;
  }
  if (const auto* v = dynamic_cast<const ast::MemberExpr*>(&n)) {
    auto c = make<ast::MemberExpr>(line, &n);
    c->object = clone(*v->object);
    c->member = v->member;
    c->is_arrow = v->is_arrow;
    return c;
  }
  if (const auto* v = dynamic_cast<const ast::ArraySubscript*>(&n)) {
    auto c = make<ast::ArraySubscript>(line, &n);
    c->array = clone(*v->array);
    c->index = clone(*v->index);
    return c;
  }
  if (const auto* v = dynamic_cast<const ast::TensorLiteral*>(&n)) {
    auto c = make<ast::TensorLiteral>(line, &n);
    for (const auto& e : v->elements) c->elements.push_back(clone(*e));
    return c;
  }
  return nullptr;  // statements are never cloned through here
}

// Reverse-mode differentiation of one straight-line function w.r.t. one parameter.
class Differentiator {
 public:
  Differentiator(const ast::FunctionDecl& f, size_t wrt) : f_(f), wrt_(wrt), line_(f.line) {}

  std::unique_ptr<ast::FunctionDecl> build() {
    const std::string wrt_type = f_.params[wrt_].type.name;
    if (f_.return_type.name != "float") return fail("'" + f_.name + "' must return a float to be differentiated");
    if (!isDiff(wrt_type)) return fail("parameter " + std::to_string(wrt_) + " of '" + f_.name + "' is not a float or float tensor");
    const auto& stmts = f_.body->stmts;
    const auto* ret = stmts.empty() ? nullptr : dynamic_cast<const ast::ReturnStmt*>(stmts.back().get());
    if (ret == nullptr || !ret->value) return fail("'" + f_.name + "' must end with 'return <expr>;'");

    auto out = make<ast::FunctionDecl>(line_);
    out->name = "__grad_" + f_.name + "_" + std::to_string(wrt_);
    out->return_type.name = wrt_type;
    out->params = f_.params;
    auto body = make<ast::CompoundStmt>(line_);

    // Forward pass: the original declarations, verbatim.
    std::vector<const ast::VarDecl*> locals;
    for (size_t i = 0; i + 1 < stmts.size(); ++i) {
      const auto* d = dynamic_cast<const ast::VarDecl*>(stmts[i].get());
      if (d == nullptr || !d->init) {
        return fail("'" + f_.name + "' is not straight-line: only declarations with initializers can precede the return (line " +
                    std::to_string(stmts[i]->line) + ")");
      }
      auto copy = make<ast::VarDecl>(d->line);
      copy->name = d->name;
      copy->type = d->type;
      copy->init = clone(*d->init);
      body->stmts.push_back(std::move(copy));
      locals.push_back(d);
    }

    // Adjoint variables (tensors are zero-initialized by the language).
    for (const auto& p : f_.params) declareAdjoint(*body, p.name, p.type.name);
    for (const auto* d : locals) declareAdjoint(*body, d->name, d->type.name);

    out_ = &body->stmts;
    backprop(*ret->value, num(1.0), "float");
    for (size_t i = locals.size(); i-- > 0;) {
      if (diff_.count(locals[i]->name)) backprop(*locals[i]->init, var(adj(locals[i]->name)), locals[i]->type.name);
    }
    if (!error_.empty()) return nullptr;

    auto r = make<ast::ReturnStmt>(line_);
    r->value = var(adj(f_.params[wrt_].name));
    body->stmts.push_back(std::move(r));
    out->body = std::move(body);
    return out;
  }

  const std::string& error() const { return error_; }

 private:
  static std::string adj(const std::string& name) { return "__d_" + name; }

  std::unique_ptr<ast::FunctionDecl> fail(const std::string& msg) {
    if (error_.empty()) error_ = msg;
    return nullptr;
  }

  Node var(const std::string& name) {
    auto n = make<ast::VarRef>(line_);
    n->name = name;
    return n;
  }
  Node num(double v) {
    auto n = make<ast::FloatLiteral>(line_);
    n->value = v;
    return n;
  }
  Node bin(const char* op, Node l, Node r) {
    auto n = make<ast::BinaryExpr>(line_);
    n->op = op;
    n->lhs = std::move(l);
    n->rhs = std::move(r);
    return n;
  }
  Node neg(Node x) {
    auto n = make<ast::UnaryExpr>(line_);
    n->op = "-";
    n->operand = std::move(x);
    return n;
  }
  Node call(const char* name, Node a) {
    auto n = make<ast::CallExpr>(line_);
    n->callee = name;
    n->args.push_back(std::move(a));
    return n;
  }
  Node call(const char* name, Node a, Node b) {
    auto n = call(name, std::move(a));
    static_cast<ast::CallExpr&>(*n).args.push_back(std::move(b));
    return n;
  }
  void emit(Node expr) {
    auto s = make<ast::ExprStmt>(line_);
    s->expr = std::move(expr);
    out_->push_back(std::move(s));
  }

  void declareAdjoint(ast::CompoundStmt& body, const std::string& name, const std::string& type) {
    if (!isDiff(type)) return;
    diff_.insert(name);
    auto d = make<ast::VarDecl>(line_);
    d->name = adj(name);
    d->type.name = type;
    if (type == "float") d->init = num(0.0);
    body.stmts.push_back(std::move(d));
  }

  // A tensor-shaped adjoint for shape-sensitive ops (matmul, transpose): materializes a scalar seed.
  Node materialize(Node g, const std::string& gt, const std::string& tensor_type) {
    if (isTensor(gt)) return g;
    const std::string name = "__g" + std::to_string(tmp_++);
    auto d = make<ast::VarDecl>(line_);
    d->name = name;
    d->type.name = tensorName("float", parseTensor(tensor_type).dims);
    out_->push_back(std::move(d));
    emit(bin("+=", var(name), std::move(g)));
    return var(name);
  }

  // Propagates adjoint `g` (of type `gt`) into expression `e`, emitting `__d_x += ...` statements.
  void backprop(const ast::ASTNode& e, Node g, std::string gt) {
    const std::string& et = e.expr_type.name;
    if (!error_.empty() || !isDiff(et)) return;
    if (et == "float" && isTensor(gt)) {  // scalar operand of a broadcast: reduce
      g = call("sum", std::move(g));
      gt = "float";
    }

    if (const auto* v = dynamic_cast<const ast::VarRef*>(&e)) {
      if (diff_.count(v->name)) emit(bin("+=", var(adj(v->name)), std::move(g)));
      return;
    }
    if (dynamic_cast<const ast::FloatLiteral*>(&e)) return;

    if (const auto* b = dynamic_cast<const ast::BinaryExpr*>(&e)) {
      const std::string& lt = b->lhs->expr_type.name;
      const std::string& rt = b->rhs->expr_type.name;
      if (b->op == "+") {
        backprop(*b->lhs, clone(*g), gt);
        backprop(*b->rhs, std::move(g), gt);
      } else if (b->op == "-") {
        backprop(*b->lhs, clone(*g), gt);
        backprop(*b->rhs, neg(std::move(g)), gt);
      } else if (b->op == "*") {
        backprop(*b->lhs, bin("*", clone(*g), clone(*b->rhs)), join(gt, rt));
        backprop(*b->rhs, bin("*", std::move(g), clone(*b->lhs)), join(gt, lt));
      } else if (b->op == "/") {
        backprop(*b->lhs, bin("/", clone(*g), clone(*b->rhs)), join(gt, rt));
        // d(a/b)/db = -a / b^2
        backprop(*b->rhs, bin("/", neg(bin("*", std::move(g), clone(*b->lhs))), bin("*", clone(*b->rhs), clone(*b->rhs))),
                 join(join(gt, lt), rt));
      } else {
        error_ = "cannot differentiate operator '" + b->op + "' (line " + std::to_string(e.line) + ")";
      }
      return;
    }
    if (const auto* u = dynamic_cast<const ast::UnaryExpr*>(&e); u != nullptr && u->op == "-") {
      backprop(*u->operand, neg(std::move(g)), gt);
      return;
    }
    if (const auto* c = dynamic_cast<const ast::CallExpr*>(&e)) {
      const std::string& fn = c->callee;
      const ast::ASTNode& a = *c->args[0];
      if (fn == "exp") {
        backprop(a, bin("*", std::move(g), call("exp", clone(a))), join(gt, a.expr_type.name));
      } else if (fn == "log") {
        backprop(a, bin("/", std::move(g), clone(a)), join(gt, a.expr_type.name));
      } else if (fn == "tanh") {
        Node t1 = call("tanh", clone(a)), t2 = call("tanh", clone(a));
        backprop(a, bin("*", std::move(g), bin("-", num(1.0), bin("*", std::move(t1), std::move(t2)))),
                 join(gt, a.expr_type.name));
      } else if (fn == "sum") {
        backprop(a, std::move(g), gt);
      } else if (fn == "transpose") {
        g = materialize(std::move(g), gt, et);
        backprop(a, call("transpose", std::move(g)), a.expr_type.name);
      } else if (fn == "matmul") {
        const ast::ASTNode& b = *c->args[1];
        g = materialize(std::move(g), gt, et);
        backprop(a, call("matmul", clone(*g), call("transpose", clone(b))), a.expr_type.name);
        backprop(b, call("matmul", call("transpose", clone(a)), std::move(g)), b.expr_type.name);
      } else {
        error_ = "cannot differentiate call to '" + fn + "' (line " + std::to_string(e.line) + ")";
      }
      return;
    }
    error_ = "cannot differentiate this expression (line " + std::to_string(e.line) + ")";
  }

  const ast::FunctionDecl& f_;
  size_t wrt_;
  int line_;
  int tmp_ = 0;
  std::string error_;
  std::set<std::string> diff_;  // names that carry an adjoint
  std::vector<Node>* out_ = nullptr;
};

}  // namespace

bool analyze(ast::TranslationUnit& unit, std::vector<std::string>& diagnostics) {
  sema::SemanticAnalyzer sema;
  const bool ok = sema.analyze(unit);
  diagnostics = sema.diagnostics();
  if (!ok) return false;
  if (sema.gradCalls().empty()) return true;

  // One generated function per distinct (f, parameter index), placed right after f.
  std::set<std::pair<std::string, size_t>> done;
  for (ast::CallExpr* call : sema.gradCalls()) {
    const std::string& name = static_cast<ast::VarRef&>(*call->args[0]).name;
    const auto index = static_cast<size_t>(static_cast<ast::IntLiteral&>(*call->args[1]).value);
    if (!done.insert({name, index}).second) continue;

    for (size_t i = 0; i < unit.decls.size(); ++i) {
      auto* f = dynamic_cast<ast::FunctionDecl*>(unit.decls[i].get());
      if (f == nullptr || f->name != name) continue;
      Differentiator d(*f, index);
      auto grad = d.build();
      if (!grad) {
        diagnostics.push_back("line " + std::to_string(call->line) + ": grad: " + d.error());
        return false;
      }
      unit.decls.insert(unit.decls.begin() + static_cast<std::ptrdiff_t>(i) + 1, std::move(grad));
      break;
    }
  }
  for (ast::CallExpr* call : sema.gradCalls()) {  // grad(f, i, args...) -> __grad_f_i(args...)
    const std::string& name = static_cast<ast::VarRef&>(*call->args[0]).name;
    const auto index = static_cast<ast::IntLiteral&>(*call->args[1]).value;
    call->callee = "__grad_" + name + "_" + std::to_string(index);
    call->args.erase(call->args.begin(), call->args.begin() + 2);
  }

  sema::SemanticAnalyzer lowered;  // re-check the generated code
  const bool ok2 = lowered.analyze(unit);
  diagnostics = lowered.diagnostics();
  return ok2;
}

}  // namespace compiler::autodiff
