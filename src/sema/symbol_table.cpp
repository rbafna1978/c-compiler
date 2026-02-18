#include "sema/symbol_table.h"

namespace compiler::sema {

void SymbolTable::enterScope() { scopes_.emplace_back(); }

void SymbolTable::exitScope() {
  if (scopes_.size() > 1) {
    scopes_.pop_back();
  }
}

bool SymbolTable::declare(const std::string& name, const ast::TypeInfo& type) {
  auto& scope = scopes_.back();
  return scope.emplace(name, type).second;
}

std::optional<ast::TypeInfo> SymbolTable::lookup(const std::string& name) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }
  return std::nullopt;
}

}  // namespace compiler::sema
