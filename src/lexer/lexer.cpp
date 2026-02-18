#include "lexer/lexer.h"

#include <cstddef>

struct yy_buffer_state;
typedef yy_buffer_state* YY_BUFFER_STATE;

int yylex(void);
YY_BUFFER_STATE yy_scan_bytes(const char* bytes, std::size_t len);
void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern int yylineno;
void lexer_set_context(compiler::lexer::LexContext* ctx);

namespace compiler::lexer {

std::vector<Token> Lexer::tokenize(const std::string& input, const std::string& filename) {
  errors_.clear();
  std::vector<Token> tokens;
  LexContext context;
  context.tokens = &tokens;
  context.errors = &errors_;
  context.filename = filename;

  yylineno = 1;
  lexer_set_context(&context);
  YY_BUFFER_STATE buffer = yy_scan_bytes(input.data(), input.size());

  while (yylex() != 0) {
  }

  yy_delete_buffer(buffer);
  lexer_set_context(nullptr);

  Token eof;
  eof.kind = Token::Kind::EndOfFile;
  eof.lexeme = "";
  eof.line = yylineno;
  tokens.push_back(eof);

  return tokens;
}

const std::vector<LexError>& Lexer::errors() const { return errors_; }

}  // namespace compiler::lexer
