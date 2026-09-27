%skeleton "lalr1.cc"
%require "3.8"

%define api.namespace {yy}
%define api.parser.class {parser}
%define api.value.type variant
%define api.token.constructor
%define parse.error detailed

%code requires {
#include <memory>
#include <string>
#include <vector>

#include "ast/ast.h"

namespace compiler::parser {
struct ParseDriver;
}
}

%parse-param { compiler::parser::ParseDriver& driver }
%lex-param { compiler::parser::ParseDriver& driver }

%code {
#include <memory>
#include <string>
#include <utility>

#include "parser/parser.h"

namespace yy {
parser::symbol_type yylex(compiler::parser::ParseDriver& driver);
}

namespace {

std::unique_ptr<compiler::ast::BinaryExpr> make_binary(
    const std::string& op, std::unique_ptr<compiler::ast::ASTNode> lhs,
    std::unique_ptr<compiler::ast::ASTNode> rhs, int line) {
  auto node = std::make_unique<compiler::ast::BinaryExpr>();
  node->op = op;
  node->lhs = std::move(lhs);
  node->rhs = std::move(rhs);
  node->line = line;
  return node;
}

std::unique_ptr<compiler::ast::UnaryExpr> make_unary(
    const std::string& op, std::unique_ptr<compiler::ast::ASTNode> operand, int line) {
  auto node = std::make_unique<compiler::ast::UnaryExpr>();
  node->op = op;
  node->operand = std::move(operand);
  node->line = line;
  return node;
}

}  // namespace
}

%token KW_INT KW_FLOAT KW_CHAR KW_VOID KW_STRUCT KW_IF KW_ELSE KW_WHILE KW_FOR KW_RETURN KW_TENSOR
%token PLUS MINUS STAR SLASH PERCENT
%token EQEQ NEQ LT GT LE GE
%token ANDAND OROR NOT
%token ASSIGN PLUSEQ MINUSEQ STAREQ SLASHEQ
%token ARROW AMP
%token LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET SEMICOLON COMMA DOT
%token INVALID

%token <long long> INT_LITERAL CHAR_LITERAL
%token <double> FLOAT_LITERAL
%token <std::string> IDENTIFIER STRING_LITERAL

%type <compiler::ast::TypeInfo> type_specifier
%type <std::string> tensor_elem
%type <std::vector<long long>> dim_list
%type <compiler::ast::ParamDecl> parameter_declaration
%type <compiler::ast::FieldDecl> field_declaration

%type <std::vector<compiler::ast::ParamDecl>> parameter_list parameter_list_opt
%type <std::vector<compiler::ast::FieldDecl>> field_declaration_list
%type <std::vector<std::unique_ptr<compiler::ast::ASTNode>>> external_declaration_list block_item_list argument_expression_list argument_expression_list_opt

%type <std::unique_ptr<compiler::ast::ASTNode>>
  external_declaration
  function_definition
  declaration
  struct_declaration
  statement
  compound_stmt
  selection_stmt
  iteration_stmt
  jump_stmt
  expr_stmt
  for_init_statement
  expression
  opt_expression
  assignment_expression
  logical_or_expression
  logical_and_expression
  equality_expression
  relational_expression
  additive_expression
  multiplicative_expression
  unary_expression
  postfix_expression
  primary_expression

%type <std::unique_ptr<compiler::ast::TranslationUnit>> translation_unit

%right ASSIGN PLUSEQ MINUSEQ STAREQ SLASHEQ
%left OROR
%left ANDAND
%left EQEQ NEQ
%left LT GT LE GE
%left PLUS MINUS
%left STAR SLASH PERCENT
%right NOT UMINUS AMP USTAR
%left LPAREN LBRACKET DOT ARROW
%nonassoc LOWER_THAN_ELSE
%nonassoc KW_ELSE

