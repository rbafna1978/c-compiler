%option noyywrap nodefault yylineno noinput nounput

%{
#include <cstdlib>
#include <string>

#include "lexer/lexer.h"

using compiler::lexer::LexContext;
using compiler::lexer::LexError;
using compiler::lexer::Token;

static LexContext* g_ctx = nullptr;

void lexer_set_context(LexContext* ctx) { g_ctx = ctx; }

static void push_token(Token::Kind kind, const char* text, int line) {
  if (g_ctx == nullptr || g_ctx->tokens == nullptr) {
    return;
  }
  Token token;
  token.kind = kind;
  token.lexeme = text;
  token.line = line;
  g_ctx->tokens->push_back(token);
}

static void push_int_token(const char* text, int line) {
  if (g_ctx == nullptr || g_ctx->tokens == nullptr) {
    return;
  }
  Token token;
  token.kind = Token::Kind::IntLiteral;
  token.lexeme = text;
  token.line = line;
  token.value.int_val = std::strtoll(text, nullptr, 10);
  g_ctx->tokens->push_back(token);
}

static void push_float_token(const char* text, int line) {
  if (g_ctx == nullptr || g_ctx->tokens == nullptr) {
    return;
  }
  Token token;
  token.kind = Token::Kind::FloatLiteral;
  token.lexeme = text;
  token.line = line;
  token.value.float_val = std::strtod(text, nullptr);
  g_ctx->tokens->push_back(token);
}

static char unescape_char(const char* text) {
  if (text[1] != '\\') {
    return text[1];
  }
  switch (text[2]) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"': return '"';
    case '0': return '\0';
    default: return text[2];
  }
}

static void push_char_token(const char* text, int line) {
  if (g_ctx == nullptr || g_ctx->tokens == nullptr) {
    return;
  }
  Token token;
  token.kind = Token::Kind::CharLiteral;
  token.lexeme = text;
  token.line = line;
  token.value.int_val = static_cast<long long>(unescape_char(text));
  g_ctx->tokens->push_back(token);
}

static void push_string_token(const std::string& text, int line) {
  if (g_ctx == nullptr || g_ctx->tokens == nullptr) {
    return;
  }
  Token token;
  token.kind = Token::Kind::StringLiteral;
  token.lexeme = text;
  token.line = line;
  g_ctx->tokens->push_back(token);
}

static void push_error(int line, const std::string& message) {
  if (g_ctx == nullptr || g_ctx->errors == nullptr) {
    return;
  }
  LexError err;
  err.filename = g_ctx->filename;
  err.line = line;
  err.message = message;
  g_ctx->errors->push_back(err);
}
%}

%x COMMENT
%x STRING

