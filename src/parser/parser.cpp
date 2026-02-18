#include "parser/parser.h"

#include "lexer/lexer.h"
#include "parser.hpp"

namespace compiler::parser {

void ParseDriver::report(const std::string& message, int line) {
  if (errors == nullptr) {
    return;
  }
  ParseError err;
  err.filename = filename;
  err.line = (line > 0) ? line : last_line;
  err.message = message;
  errors->push_back(std::move(err));
}

const lexer::Token& ParseDriver::peek() const {
  static lexer::Token eof;
  eof.kind = lexer::Token::Kind::EndOfFile;
  if (index >= tokens.size()) {
    return eof;
  }
  return tokens[index];
}

lexer::Token ParseDriver::consume() {
  const lexer::Token& token = peek();
  if (index < tokens.size()) {
    ++index;
  }
  last_line = token.line;
  return token;
}

}  // namespace compiler::parser

namespace yy {

parser::symbol_type yylex(compiler::parser::ParseDriver& driver) {
  const auto token = driver.consume();
  using Kind = compiler::lexer::Token::Kind;

  switch (token.kind) {
    case Kind::KwInt:
      return parser::make_KW_INT();
    case Kind::KwFloat:
      return parser::make_KW_FLOAT();
    case Kind::KwChar:
      return parser::make_KW_CHAR();
    case Kind::KwVoid:
      return parser::make_KW_VOID();
    case Kind::KwStruct:
      return parser::make_KW_STRUCT();
    case Kind::KwIf:
      return parser::make_KW_IF();
    case Kind::KwElse:
      return parser::make_KW_ELSE();
    case Kind::KwWhile:
      return parser::make_KW_WHILE();
    case Kind::KwFor:
      return parser::make_KW_FOR();
    case Kind::KwReturn:
      return parser::make_KW_RETURN();

    case Kind::Plus:
      return parser::make_PLUS();
    case Kind::Minus:
      return parser::make_MINUS();
    case Kind::Star:
      return parser::make_STAR();
    case Kind::Slash:
      return parser::make_SLASH();
    case Kind::Percent:
      return parser::make_PERCENT();
    case Kind::EqEq:
      return parser::make_EQEQ();
    case Kind::NotEq:
      return parser::make_NEQ();
    case Kind::Lt:
      return parser::make_LT();
    case Kind::Gt:
      return parser::make_GT();
    case Kind::Le:
      return parser::make_LE();
    case Kind::Ge:
      return parser::make_GE();
    case Kind::AndAnd:
      return parser::make_ANDAND();
    case Kind::OrOr:
      return parser::make_OROR();
    case Kind::Not:
      return parser::make_NOT();
    case Kind::Assign:
      return parser::make_ASSIGN();
    case Kind::PlusAssign:
      return parser::make_PLUSEQ();
    case Kind::MinusAssign:
      return parser::make_MINUSEQ();
    case Kind::StarAssign:
      return parser::make_STAREQ();
    case Kind::SlashAssign:
      return parser::make_SLASHEQ();
    case Kind::Arrow:
      return parser::make_ARROW();
    case Kind::Amp:
      return parser::make_AMP();

    case Kind::LParen:
      return parser::make_LPAREN();
    case Kind::RParen:
      return parser::make_RPAREN();
    case Kind::LBrace:
      return parser::make_LBRACE();
    case Kind::RBrace:
      return parser::make_RBRACE();
    case Kind::LBracket:
      return parser::make_LBRACKET();
    case Kind::RBracket:
      return parser::make_RBRACKET();
    case Kind::Semicolon:
      return parser::make_SEMICOLON();
    case Kind::Comma:
      return parser::make_COMMA();
    case Kind::Dot:
      return parser::make_DOT();

    case Kind::IntLiteral:
      return parser::make_INT_LITERAL(token.value.int_val);
    case Kind::FloatLiteral:
      return parser::make_FLOAT_LITERAL(token.value.float_val);
    case Kind::CharLiteral:
      return parser::make_CHAR_LITERAL(token.value.int_val);
    case Kind::StringLiteral:
      return parser::make_STRING_LITERAL(token.lexeme);
    case Kind::Identifier:
      return parser::make_IDENTIFIER(token.lexeme);

    case Kind::Invalid:
      driver.report("invalid token: " + token.lexeme, token.line);
      return parser::make_INVALID();

    case Kind::EndOfFile:
      return parser::make_YYEOF();
  }

  driver.report("unhandled token", token.line);
  return parser::make_INVALID();
}

}  // namespace yy

namespace compiler::parser {

std::unique_ptr<ast::TranslationUnit> Parser::parse(const std::string& input,
                                                    const std::string& filename) {
  errors_.clear();

  lexer::Lexer lexer;
  ParseDriver driver;
  driver.tokens = lexer.tokenize(input, filename);
  driver.filename = filename;
  driver.errors = &errors_;

  for (const auto& lex_error : lexer.errors()) {
    ParseError err;
    err.filename = lex_error.filename;
    err.line = lex_error.line;
    err.message = lex_error.message;
    errors_.push_back(std::move(err));
  }

  yy::parser parser(driver);
  const int parse_status = parser.parse();

  if (parse_status != 0 || !errors_.empty()) {
    return nullptr;
  }
  return std::move(driver.result);
}

const std::vector<ParseError>& Parser::errors() const { return errors_; }

}  // namespace compiler::parser
