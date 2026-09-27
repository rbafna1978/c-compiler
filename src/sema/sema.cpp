#include "sema/sema.h"

#include <sstream>

namespace compiler::sema {

namespace {

constexpr const char* kErr = "<error>";

bool isInt(const std::string& t) { return t == "int" || t == "char"; }
bool isNumeric(const std::string& t) { return isInt(t) || t == "float"; }
bool isPtr(const std::string& t) { return !t.empty() && t.back() == '*'; }
bool isScalar(const std::string& t) { return isNumeric(t) || isPtr(t); }
bool isStructName(const std::string& t) { return t.rfind("struct ", 0) == 0 && !isPtr(t); }

// Tensor types are canonical strings: "tensor<float,2,3>" (element type, then dimensions).
bool isTensor(const std::string& t) { return t.rfind("tensor<", 0) == 0 && t.back() == '>'; }

struct Shape {
  std::string elem;
  std::vector<long long> dims;
};

Shape parseTensor(const std::string& t) {
  Shape s;
  std::stringstream in(t.substr(7, t.size() - 8));
  std::string part;
  std::getline(in, s.elem, ',');
  while (std::getline(in, part, ',')) s.dims.push_back(std::stoll(part));
  return s;
}

std::string tensorName(const std::string& elem, const std::vector<long long>& dims) {
  std::string name = "tensor<" + elem;
  for (long long d : dims) name += "," + std::to_string(d);
  return name + ">";
}

std::string promote(const std::string& a, const std::string& b) {
  return (a == "float" || b == "float") ? "float" : "int";
}

// Same shape required; an int tensor converts implicitly to a float tensor.
bool compatible(const std::string& to, const std::string& from) {
  if (isTensor(to) && isTensor(from)) {
    const Shape a = parseTensor(to), b = parseTensor(from);
    return a.dims == b.dims && (a.elem == b.elem || (a.elem == "float" && b.elem == "int"));
  }
  return to == from || (isNumeric(to) && isNumeric(from));
}

bool isBuiltin(const std::string& name) { return name == "matmul" || name == "transpose" || name == "sum"; }

bool isLvalue(const ast::ASTNode& n) {
  if (dynamic_cast<const ast::VarRef*>(&n) || dynamic_cast<const ast::MemberExpr*>(&n) ||
      dynamic_cast<const ast::ArraySubscript*>(&n)) {
    return true;
  }
  const auto* u = dynamic_cast<const ast::UnaryExpr*>(&n);
  return u != nullptr && u->op == "*";
}

}  // namespace

bool SemanticAnalyzer::analyze(ast::TranslationUnit& unit) {
  diagnostics_.clear();
  funcs_.clear();
  structs_.clear();
  symbols_ = SymbolTable();
  unit.accept(*this);
  return diagnostics_.empty();
}

const std::vector<std::string>& SemanticAnalyzer::diagnostics() const {
  return diagnostics_;
}

void SemanticAnalyzer::error(int line, const std::string& message) {
  diagnostics_.push_back("line " + std::to_string(line) + ": " + message);
}

std::string SemanticAnalyzer::typeOf(ast::ASTNode& node) {
  node.accept(*this);
  return node.expr_type.name;
}

void SemanticAnalyzer::checkType(const ast::TypeInfo& type, int line) {
  if (isTensor(type.name)) {
    for (long long d : parseTensor(type.name).dims) {
      if (d <= 0) error(line, "tensor dimensions must be positive in '" + type.name + "'");
    }
  }
  if (isStructName(type.name) && !structs_.count(type.name)) {
    error(line, "use of undeclared type '" + type.name + "'");
  }
}

void SemanticAnalyzer::checkCondition(ast::ASTNode& expr, int line) {
  const std::string t = typeOf(expr);
  if (t != kErr && !isScalar(t)) {
    error(line, "condition has non-scalar type '" + t + "'");
  }
}

void SemanticAnalyzer::visit(ast::TranslationUnit& n) {
  for (auto& d : n.decls) {
    d->accept(*this);
  }
}

void SemanticAnalyzer::visit(ast::FunctionDecl& n) {
  if (!funcs_.emplace(n.name, &n).second) {
    error(n.line, "redefinition of function '" + n.name + "'");
    return;
  }
  checkType(n.return_type, n.line);
  current_return_ = n.return_type.name;
  // ponytail: params live in an outer scope of the body, so a body local may shadow a param.
  symbols_.enterScope();
  for (const auto& p : n.params) {
    checkType(p.type, n.line);
    if (p.type.name == "void") {
      error(n.line, "parameter '" + p.name + "' has void type");
    } else if (!symbols_.declare(p.name, p.type)) {
      error(n.line, "duplicate parameter '" + p.name + "'");
    }
  }
  if (n.body) {
    n.body->accept(*this);
  }
  symbols_.exitScope();
}

void SemanticAnalyzer::visit(ast::VarDecl& n) {
  checkType(n.type, n.line);
  if (n.type.name == "void") {
    error(n.line, "variable '" + n.name + "' has void type");
  }
  if (n.init) {
    const std::string t = typeOf(*n.init);
    if (t != kErr && !compatible(n.type.name, t)) {
      error(n.line, "cannot initialize '" + n.type.name + "' with '" + t + "'");
    }
  }
  if (!symbols_.declare(n.name, n.type)) {
    error(n.line, "redeclaration of '" + n.name + "'");
  }
}

void SemanticAnalyzer::visit(ast::StructDecl& n) {
  const std::string key = "struct " + n.name;
  if (structs_.count(key)) {
    error(n.line, "redefinition of '" + key + "'");
    return;
  }
  std::unordered_map<std::string, bool> seen;
  for (const auto& f : n.fields) {
    checkType(f.type, n.line);
    if (f.type.name == "void") {
      error(n.line, "field '" + f.name + "' has void type");
    }
    if (!seen.emplace(f.name, true).second) {
      error(n.line, "duplicate field '" + f.name + "' in '" + key + "'");
    }
  }
  structs_[key] = n.fields;
}

void SemanticAnalyzer::visit(ast::CompoundStmt& n) {
  symbols_.enterScope();
  for (auto& s : n.stmts) {
    s->accept(*this);
  }
  symbols_.exitScope();
}

void SemanticAnalyzer::visit(ast::IfStmt& n) {
  checkCondition(*n.cond, n.line);
  n.then_branch->accept(*this);
  if (n.else_branch) {
    n.else_branch->accept(*this);
  }
}

void SemanticAnalyzer::visit(ast::WhileStmt& n) {
  checkCondition(*n.cond, n.line);
  n.body->accept(*this);
}

void SemanticAnalyzer::visit(ast::ForStmt& n) {
  symbols_.enterScope();
  if (n.init) {
    n.init->accept(*this);
  }
  if (n.cond) {
    checkCondition(*n.cond, n.line);
  }
  if (n.incr) {
    n.incr->accept(*this);
  }
  n.body->accept(*this);
  symbols_.exitScope();
}

void SemanticAnalyzer::visit(ast::ReturnStmt& n) {
  if (!n.value) {
    if (current_return_ != "void") {
      error(n.line, "non-void function should return a value");
    }
    return;
  }
  const std::string t = typeOf(*n.value);
  if (t == kErr) {
    return;
  }
  if (current_return_ == "void") {
    error(n.line, "void function should not return a value");
  } else if (!compatible(current_return_, t)) {
    error(n.line, "cannot return '" + t + "' from function returning '" + current_return_ + "'");
  }
}

void SemanticAnalyzer::visit(ast::ExprStmt& n) {
  if (n.expr) {
    n.expr->accept(*this);
  }
}

void SemanticAnalyzer::visit(ast::BinaryExpr& n) {
  const std::string l = typeOf(*n.lhs);
  const std::string r = typeOf(*n.rhs);
  const std::string& op = n.op;
  n.expr_type.name = kErr;
  if (l == kErr || r == kErr) {
    return;
  }
  auto bad = [&] { error(n.line, "invalid operands '" + l + "' and '" + r + "' to '" + op + "'"); };

  if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=") {
    if (!isLvalue(*n.lhs)) {
      error(n.line, "expression is not assignable");
    } else if (isTensor(l) && op != "=") {
      const std::string t = tensorArith(n.line, op.substr(0, 1), l, r);
      if (!t.empty()) {
        compatible(l, t) ? void(n.expr_type.name = l) : bad();
      }
    } else if (!compatible(l, r) || (op != "=" && !isNumeric(l))) {
      bad();
    } else {
      n.expr_type.name = l;
    }
  } else if (op == "&&" || op == "||") {
    isScalar(l) && isScalar(r) ? void(n.expr_type.name = "int") : bad();
  } else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
    (isNumeric(l) && isNumeric(r)) || (isPtr(l) && l == r) ? void(n.expr_type.name = "int") : bad();
  } else if (op == "%") {
    isInt(l) && isInt(r) ? void(n.expr_type.name = "int") : bad();
  } else if (isTensor(l) || isTensor(r)) {  // + - * / (elementwise, scalars broadcast)
    const std::string t = tensorArith(n.line, op, l, r);
    if (!t.empty()) n.expr_type.name = t;
  } else if (isNumeric(l) && isNumeric(r)) {  // + - * /
    n.expr_type.name = (l == "float" || r == "float") ? "float" : "int";
  } else if (op == "+" && isPtr(l) && isInt(r)) {
    n.expr_type.name = l;
  } else if (op == "+" && isInt(l) && isPtr(r)) {
    n.expr_type.name = r;
  } else if (op == "-" && isPtr(l) && isInt(r)) {
    n.expr_type.name = l;
  } else {
    bad();
  }
}

