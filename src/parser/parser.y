%{
#include <cstdio>
#include <cstdlib>

int yylex();
void yyerror(const char* s);
%}

%token KW_INT KW_FLOAT KW_CHAR KW_VOID KW_STRUCT KW_IF KW_ELSE KW_WHILE KW_FOR KW_RETURN
%token T_EQEQ T_NEQ T_LE T_GE T_ANDAND T_OROR T_PLUSEQ T_MINUSEQ T_MULEQ T_DIVEQ T_ARROW
%token T_FLOAT_LITERAL T_INT_LITERAL T_CHAR_LITERAL T_STRING_LITERAL T_IDENTIFIER T_INVALID

%%
translation_unit
  : /* empty */
  ;
%%

void yyerror(const char* s) {
  std::fprintf(stderr, "parse error: %s\n", s);
}
