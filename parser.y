%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
extern int yylex();
void yyerror(const char *s);
ASTNode *root;  // Global AST root.
%}

%code requires {
  #include "ast.h"
}

/* Semantic value union */
%union {
    char* str;
    ASTNode* node;
}

/* Token declarations */
%token VAR TYPE INT FLOAT BOOL CHAR STRING PRINT LOOP IF WHILE UNTIL ASSIGN IS SEMICOLON LPAREN RPAREN LBRACE RBRACE ELSE INPUT
%token LT GT
%token LE GE
%token EQ NE
%token AND OR
%token NOT
%token PLUS MINUS MULTIPLY DIVIDE
%token <str> NUMBER FLOAT_NUMBER CHAR_LITERAL IDENTIFIER BOOLEAN STRING_LITERAL
%token FUN RETURN COMMA
%token SWITCH CASE DEFAULT BREAK
%token LBRACKET RBRACKET
%token INLINE

/* Precedence declarations */
%right ASSIGN IS
%left OR
%left AND
%nonassoc EQ NE
%nonassoc LT GT LE GE
%left PLUS MINUS
%left MULTIPLY DIVIDE

/* Nonterminals */
%type <node> program global_declarations global_declaration statements statement loop_header expression logical_or_expression logical_and_expression equality_expression relational_expression additive_expression multiplicative_expression primary else_if_ladder_opt if_ladder
%type <node> function_definition parameter_list_opt parameter_list parameter function_body argument_list_opt argument_list
%type <node> case_list case_clause default_clause
%type <node> element_list

/* Start symbol */
%start program

%%

program:
    global_declarations { root = $1; }
    ;

global_declarations:
      global_declaration                { $$ = $1; }
    | global_declarations global_declaration  { $$ = createASTNode("GLOBAL_LIST", NULL, $1, $2); }
    ;

global_declaration:
      function_definition               { $$ = $1; }
    | statement                         { $$ = $1; }
    ;

/* --- Function Definitions --- */
function_definition:
    FUN IDENTIFIER LPAREN parameter_list_opt RPAREN LBRACE function_body RBRACE
          { $$ = createASTNode("FUNC_DEF", $2, $4, $7); }
    ;

/* Parameter List */
parameter_list_opt:
      /* empty */                       { $$ = NULL; }
    | parameter_list                    { $$ = $1; }
    ;

parameter_list:
      parameter                         { $$ = $1; }
    | parameter_list COMMA parameter    { $$ = createASTNode("PARAM_LIST", NULL, $1, $3); }
    ;

parameter:
      INT IDENTIFIER                    { $$ = createASTNode("PARAM", $2, createASTNode("TYPE_LITERAL", "int", NULL, NULL), NULL); }
    | FLOAT IDENTIFIER                  { $$ = createASTNode("PARAM", $2, createASTNode("TYPE_LITERAL", "float", NULL, NULL), NULL); }
    | BOOL IDENTIFIER                   { $$ = createASTNode("PARAM", $2, createASTNode("TYPE_LITERAL", "bool", NULL, NULL), NULL); }
    | CHAR IDENTIFIER                   { $$ = createASTNode("PARAM", $2, createASTNode("TYPE_LITERAL", "char", NULL, NULL), NULL); }
    | STRING IDENTIFIER                 { $$ = createASTNode("PARAM", $2, createASTNode("TYPE_LITERAL", "string", NULL, NULL), NULL); }
    ;

function_body:
    statements                        { $$ = $1; }
    ;

/* --- Argument List --- */
argument_list_opt:
      /* empty */                       { $$ = NULL; }
    | argument_list                     { $$ = $1; }
    ;

argument_list:
      expression                        { $$ = $1; }
    | argument_list COMMA expression    { $$ = createASTNode("ARG_LIST", NULL, $1, $3); }
    ;

/* --- Loop Header --- 
     This nonterminal distinguishes among three loop forms:
       - Iterator-based: "loop i : exp { ... }"
       - Range-based for-loop: "loop exp : exp { ... }"
       - Simple loop: "loop exp { ... }"
*/
loop_header:
      IDENTIFIER ':' expression         { $$ = createASTNode("FOR_LOOP", $1, createASTNode("RANGE", NULL, NULL, $3), NULL); }
    | expression ':' expression         { $$ = createASTNode("FOR_LOOP", NULL, createASTNode("RANGE", NULL, $1, $3), NULL); }
    | expression                        { $$ = createASTNode("LOOP", NULL, $1, NULL); }
    ;

/* --- Element List for Arrays --- */
element_list:
      expression                        { $$ = $1; }
    | element_list COMMA expression     { $$ = createASTNode("ARRAY_ELEM_LIST", NULL, $1, $3); }
    ;

/* --- Statements --- */
statements:
      statement                         { $$ = $1; }
    | statements statement              { $$ = createASTNode("STATEMENT_LIST", NULL, $1, $2); }
    ;