void SemanticAnalyzer::visit(ast::UnaryExpr& n) {
  const std::string t = typeOf(*n.operand);
  n.expr_type.name = kErr;
  if (t == kErr) {
    return;
  }
  if (n.op == "!" && isScalar(t)) {
    n.expr_type.name = "int";
  } else if (n.op == "-" && isNumeric(t)) {
    n.expr_type.name = t == "float" ? "float" : "int";
  } else if (n.op == "-" && isTensor(t)) {
    n.expr_type.name = t;
  } else if (n.op == "&" && isLvalue(*n.operand)) {
    n.expr_type.name = t + "*";
  } else if (n.op == "*" && isPtr(t)) {
    n.expr_type.name = t.substr(0, t.size() - 1);
  } else {
    error(n.line, "invalid operand '" + t + "' to unary '" + n.op + "'");
  }
}

void SemanticAnalyzer::visit(ast::CallExpr& n) {
  std::vector<std::string> arg_types;
  for (auto& a : n.args) {
    arg_types.push_back(typeOf(*a));
  }
  n.expr_type.name = kErr;
  auto it = funcs_.find(n.callee);
  if (it == funcs_.end() && isBuiltin(n.callee)) {  // user definitions shadow builtins
    n.expr_type.name = builtinType(n, arg_types);
    return;
  }
  if (it == funcs_.end()) {
    error(n.line, "call to undeclared function '" + n.callee + "'");
    return;
  }
  const auto& params = it->second->params;
  if (params.size() != n.args.size()) {
    error(n.line, "function '" + n.callee + "' expects " + std::to_string(params.size()) +
                      " argument(s), got " + std::to_string(n.args.size()));
  } else {
    for (size_t i = 0; i < params.size(); ++i) {
      if (arg_types[i] != kErr && !compatible(params[i].type.name, arg_types[i])) {
        error(n.line, "argument " + std::to_string(i + 1) + " of '" + n.callee + "': cannot pass '" +
                          arg_types[i] + "' as '" + params[i].type.name + "'");
      }
    }
  }
  n.expr_type = it->second->return_type;
}