%%
translation_unit
  : external_declaration_list
    {
      auto unit = std::make_unique<compiler::ast::TranslationUnit>();
      unit->decls = std::move($1);
      driver.result = std::move(unit);
      $$ = nullptr;
    }
  ;

external_declaration_list
  : /* empty */ { $$ = std::vector<std::unique_ptr<compiler::ast::ASTNode>>(); }
  | external_declaration_list external_declaration
    {
      $$ = std::move($1);
      if ($2) {
        $$.push_back(std::move($2));
      }
    }
  ;

external_declaration
  : function_definition { $$ = std::move($1); }
  | declaration SEMICOLON { $$ = std::move($1); }
  | struct_declaration SEMICOLON { $$ = std::move($1); }
  | error SEMICOLON
    {
      driver.report("syntax error in top-level declaration");
      yyerrok;
      $$ = nullptr;
    }
  ;

function_definition
  : type_specifier IDENTIFIER LPAREN parameter_list_opt RPAREN compound_stmt
    {
      auto fn = std::make_unique<compiler::ast::FunctionDecl>();
      fn->line = driver.last_line;
      fn->return_type = std::move($1);
      fn->name = std::move($2);
      fn->params = std::move($4);
      auto* body = dynamic_cast<compiler::ast::CompoundStmt*>($6.release());
      if (body == nullptr) {
        driver.report("function body must be a compound statement");
        $$ = nullptr;
      } else {
        fn->body.reset(body);
        fn->line = body->line;
        $$ = std::move(fn);
      }
    }
  ;

parameter_list_opt
  : /* empty */ { $$ = {}; }
  | parameter_list { $$ = std::move($1); }
  ;

parameter_list
  : parameter_declaration { $$ = {std::move($1)}; }
  | parameter_list COMMA parameter_declaration
    {
      $$ = std::move($1);
      $$.push_back(std::move($3));
    }
  ;

parameter_declaration
  : type_specifier IDENTIFIER
    {
      compiler::ast::ParamDecl param;
      param.type = std::move($1);
      param.name = std::move($2);
      $$ = std::move(param);
    }
  ;

field_declaration_list
  : /* empty */ { $$ = {}; }
  | field_declaration_list field_declaration
    {
      $$ = std::move($1);
      $$.push_back(std::move($2));
    }
  ;

field_declaration
  : type_specifier IDENTIFIER SEMICOLON
    {
      compiler::ast::FieldDecl field;
      field.type = std::move($1);
      field.name = std::move($2);
      $$ = std::move(field);
    }
  ;

struct_declaration
  : KW_STRUCT IDENTIFIER LBRACE field_declaration_list RBRACE
    {
      auto st = std::make_unique<compiler::ast::StructDecl>();
      st->line = driver.last_line;
      st->name = std::move($2);
      st->fields = std::move($4);
      $$ = std::move(st);
    }
  ;

type_specifier
  : KW_INT { compiler::ast::TypeInfo t; t.name = "int"; $$ = std::move(t); }
  | KW_FLOAT { compiler::ast::TypeInfo t; t.name = "float"; $$ = std::move(t); }
  | KW_CHAR { compiler::ast::TypeInfo t; t.name = "char"; $$ = std::move(t); }
  | KW_VOID { compiler::ast::TypeInfo t; t.name = "void"; $$ = std::move(t); }
  | KW_STRUCT IDENTIFIER
    {
      compiler::ast::TypeInfo t;
      t.name = "struct " + $2;
      $$ = std::move(t);
    }
  | KW_TENSOR LT tensor_elem COMMA dim_list GT
    {
      // Canonical name, e.g. "tensor<float,2,3>"; sema parses it back into a shape.
      compiler::ast::TypeInfo t;
      t.name = "tensor<" + $3;
      for (long long d : $5) {
        t.name += "," + std::to_string(d);
      }
      t.name += ">";
      $$ = std::move(t);
    }
  ;

