#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ASTNode* createASTNode(char* type, char* value, ASTNode* left, ASTNode* right) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    node->type = strdup(type);
    node->value = value ? strdup(value) : NULL;
    node->left = left;
    node->right = right;
    return node;
}

void printAST(ASTNode* node, int level) {
    if (!node) return;
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
    printf("%s: %s\n", node->type, node->value ? node->value : "NULL");
    printAST(node->left, level + 1);
    printAST(node->right, level + 1);
}

void freeAST(ASTNode* node) {
    if (!node) return;
    free(node->type);
    if (node->value)
        free(node->value);
    freeAST(node->left);
    freeAST(node->right);
    free(node);
}