WS              [ \t\r\n]+
ID              [a-zA-Z_][a-zA-Z0-9_]*
INT             [0-9]+
FLOAT           (([0-9]+\.[0-9]*|\.[0-9]+)([eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+)
CHARLIT         \'([^\\\n]|\\[ntr\\\'\"0])\'

%%
"int"           { push_token(Token::Kind::KwInt, yytext, yylineno); return 1; }
"float"         { push_token(Token::Kind::KwFloat, yytext, yylineno); return 1; }
"char"          { push_token(Token::Kind::KwChar, yytext, yylineno); return 1; }
"void"          { push_token(Token::Kind::KwVoid, yytext, yylineno); return 1; }
"struct"        { push_token(Token::Kind::KwStruct, yytext, yylineno); return 1; }
"if"            { push_token(Token::Kind::KwIf, yytext, yylineno); return 1; }
"else"          { push_token(Token::Kind::KwElse, yytext, yylineno); return 1; }
"while"         { push_token(Token::Kind::KwWhile, yytext, yylineno); return 1; }
"for"           { push_token(Token::Kind::KwFor, yytext, yylineno); return 1; }
"return"        { push_token(Token::Kind::KwReturn, yytext, yylineno); return 1; }

"=="            { push_token(Token::Kind::EqEq, yytext, yylineno); return 1; }
"!="            { push_token(Token::Kind::NotEq, yytext, yylineno); return 1; }
"<="            { push_token(Token::Kind::Le, yytext, yylineno); return 1; }
">="            { push_token(Token::Kind::Ge, yytext, yylineno); return 1; }
"&&"            { push_token(Token::Kind::AndAnd, yytext, yylineno); return 1; }
"||"            { push_token(Token::Kind::OrOr, yytext, yylineno); return 1; }
"+="            { push_token(Token::Kind::PlusAssign, yytext, yylineno); return 1; }
"-="            { push_token(Token::Kind::MinusAssign, yytext, yylineno); return 1; }
"*="            { push_token(Token::Kind::StarAssign, yytext, yylineno); return 1; }
"/="            { push_token(Token::Kind::SlashAssign, yytext, yylineno); return 1; }
"->"            { push_token(Token::Kind::Arrow, yytext, yylineno); return 1; }

"+"             { push_token(Token::Kind::Plus, yytext, yylineno); return 1; }
"-"             { push_token(Token::Kind::Minus, yytext, yylineno); return 1; }
"*"             { push_token(Token::Kind::Star, yytext, yylineno); return 1; }
"/"             { push_token(Token::Kind::Slash, yytext, yylineno); return 1; }
"%"             { push_token(Token::Kind::Percent, yytext, yylineno); return 1; }
"<"             { push_token(Token::Kind::Lt, yytext, yylineno); return 1; }
">"             { push_token(Token::Kind::Gt, yytext, yylineno); return 1; }
"!"             { push_token(Token::Kind::Not, yytext, yylineno); return 1; }
"="             { push_token(Token::Kind::Assign, yytext, yylineno); return 1; }
"&"             { push_token(Token::Kind::Amp, yytext, yylineno); return 1; }
"("             { push_token(Token::Kind::LParen, yytext, yylineno); return 1; }
")"             { push_token(Token::Kind::RParen, yytext, yylineno); return 1; }
"{"             { push_token(Token::Kind::LBrace, yytext, yylineno); return 1; }
"}"             { push_token(Token::Kind::RBrace, yytext, yylineno); return 1; }
"["             { push_token(Token::Kind::LBracket, yytext, yylineno); return 1; }
"]"             { push_token(Token::Kind::RBracket, yytext, yylineno); return 1; }
";"             { push_token(Token::Kind::Semicolon, yytext, yylineno); return 1; }
","             { push_token(Token::Kind::Comma, yytext, yylineno); return 1; }
"."             { push_token(Token::Kind::Dot, yytext, yylineno); return 1; }

{FLOAT}         { push_float_token(yytext, yylineno); return 1; }
{INT}           { push_int_token(yytext, yylineno); return 1; }
{CHARLIT}       { push_char_token(yytext, yylineno); return 1; }
"\""            {
                  if (g_ctx != nullptr) {
                    g_ctx->string_buffer.clear();
                    g_ctx->string_start_line = yylineno;
                  }
                  BEGIN(STRING);
                }
{ID}            { push_token(Token::Kind::Identifier, yytext, yylineno); return 1; }

"//"[^\n]*      { }
"/*"            {
                  if (g_ctx != nullptr) {
                    g_ctx->comment_start_line = yylineno;
                  }
                  BEGIN(COMMENT);
                }

{WS}            { }

<COMMENT>"*/"   { BEGIN(INITIAL); }
<COMMENT>.|\n   { }
<COMMENT><<EOF>> {
                  push_error(g_ctx != nullptr ? g_ctx->comment_start_line : yylineno,
                             "unterminated block comment");
                  BEGIN(INITIAL);
                  return 0;
                }

<STRING>\"      {
                  push_string_token(g_ctx != nullptr ? g_ctx->string_buffer : std::string(),
                                    g_ctx != nullptr ? g_ctx->string_start_line : yylineno);
                  BEGIN(INITIAL);
                  return 1;
                }
<STRING>\\.      {
                  if (g_ctx != nullptr) {
                    g_ctx->string_buffer.append(yytext);
                  }
                }
<STRING>[^\\\"\n]+ {
                  if (g_ctx != nullptr) {
                    g_ctx->string_buffer.append(yytext);
                  }
                }
<STRING>\n       {
                  push_error(g_ctx != nullptr ? g_ctx->string_start_line : yylineno,
                             "unterminated string literal");
                  BEGIN(INITIAL);
                }
<STRING><<EOF>>  {
                  push_error(g_ctx != nullptr ? g_ctx->string_start_line : yylineno,
                             "unterminated string literal");
                  BEGIN(INITIAL);
                  return 0;
                }

.               {
                  push_token(Token::Kind::Invalid, yytext, yylineno);
                  push_error(yylineno, std::string("invalid token: ") + yytext);
                  return 1;
                }

<<EOF>>         { return 0; }
%%