tensor_elem
  : KW_INT { $$ = "int"; }
  | KW_FLOAT { $$ = "float"; }
  ;

dim_list
  : INT_LITERAL { $$ = {$1}; }
  | dim_list COMMA INT_LITERAL
    {
      $$ = std::move($1);
      $$.push_back($3);
    }
  ;

declaration
  : type_specifier IDENTIFIER
    {
      auto decl = std::make_unique<compiler::ast::VarDecl>();
      decl->line = driver.last_line;
      decl->type = std::move($1);
      decl->name = std::move($2);
      $$ = std::move(decl);
    }
  | type_specifier IDENTIFIER ASSIGN expression
    {
      auto decl = std::make_unique<compiler::ast::VarDecl>();
      decl->line = driver.last_line;
      decl->type = std::move($1);
      decl->name = std::move($2);
      decl->init = std::move($4);
      $$ = std::move(decl);
    }
  ;

compound_stmt
  : LBRACE block_item_list RBRACE
    {
      auto compound = std::make_unique<compiler::ast::CompoundStmt>();
      compound->line = driver.last_line;
      compound->stmts = std::move($2);
      $$ = std::move(compound);
    }
  | LBRACE error RBRACE
    {
      driver.report("invalid compound statement");
      yyerrok;
      $$ = std::make_unique<compiler::ast::CompoundStmt>();
    }
  ;

block_item_list
  : /* empty */ { $$ = std::vector<std::unique_ptr<compiler::ast::ASTNode>>(); }
  | block_item_list statement
    {
      $$ = std::move($1);
      if ($2) {
        $$.push_back(std::move($2));
      }
    }
  ;

statement
  : compound_stmt { $$ = std::move($1); }
  | selection_stmt { $$ = std::move($1); }
  | iteration_stmt { $$ = std::move($1); }
  | jump_stmt { $$ = std::move($1); }
  | expr_stmt { $$ = std::move($1); }
  | declaration SEMICOLON { $$ = std::move($1); }
  | error SEMICOLON
    {
      driver.report("invalid statement");
      yyerrok;
      $$ = std::make_unique<compiler::ast::ExprStmt>();
    }
  ;

selection_stmt
  : KW_IF LPAREN expression RPAREN statement %prec LOWER_THAN_ELSE
    {
      auto node = std::make_unique<compiler::ast::IfStmt>();
      node->line = driver.last_line;
      node->cond = std::move($3);
      node->then_branch = std::move($5);
      $$ = std::move(node);
    }
  | KW_IF LPAREN expression RPAREN statement KW_ELSE statement
    {
      auto node = std::make_unique<compiler::ast::IfStmt>();
      node->line = driver.last_line;
      node->cond = std::move($3);
      node->then_branch = std::move($5);
      node->else_branch = std::move($7);
      $$ = std::move(node);
    }
  ;

iteration_stmt
  : KW_WHILE LPAREN expression RPAREN statement
    {
      auto node = std::make_unique<compiler::ast::WhileStmt>();
      node->line = driver.last_line;
      node->cond = std::move($3);
      node->body = std::move($5);
      $$ = std::move(node);
    }
  | KW_FOR LPAREN for_init_statement opt_expression SEMICOLON opt_expression RPAREN statement
    {
      auto node = std::make_unique<compiler::ast::ForStmt>();
      node->line = driver.last_line;
      node->init = std::move($3);
      node->cond = std::move($4);
      node->incr = std::move($6);
      node->body = std::move($8);
      $$ = std::move(node);
    }
  ;

for_init_statement
  : SEMICOLON { $$ = nullptr; }
  | expression SEMICOLON { $$ = std::move($1); }
  | declaration SEMICOLON { $$ = std::move($1); }
  ;

