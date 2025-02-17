#ifndef AST_H
#define AST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ASTNode {
    char* type;              // Node type (e.g. "NUMBER", "VAR_DECL", "TYPE", etc.)
    char* value;             // Extra string value (e.g. identifier name or literal)
    struct ASTNode* left;    // Left child (for binary operators, assignments, etc.)
    struct ASTNode* right;   // Right child
} ASTNode;

ASTNode* createASTNode(char* type, char* value, ASTNode* left, ASTNode* right);
void printAST(ASTNode* node, int level);
void freeAST(ASTNode* node);

#ifdef __cplusplus
}
#endif

#endif
