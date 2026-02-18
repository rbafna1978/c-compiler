#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "ast/ast.h"
#include "lexer/lexer.h"

namespace compiler::parser {

/** Represents a parser diagnostic. */
struct ParseError {
  std::string filename;
  int line = 1;
  std::string message;
};

/** Shared parse state used by Bison parser and lexer bridge. */
struct ParseDriver {
  std::vector<lexer::Token> tokens;
  std::size_t index = 0;
  std::string filename;
  int last_line = 1;
  std::vector<ParseError>* errors = nullptr;
  std::unique_ptr<ast::TranslationUnit> result;

  /** Adds a line-numbered parser diagnostic. */
  void report(const std::string& message, int line = -1);

  /** Returns the current token without consuming it. */
  const lexer::Token& peek() const;

  /** Returns and consumes the current token. */
  lexer::Token consume();
};

/** Bison-backed parser entry point. */
class Parser {
 public:
  /** Parses source text into an AST translation unit. */
  std::unique_ptr<ast::TranslationUnit> parse(const std::string& input,
                                              const std::string& filename = "<input>");

  /** Returns diagnostics produced by the last parse call. */
  const std::vector<ParseError>& errors() const;

 private:
  std::vector<ParseError> errors_;
};

}  // namespace compiler::parser