jump_stmt
  : KW_RETURN SEMICOLON
    {
      auto node = std::make_unique<compiler::ast::ReturnStmt>();
      node->line = driver.last_line;
      $$ = std::move(node);
    }
  | KW_RETURN expression SEMICOLON
    {
      auto node = std::make_unique<compiler::ast::ReturnStmt>();
      node->line = driver.last_line;
      node->value = std::move($2);
      $$ = std::move(node);
    }
  ;

expr_stmt
  : SEMICOLON
    {
      $$ = std::make_unique<compiler::ast::ExprStmt>();
    }
  | expression SEMICOLON
    {
      auto node = std::make_unique<compiler::ast::ExprStmt>();
      node->line = driver.last_line;
      node->expr = std::move($1);
      $$ = std::move(node);
    }
  ;

opt_expression
  : /* empty */ { $$ = nullptr; }
  | expression { $$ = std::move($1); }
  ;

expression
  : assignment_expression { $$ = std::move($1); }
  ;

assignment_expression
  : logical_or_expression { $$ = std::move($1); }
  | unary_expression ASSIGN assignment_expression
    { $$ = make_binary("=", std::move($1), std::move($3), driver.last_line); }
  | unary_expression PLUSEQ assignment_expression
    { $$ = make_binary("+=", std::move($1), std::move($3), driver.last_line); }
  | unary_expression MINUSEQ assignment_expression
    { $$ = make_binary("-=", std::move($1), std::move($3), driver.last_line); }
  | unary_expression STAREQ assignment_expression
    { $$ = make_binary("*=", std::move($1), std::move($3), driver.last_line); }
  | unary_expression SLASHEQ assignment_expression
    { $$ = make_binary("/=", std::move($1), std::move($3), driver.last_line); }
  ;

logical_or_expression
  : logical_and_expression { $$ = std::move($1); }
  | logical_or_expression OROR logical_and_expression
    { $$ = make_binary("||", std::move($1), std::move($3), driver.last_line); }
  ;

logical_and_expression
  : equality_expression { $$ = std::move($1); }
  | logical_and_expression ANDAND equality_expression
    { $$ = make_binary("&&", std::move($1), std::move($3), driver.last_line); }
  ;

equality_expression
  : relational_expression { $$ = std::move($1); }
  | equality_expression EQEQ relational_expression
    { $$ = make_binary("==", std::move($1), std::move($3), driver.last_line); }
  | equality_expression NEQ relational_expression
    { $$ = make_binary("!=", std::move($1), std::move($3), driver.last_line); }
  ;

relational_expression
  : additive_expression { $$ = std::move($1); }
  | relational_expression LT additive_expression
    { $$ = make_binary("<", std::move($1), std::move($3), driver.last_line); }
  | relational_expression GT additive_expression
    { $$ = make_binary(">", std::move($1), std::move($3), driver.last_line); }
  | relational_expression LE additive_expression
    { $$ = make_binary("<=", std::move($1), std::move($3), driver.last_line); }
  | relational_expression GE additive_expression
    { $$ = make_binary(">=", std::move($1), std::move($3), driver.last_line); }
  ;

additive_expression
  : multiplicative_expression { $$ = std::move($1); }
  | additive_expression PLUS multiplicative_expression
    { $$ = make_binary("+", std::move($1), std::move($3), driver.last_line); }
  | additive_expression MINUS multiplicative_expression
    { $$ = make_binary("-", std::move($1), std::move($3), driver.last_line); }
  ;

multiplicative_expression
  : unary_expression { $$ = std::move($1); }
  | multiplicative_expression STAR unary_expression
    { $$ = make_binary("*", std::move($1), std::move($3), driver.last_line); }
  | multiplicative_expression SLASH unary_expression
    { $$ = make_binary("/", std::move($1), std::move($3), driver.last_line); }
  | multiplicative_expression PERCENT unary_expression
    { $$ = make_binary("%", std::move($1), std::move($3), driver.last_line); }
  ;