statement:
    INPUT LPAREN IDENTIFIER RPAREN SEMICOLON
          { $$ = createASTNode("INPUT", $3, NULL, NULL); }
    | IF LPAREN expression RPAREN LBRACE statements RBRACE else_if_ladder_opt {
            if ($8 == NULL)
              $$ = createASTNode("IF", NULL, $3, $6);
            else
              $$ = createASTNode("IF_CHAIN", NULL, createASTNode("IF", NULL, $3, $6), $8);
          }
    /* Array Iterator alternative */
    | LOOP IDENTIFIER ':' IDENTIFIER LBRACE statements RBRACE {
          $$ = createASTNode("ARRAY_ITERATOR", $2, createASTNode("IDENTIFIER", $4, NULL, NULL), $6);
      }
    | LOOP loop_header LBRACE statements RBRACE
          { $2->right = $4; $$ = $2; }
    | LOOP UNTIL LPAREN expression RPAREN LBRACE statements RBRACE
          { $$ = createASTNode("LOOP_UNTIL", NULL, $4, $7); }
    | WHILE UNTIL expression LBRACE statements RBRACE
          { $$ = createASTNode("LOOP_UNTIL", NULL, $3, $5); }
    | INT IDENTIFIER ASSIGN expression SEMICOLON
          { $$ = createASTNode("ASSIGN_INT", $2, $4, NULL); }
    | INT IDENTIFIER IS expression SEMICOLON
          { $$ = createASTNode("ASSIGN_INT", $2, $4, NULL); }
    | FLOAT IDENTIFIER ASSIGN expression SEMICOLON
          { $$ = createASTNode("ASSIGN_FLOAT", $2, $4, NULL); }
    | FLOAT IDENTIFIER IS expression SEMICOLON
          { $$ = createASTNode("ASSIGN_FLOAT", $2, $4, NULL); }
    | BOOL IDENTIFIER ASSIGN BOOLEAN SEMICOLON
          { $$ = createASTNode("ASSIGN_BOOL", $2, createASTNode("BOOLEAN", $4, NULL, NULL), NULL); }
    | BOOL IDENTIFIER IS BOOLEAN SEMICOLON
          { $$ = createASTNode("ASSIGN_BOOL", $2, createASTNode("BOOLEAN", $4, NULL, NULL), NULL); }
    | CHAR IDENTIFIER ASSIGN CHAR_LITERAL SEMICOLON
          { $$ = createASTNode("ASSIGN_CHAR", $2, createASTNode("CHAR", $4, NULL, NULL), NULL); }
    | CHAR IDENTIFIER IS CHAR_LITERAL SEMICOLON
          { $$ = createASTNode("ASSIGN_CHAR", $2, createASTNode("CHAR", $4, NULL, NULL), NULL); }
    | STRING IDENTIFIER ASSIGN expression SEMICOLON
          { $$ = createASTNode("ASSIGN_STRING", $2, $4, NULL); }
    | STRING IDENTIFIER IS expression SEMICOLON
          { $$ = createASTNode("ASSIGN_STRING", $2, $4, NULL); }
    | VAR IDENTIFIER ASSIGN expression SEMICOLON
          { $$ = createASTNode("VAR_DECL", $2, $4, NULL); }
    | VAR IDENTIFIER IS expression SEMICOLON
          { $$ = createASTNode("VAR_DECL", $2, $4, NULL); }
    | INLINE LPAREN expression RPAREN SEMICOLON
          { $$ = createASTNode("INLINE", NULL, $3, NULL); }
    | PRINT LPAREN expression RPAREN SEMICOLON
          { $$ = createASTNode("PRINT", NULL, $3, NULL); }
    /* New alternative for print newline */
    | PRINT LPAREN RPAREN SEMICOLON
          { $$ = createASTNode("PRINT_NEWLINE", NULL, NULL, NULL); }
    | IDENTIFIER LBRACKET expression RBRACKET ASSIGN expression SEMICOLON
          { $$ = createASTNode("ARRAY_ASSIGN", $1, $3, $6); }
    | TYPE LPAREN expression RPAREN
          { $$ = createASTNode("TYPE", NULL, $3, NULL); }
    | IDENTIFIER ASSIGN expression SEMICOLON
          { $$ = createASTNode("REASSIGN", $1, $3, NULL); }
    | INT IDENTIFIER SEMICOLON
          { $$ = createASTNode("DECL_INT", $2, NULL, NULL); }
    | FLOAT IDENTIFIER SEMICOLON
          { $$ = createASTNode("DECL_FLOAT", $2, NULL, NULL); }
    | BOOL IDENTIFIER SEMICOLON
          { $$ = createASTNode("DECL_BOOL", $2, NULL, NULL); }
    | CHAR IDENTIFIER SEMICOLON
          { $$ = createASTNode("DECL_CHAR", $2, NULL, NULL); }
    | STRING IDENTIFIER SEMICOLON
          { $$ = createASTNode("DECL_STRING", $2, NULL, NULL); }
    | INT IDENTIFIER LBRACKET expression RBRACKET SEMICOLON
          { $$ = createASTNode("DECL_ARRAY", $2, $4, NULL); }
    | INT IDENTIFIER LBRACKET RBRACKET ASSIGN LBRACE element_list RBRACE SEMICOLON
          { $$ = createASTNode("DECL_ARRAY_INIT", $2, $7, NULL); }
    | RETURN LPAREN expression RPAREN SEMICOLON
          { $$ = createASTNode("RETURN", NULL, $3, NULL); }
    | IDENTIFIER LPAREN argument_list_opt RPAREN SEMICOLON
          { $$ = createASTNode("CALL", $1, $3, NULL); }
    | SWITCH LPAREN expression RPAREN LBRACE case_list default_clause RBRACE
          { $$ = createASTNode("SWITCH", NULL, $3, createASTNode("SWITCH_BODY", NULL, $6, $7)); }
    | BREAK SEMICOLON
          { $$ = createASTNode("BREAK", NULL, NULL, NULL); }
    ;

