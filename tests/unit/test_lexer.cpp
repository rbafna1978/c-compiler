#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "lexer/lexer.h"

namespace {

using compiler::lexer::Lexer;
using compiler::lexer::Token;

std::vector<Token::Kind> kinds(const std::vector<Token>& tokens) {
  std::vector<Token::Kind> out;
  for (const auto& token : tokens) {
    out.push_back(token.kind);
  }
  return out;
}

}  // namespace

TEST(LexerTest, TokenizesAllKeywords) {
  Lexer lexer;
  auto tokens = lexer.tokenize("int float char void struct if else while for return");
  EXPECT_TRUE(lexer.errors().empty());

  EXPECT_EQ(kinds(tokens), (std::vector<Token::Kind>{
                              Token::Kind::KwInt,
                              Token::Kind::KwFloat,
                              Token::Kind::KwChar,
                              Token::Kind::KwVoid,
                              Token::Kind::KwStruct,
                              Token::Kind::KwIf,
                              Token::Kind::KwElse,
                              Token::Kind::KwWhile,
                              Token::Kind::KwFor,
                              Token::Kind::KwReturn,
                              Token::Kind::EndOfFile,
                            }));
}

TEST(LexerTest, TokenizesAllOperatorsAndDelimiters) {
  Lexer lexer;
  auto tokens = lexer.tokenize(
      "+ - * / % == != < > <= >= && || ! = += -= *= /= -> & ( ) { } [ ] ; , .");
  EXPECT_TRUE(lexer.errors().empty());

  EXPECT_EQ(kinds(tokens), (std::vector<Token::Kind>{
                              Token::Kind::Plus,
                              Token::Kind::Minus,
                              Token::Kind::Star,
                              Token::Kind::Slash,
                              Token::Kind::Percent,
                              Token::Kind::EqEq,
                              Token::Kind::NotEq,
                              Token::Kind::Lt,
                              Token::Kind::Gt,
                              Token::Kind::Le,
                              Token::Kind::Ge,
                              Token::Kind::AndAnd,
                              Token::Kind::OrOr,
                              Token::Kind::Not,
                              Token::Kind::Assign,
                              Token::Kind::PlusAssign,
                              Token::Kind::MinusAssign,
                              Token::Kind::StarAssign,
                              Token::Kind::SlashAssign,
                              Token::Kind::Arrow,
                              Token::Kind::Amp,
                              Token::Kind::LParen,
                              Token::Kind::RParen,
                              Token::Kind::LBrace,
                              Token::Kind::RBrace,
                              Token::Kind::LBracket,
                              Token::Kind::RBracket,
                              Token::Kind::Semicolon,
                              Token::Kind::Comma,
                              Token::Kind::Dot,
                              Token::Kind::EndOfFile,
                            }));
}

TEST(LexerTest, TokenizesNumericCharAndStringLiterals) {
  Lexer lexer;
  auto tokens = lexer.tokenize("42 3.5 'a' \"hello\"");
  ASSERT_TRUE(lexer.errors().empty());

  ASSERT_GE(tokens.size(), 5U);
  EXPECT_EQ(tokens[0].kind, Token::Kind::IntLiteral);
  EXPECT_EQ(tokens[0].value.int_val, 42);

  EXPECT_EQ(tokens[1].kind, Token::Kind::FloatLiteral);
  EXPECT_DOUBLE_EQ(tokens[1].value.float_val, 3.5);

  EXPECT_EQ(tokens[2].kind, Token::Kind::CharLiteral);
  EXPECT_EQ(tokens[2].value.int_val, static_cast<long long>('a'));

  EXPECT_EQ(tokens[3].kind, Token::Kind::StringLiteral);
  EXPECT_EQ(tokens[3].lexeme, "hello");
}

TEST(LexerTest, SkipsCommentsAndTracksLines) {
  Lexer lexer;
  auto tokens = lexer.tokenize("int x; // one\n/* two */\nreturn x;");
  ASSERT_TRUE(lexer.errors().empty());

  ASSERT_GE(tokens.size(), 7U);
  EXPECT_EQ(tokens[0].kind, Token::Kind::KwInt);
  EXPECT_EQ(tokens[0].line, 1);
  EXPECT_EQ(tokens[3].kind, Token::Kind::KwReturn);
  EXPECT_EQ(tokens[3].line, 3);
}

TEST(LexerTest, ReportsUnterminatedString) {
  Lexer lexer;
  auto tokens = lexer.tokenize("\"unterminated");

  EXPECT_FALSE(lexer.errors().empty());
  EXPECT_EQ(lexer.errors()[0].line, 1);
  EXPECT_NE(lexer.errors()[0].message.find("unterminated string literal"), std::string::npos);
  EXPECT_EQ(tokens.back().kind, Token::Kind::EndOfFile);
}

TEST(LexerTest, ReportsUnterminatedComment) {
  Lexer lexer;
  auto tokens = lexer.tokenize("/* unterminated");

  EXPECT_FALSE(lexer.errors().empty());
  EXPECT_EQ(lexer.errors()[0].line, 1);
  EXPECT_NE(lexer.errors()[0].message.find("unterminated block comment"), std::string::npos);
  EXPECT_EQ(tokens.back().kind, Token::Kind::EndOfFile);
}

TEST(LexerTest, ReportsInvalidToken) {
  Lexer lexer;
  auto tokens = lexer.tokenize("int @");

  ASSERT_FALSE(lexer.errors().empty());
  EXPECT_NE(lexer.errors()[0].message.find("invalid token"), std::string::npos);
  EXPECT_EQ(tokens[1].kind, Token::Kind::Invalid);
}
