#ifndef AST_H
#define AST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ASTNode {
    char* type;              // Node type (e.g. "NUMBER", "ASSIGN_INT", "IF", "INPUT", etc.)
    char* value;             // Extra string value (identifier name or literal)
    struct ASTNode* left;    // Left child (used for binary operators, list nodes, etc.)
    struct ASTNode* right;   // Right child (used for binary operators, list nodes, etc.)
} ASTNode;

ASTNode* createASTNode(char* type, char* value, ASTNode* left, ASTNode* right);
void printAST(ASTNode* node, int level);
void freeAST(ASTNode* node);

#ifdef __cplusplus
}
#endif

#endif