else_if_ladder_opt:
      /* empty */ { $$ = NULL; }
    | ELSE if_ladder { $$ = $2; }
    ;

if_ladder:
    IF LPAREN expression RPAREN LBRACE statements RBRACE else_if_ladder_opt
          { $$ = createASTNode("ELSE_IF", NULL, $3, createASTNode("IF_ELSE_BODY", NULL, $6, $8)); }
    | LBRACE statements RBRACE
          { $$ = createASTNode("ELSE", NULL, $2, NULL); }
    ;

/* Case list for switch-case */
case_list:
      case_clause { $$ = $1; }
    | case_list case_clause { $$ = createASTNode("CASE_LIST", NULL, $1, $2); }
    ;

/* A case clause */
case_clause:
      CASE expression statements { $$ = createASTNode("CASE", NULL, $2, $3); }
    ;

/* Default clause for switch-case */
default_clause:
      /* empty */ { $$ = NULL; }
    | DEFAULT ':' statements { $$ = createASTNode("DEFAULT", NULL, $3, NULL); }
    ;

expression:
    logical_or_expression { $$ = $1; }
    ;

logical_or_expression:
    logical_or_expression OR logical_and_expression { $$ = createASTNode("OR", "||", $1, $3); }
    | logical_and_expression { $$ = $1; }
    ;

logical_and_expression:
    logical_and_expression AND equality_expression { $$ = createASTNode("AND", "&&", $1, $3); }
    | equality_expression { $$ = $1; }
    ;

equality_expression:
    equality_expression EQ relational_expression { $$ = createASTNode("EQ", "==", $1, $3); }
    | equality_expression NE relational_expression { $$ = createASTNode("NE", "!=", $1, $3); }
    | relational_expression { $$ = $1; }
    ;

relational_expression:
    relational_expression LT additive_expression { $$ = createASTNode("LT", "<", $1, $3); }
    | relational_expression GT additive_expression { $$ = createASTNode("GT", ">", $1, $3); }
    | relational_expression LE additive_expression { $$ = createASTNode("LE", "<=", $1, $3); }
    | relational_expression GE additive_expression { $$ = createASTNode("GE", ">=", $1, $3); }
    | additive_expression { $$ = $1; }
    ;

additive_expression:
    additive_expression PLUS multiplicative_expression { $$ = createASTNode("ADD", "+", $1, $3); }
    | additive_expression MINUS multiplicative_expression { $$ = createASTNode("SUB", "-", $1, $3); }
    | multiplicative_expression { $$ = $1; }
    ;

multiplicative_expression:
    multiplicative_expression MULTIPLY primary { $$ = createASTNode("MUL", "*", $1, $3); }
    | multiplicative_expression DIVIDE primary { $$ = createASTNode("DIV", "/", $1, $3); }
    | primary { $$ = $1; }
    ;

primary:
    NOT primary { $$ = createASTNode("NOT", "!", $2, NULL); }
    | MINUS primary { $$ = createASTNode("NEG", "-", $2, NULL); }
    | NUMBER { $$ = createASTNode("NUMBER", $1, NULL, NULL); }
    | FLOAT_NUMBER { $$ = createASTNode("FLOAT", $1, NULL, NULL); }
    | BOOLEAN { $$ = createASTNode("BOOLEAN", $1, NULL, NULL); }
    | CHAR_LITERAL { $$ = createASTNode("CHAR", $1, NULL, NULL); }
    | STRING_LITERAL { $$ = createASTNode("STRING", $1, NULL, NULL); }
    | IDENTIFIER LPAREN argument_list_opt RPAREN { $$ = createASTNode("CALL", $1, $3, NULL); }
    | IDENTIFIER LBRACKET expression RBRACKET { $$ = createASTNode("ARRAY_ACCESS", $1, $3, NULL); }
    | IDENTIFIER { $$ = createASTNode("IDENTIFIER", $1, NULL, NULL); }
    | LPAREN expression RPAREN { $$ = $2; }
    ;

%%

void yyerror(const char *s) {
  fprintf(stderr, "Error: %s\n", s);
}