void SemanticAnalyzer::visit(ast::MemberExpr& n) {
  const std::string t = typeOf(*n.object);
  n.expr_type.name = kErr;
  if (t == kErr) {
    return;
  }
  std::string base = t;
  if (n.is_arrow) {
    if (!isPtr(t)) {
      error(n.line, "'->' applied to non-pointer type '" + t + "'");
      return;
    }
    base = t.substr(0, t.size() - 1);
  }
  auto s = structs_.find(base);
  if (s == structs_.end()) {
    error(n.line, "member access on non-struct type '" + t + "'");
    return;
  }
  for (const auto& f : s->second) {
    if (f.name == n.member) {
      n.expr_type = f.type;
      return;
    }
  }
  error(n.line, "no member '" + n.member + "' in '" + base + "'");
}

void SemanticAnalyzer::visit(ast::ArraySubscript& n) {
  const std::string a = typeOf(*n.array);
  const std::string i = typeOf(*n.index);
  n.expr_type.name = kErr;
  if (a == kErr || i == kErr) {
    return;
  }
  if (isTensor(a) && isInt(i)) {  // A[i] drops the leading dimension
    const Shape s = parseTensor(a);
    if (const auto* lit = dynamic_cast<const ast::IntLiteral*>(n.index.get());
        lit != nullptr && (lit->value < 0 || lit->value >= s.dims[0])) {
      error(n.line, "index " + std::to_string(lit->value) + " is out of bounds for dimension of size " +
                        std::to_string(s.dims[0]) + " in '" + a + "'");
    }
    n.expr_type.name = s.dims.size() == 1 ? s.elem : tensorName(s.elem, {s.dims.begin() + 1, s.dims.end()});
    return;
  }
  if (!isPtr(a) || !isInt(i)) {
    error(n.line, "invalid subscript of '" + a + "' with '" + i + "'");
    return;
  }
  n.expr_type.name = a.substr(0, a.size() - 1);
}

