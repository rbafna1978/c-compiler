#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast/ast.h"

namespace compiler::sema {

/** Scoped symbol table for semantic analysis. */
class SymbolTable {
 public:
  /** Enters a new lexical scope. */
  void enterScope();

  /** Exits the current lexical scope. */
  void exitScope();

  /** Declares a symbol in the current scope. */
  bool declare(const std::string& name, const ast::TypeInfo& type);

  /** Looks up a symbol through parent scopes. */
  std::optional<ast::TypeInfo> lookup(const std::string& name) const;

 private:
  std::vector<std::unordered_map<std::string, ast::TypeInfo>> scopes_{{}};
};

}  // namespace compiler::sema
