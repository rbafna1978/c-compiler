#pragma once

#include <string>
#include <vector>

namespace compiler::lexer {

/** Represents a lexical token. */
struct Token {
  enum class Kind {
    KwInt,
    KwFloat,
    KwChar,
    KwVoid,
    KwStruct,
    KwIf,
    KwElse,
    KwWhile,
    KwFor,
    KwReturn,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    EqEq,
    NotEq,
    Lt,
    Gt,
    Le,
    Ge,
    AndAnd,
    OrOr,
    Not,
    Assign,
    PlusAssign,
    MinusAssign,
    StarAssign,
    SlashAssign,
    Arrow,
    Amp,
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Semicolon,
    Comma,
    Dot,
    IntLiteral,
    FloatLiteral,
    CharLiteral,
    StringLiteral,
    Identifier,
    EndOfFile,
    Invalid,
  };

  Kind kind = Kind::Invalid;
  std::string lexeme;
  int line = 1;
  union {
    long long int_val;
    double float_val;
  } value{};
};

/** Represents a lexing diagnostic. */
struct LexError {
  std::string filename;
  int line = 1;
  std::string message;
};

struct LexContext {
  std::vector<Token>* tokens = nullptr;
  std::vector<LexError>* errors = nullptr;
  std::string filename;
  std::string string_buffer;
  int string_start_line = 1;
  int comment_start_line = 1;
};

/** Flex-backed lexer entry point. */
class Lexer {
 public:
  /** Tokenizes the given input source. */
  std::vector<Token> tokenize(const std::string& input, const std::string& filename = "<input>");

  /** Returns diagnostics produced by the last tokenize call. */
  const std::vector<LexError>& errors() const;

 private:
  std::vector<LexError> errors_;
};

}  // namespace compiler::lexer