std::string SemanticAnalyzer::tensorArith(int line, const std::string& op, const std::string& l,
                                          const std::string& r) {
  if (isTensor(l) && isTensor(r)) {
    const Shape a = parseTensor(l), b = parseTensor(r);
    if (a.dims != b.dims) {
      error(line, "shape mismatch in '" + op + "': " + l + " vs " + r);
      return "";
    }
    return tensorName(promote(a.elem, b.elem), a.dims);
  }
  const std::string& tensor = isTensor(l) ? l : r;
  const std::string& scalar = isTensor(l) ? r : l;
  if (!isNumeric(scalar)) {
    error(line, "invalid operands '" + l + "' and '" + r + "' to '" + op + "'");
    return "";
  }
  const Shape s = parseTensor(tensor);
  return tensorName(promote(s.elem, scalar), s.dims);
}

std::string SemanticAnalyzer::builtinType(ast::CallExpr& n, const std::vector<std::string>& arg_types) {
  const std::string& name = n.callee;
  const size_t want = name == "matmul" ? 2 : 1;
  for (const auto& t : arg_types) {
    if (t == kErr) return kErr;
  }
  if (arg_types.size() != want) {
    error(n.line, "'" + name + "' expects " + std::to_string(want) + " argument(s), got " +
                      std::to_string(arg_types.size()));
    return kErr;
  }
  std::vector<Shape> shapes;
  for (size_t i = 0; i < arg_types.size(); ++i) {
    if (!isTensor(arg_types[i])) {
      error(n.line, "argument " + std::to_string(i + 1) + " of '" + name + "' must be a tensor, got '" +
                        arg_types[i] + "'");
      return kErr;
    }
    shapes.push_back(parseTensor(arg_types[i]));
  }
  if (name == "sum") return shapes[0].elem;
  for (size_t i = 0; i < shapes.size(); ++i) {
    if (shapes[i].dims.size() != 2) {
      error(n.line, "'" + name + "' requires rank-2 tensors, got '" + arg_types[i] + "'");
      return kErr;
    }
  }
  const Shape& a = shapes[0];
  if (name == "transpose") return tensorName(a.elem, {a.dims[1], a.dims[0]});
  const Shape& b = shapes[1];  // matmul
  if (a.dims[1] != b.dims[0]) {
    error(n.line, "matmul: inner dimensions differ (" + arg_types[0] + " x " + arg_types[1] + ")");
    return kErr;
  }
  return tensorName(promote(a.elem, b.elem), {a.dims[0], b.dims[1]});
}

void SemanticAnalyzer::visit(ast::TensorLiteral& n) {
  std::vector<std::string> types;
  for (auto& e : n.elements) types.push_back(typeOf(*e));
  n.expr_type.name = kErr;
  for (const auto& t : types) {
    if (t == kErr) return;
  }
  const bool nested = isTensor(types[0]);
  std::string elem = "int";
  std::vector<long long> dims = {static_cast<long long>(types.size())};
  for (const auto& t : types) {
    if (isTensor(t) != nested || (!nested && !isNumeric(t))) {
      error(n.line, "tensor literal elements must be all numbers or all tensors, got '" + t + "'");
      return;
    }
    if (nested) {
      const Shape s = parseTensor(t);
      if (dims.size() == 1) dims.insert(dims.end(), s.dims.begin(), s.dims.end());
      else if (std::vector<long long>(dims.begin() + 1, dims.end()) != s.dims) {
        error(n.line, "tensor literal rows have different shapes: '" + types[0] + "' vs '" + t + "'");
        return;
      }
      elem = promote(elem, s.elem);
    } else {
      elem = promote(elem, t);
    }
  }
  n.expr_type.name = tensorName(elem, dims);
}

void SemanticAnalyzer::visit(ast::IntLiteral& n) { n.expr_type.name = "int"; }
void SemanticAnalyzer::visit(ast::FloatLiteral& n) { n.expr_type.name = "float"; }
void SemanticAnalyzer::visit(ast::CharLiteral& n) { n.expr_type.name = "char"; }
void SemanticAnalyzer::visit(ast::StringLiteral& n) { n.expr_type.name = "char*"; }

void SemanticAnalyzer::visit(ast::VarRef& n) {
  if (auto t = symbols_.lookup(n.name)) {
    n.expr_type = *t;
  } else {
    n.expr_type.name = kErr;
    error(n.line, "use of undeclared identifier '" + n.name + "'");
  }
}

}  // namespace compiler::sema