unary_expression
  : postfix_expression { $$ = std::move($1); }
  | NOT unary_expression %prec NOT
    { $$ = make_unary("!", std::move($2), driver.last_line); }
  | MINUS unary_expression %prec UMINUS
    { $$ = make_unary("-", std::move($2), driver.last_line); }
  | AMP unary_expression %prec AMP
    { $$ = make_unary("&", std::move($2), driver.last_line); }
  | STAR unary_expression %prec USTAR
    { $$ = make_unary("*", std::move($2), driver.last_line); }
  ;

postfix_expression
  : primary_expression { $$ = std::move($1); }
  | postfix_expression LPAREN argument_expression_list_opt RPAREN
    {
      auto call = std::make_unique<compiler::ast::CallExpr>();
      call->line = driver.last_line;
      if (auto* var = dynamic_cast<compiler::ast::VarRef*>($1.get())) {
        call->callee = var->name;
      } else {
        driver.report("function call requires identifier callee");
        call->callee = "<invalid>";
      }
      call->args = std::move($3);
      $$ = std::move(call);
    }
  | postfix_expression LBRACKET expression RBRACKET
    {
      auto sub = std::make_unique<compiler::ast::ArraySubscript>();
      sub->line = driver.last_line;
      sub->array = std::move($1);
      sub->index = std::move($3);
      $$ = std::move(sub);
    }
  | postfix_expression DOT IDENTIFIER
    {
      auto member = std::make_unique<compiler::ast::MemberExpr>();
      member->line = driver.last_line;
      member->object = std::move($1);
      member->member = std::move($3);
      member->is_arrow = false;
      $$ = std::move(member);
    }
  | postfix_expression ARROW IDENTIFIER
    {
      auto member = std::make_unique<compiler::ast::MemberExpr>();
      member->line = driver.last_line;
      member->object = std::move($1);
      member->member = std::move($3);
      member->is_arrow = true;
      $$ = std::move(member);
    }
  ;

argument_expression_list_opt
  : /* empty */ { $$ = std::vector<std::unique_ptr<compiler::ast::ASTNode>>(); }
  | argument_expression_list { $$ = std::move($1); }
  ;

argument_expression_list
  : assignment_expression
    {
      $$ = std::vector<std::unique_ptr<compiler::ast::ASTNode>>();
      $$.push_back(std::move($1));
    }
  | argument_expression_list COMMA assignment_expression
    {
      $$ = std::move($1);
      $$.push_back(std::move($3));
    }
  ;

primary_expression
  : IDENTIFIER
    {
      auto ref = std::make_unique<compiler::ast::VarRef>();
      ref->line = driver.last_line;
      ref->name = std::move($1);
      $$ = std::move(ref);
    }
  | INT_LITERAL
    {
      auto lit = std::make_unique<compiler::ast::IntLiteral>();
      lit->line = driver.last_line;
      lit->value = $1;
      $$ = std::move(lit);
    }
  | FLOAT_LITERAL
    {
      auto lit = std::make_unique<compiler::ast::FloatLiteral>();
      lit->line = driver.last_line;
      lit->value = $1;
      $$ = std::move(lit);
    }
  | CHAR_LITERAL
    {
      auto lit = std::make_unique<compiler::ast::CharLiteral>();
      lit->line = driver.last_line;
      lit->value = static_cast<char>($1);
      $$ = std::move(lit);
    }
  | STRING_LITERAL
    {
      auto lit = std::make_unique<compiler::ast::StringLiteral>();
      lit->line = driver.last_line;
      lit->value = std::move($1);
      $$ = std::move(lit);
    }
  | LBRACKET argument_expression_list RBRACKET
    {
      auto lit = std::make_unique<compiler::ast::TensorLiteral>();
      lit->line = driver.last_line;
      lit->elements = std::move($2);
      $$ = std::move(lit);
    }
  | LPAREN expression RPAREN { $$ = std::move($2); }
  ;

%%

void yy::parser::error(const std::string& message) {
  driver.report(message);
}
