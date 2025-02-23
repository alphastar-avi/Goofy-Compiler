%{
#include "parser.tab.h"
#include <stdlib.h>
#include <string.h>
#include "ast.h"
%}

%option noyywrap

%%
"//".*                                   { /* Ignore single-line comment starting with '//' */ }
"comment:"[^\n]*                         { /* Ignore comment starting with 'comment:' */ }
"input"                                  { return INPUT; }
"lesser"[ \t\n]+"than"[ \t\n]+"equals"    { return LE; }
"greater"[ \t\n]+"than"[ \t\n]+"equals"   { return GE; }
"lesser"[ \t\n]+"than"                    { return LT; }
"greater"[ \t\n]+"than"                   { return GT; }
"equals"                                 { return EQ; }
"=="                                     { return EQ; }
"not"[ \t\n]+"equals"                     { return NE; }
"!="                                     { return NE; }
"not"                                    { return NOT; }
"!"                                      { return NOT; }
"and"                                    { return AND; }
"or"                                     { return OR; }
"while"                                  { return WHILE; }
"until"                                  { return UNTIL; }
"&&"                                     { return AND; }
"||"                                     { return OR; }
"<="                                     { return LE; }
">="                                     { return GE; }
"var"                                    { return VAR; }
"type"                                   { return TYPE; }
"int"                                    { return INT; }
"float"                                  { return FLOAT; }
"bool"                                   { return BOOL; }
"char"                                   { return CHAR; }
"str"                                    { return STRING; }
"print"                                  { return PRINT; }
"loop"                                   { return LOOP; }
"if"                                     { return IF; }
"else"                                   { return ELSE; }
"fun"                                    { return FUN; }
"return"                                 { return RETURN; }
"switch"                                 { return SWITCH; }
"case:"                                  { return CASE; }
"default"                                { return DEFAULT; }
"break"                                  { return BREAK; }
"inline"                                 { return INLINE; }
":"                                      { return ':'; }
","                                      { return COMMA; }
"<"                                      { return LT; }
">"                                      { return GT; }
"="                                      { return ASSIGN; }
";"                                      { return SEMICOLON; }
"("                                      { return LPAREN; }
")"                                      { return RPAREN; }
"{"                                      { return LBRACE; }
"}"                                      { return RBRACE; }
"["                                      { return LBRACKET; }
"]"                                      { return RBRACKET; }
"+"                                      { return PLUS; }
"-"                                      { return MINUS; }
"*"                                      { return MULTIPLY; }
"/"                                      { return DIVIDE; }
[0-9]+\.[0-9]+                          { yylval.str = strdup(yytext); return FLOAT_NUMBER; }
[0-9]+                                   { yylval.str = strdup(yytext); return NUMBER; }
"'"[^\']"'"                              { yylval.str = strdup(yytext); return CHAR_LITERAL; }
\"[^\"]*\"                              { yylval.str = strdup(yytext); return STRING_LITERAL; }
[a-zA-Z_][a-zA-Z0-9_]*                    { yylval.str = strdup(yytext); return IDENTIFIER; }
[ \t\n]+                                ;
.                                        { /* ignore any other character */ }
%%
