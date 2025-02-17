#include <map>
#include <vector>
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "ast.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
using namespace llvm;
extern "C" int yyparse();
LLVMContext Context;
Module *TheModule = new Module("GoofyLang", Context);
IRBuilder<> Builder(Context);
std::map<std::string, Value*> NamedValues;
extern ASTNode* root;
extern void printAST(ASTNode* node, int level);
static AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const std::string &VarName, Type *type) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(type, nullptr, VarName);
}
Function* getPrintfFunction() {
    Function *printfFunc = TheModule->getFunction("printf");
    if (!printfFunc) {
        std::vector<Type*> printfArgs;
        printfArgs.push_back(PointerType::get(Type::getInt8Ty(Context), 0));
        FunctionType* printfType = FunctionType::get(Type::getInt32Ty(Context), printfArgs, true);
        printfFunc = Function::Create(printfType, Function::ExternalLinkage, "printf", TheModule);
    }
    return printfFunc;
}
GlobalVariable* getFormatStringInt() {
    GlobalVariable *fmtStrVar = TheModule->getNamedGlobal(".str_int");
    if (!fmtStrVar) {
        Constant *formatStr = ConstantDataArray::getString(Context, "%d\n", true);
        fmtStrVar = new GlobalVariable(*TheModule, formatStr->getType(), true, GlobalValue::PrivateLinkage, formatStr, ".str_int");
    }
    return fmtStrVar;
}
GlobalVariable* getFormatStringFloat() {
    GlobalVariable *fmtStrVar = TheModule->getNamedGlobal(".str_float");
    if (!fmtStrVar) {
        Constant *formatStr = ConstantDataArray::getString(Context, "%f\n", true);
        fmtStrVar = new GlobalVariable(*TheModule, formatStr->getType(), true,
            GlobalValue::PrivateLinkage, formatStr, ".str_float");
    }
    return fmtStrVar;
}
GlobalVariable* getFormatStringChar() {
    GlobalVariable *fmtStrVar = TheModule->getNamedGlobal(".str_char");
    if (!fmtStrVar) {
        Constant *formatStr = ConstantDataArray::getString(Context, "%c\n", true);
        fmtStrVar = new GlobalVariable(*TheModule, formatStr->getType(), true, GlobalValue::PrivateLinkage, formatStr, ".str_char");
    }
    return fmtStrVar;
}
GlobalVariable* getFormatStringStr() {
    GlobalVariable *fmtStrVar = TheModule->getNamedGlobal(".str_string");
    if (!fmtStrVar) {
        Constant *formatStr = ConstantDataArray::getString(Context, "%s\n", true);
        fmtStrVar = new GlobalVariable(*TheModule, formatStr->getType(), true, GlobalValue::PrivateLinkage, formatStr, ".str_string");
    }
    return fmtStrVar;
}
Function* getConcatFunction() {
    Function *concatFunc = TheModule->getFunction("concat_strings");
    if (!concatFunc) {
        std::vector<Type*> concatArgs;
        concatArgs.push_back(PointerType::get(Type::getInt8Ty(Context), 0));
        concatArgs.push_back(PointerType::get(Type::getInt8Ty(Context), 0));
        FunctionType* concatType = FunctionType::get(PointerType::get(Type::getInt8Ty(Context), 0), concatArgs, false);
        concatFunc = Function::Create(concatType, Function::ExternalLinkage, "concat_strings", TheModule);
    }
    return concatFunc;
}
Value *generateIR(ASTNode *node, Function* currentFunction) {
    if (!node) return nullptr;
    
    // Integer constant.
    if (strcmp(node->type, "NUMBER") == 0) {
        // atoi supports negative numbers.
        return ConstantInt::get(Type::getInt32Ty(Context), atoi(node->value));
    }
    // Float constant.
    if (strcmp(node->type, "FLOAT") == 0) {
        return ConstantFP::get(Type::getFloatTy(Context), strtof(node->value, nullptr));
    }
    // Boolean constant.
    if (strcmp(node->type, "BOOLEAN") == 0) {
        return ConstantInt::get(Type::getInt1Ty(Context), atoi(node->value));
    }
    // Char constant.
    if (strcmp(node->type, "CHAR") == 0) {
        if (strlen(node->value) < 3) { std::cerr << "Invalid char literal: " << node->value << "\n"; return ConstantInt::get(Type::getInt8Ty(Context), 0); }
        return ConstantInt::get(Type::getInt8Ty(Context), node->value[1]);
    }
    // String literal.
    if (strcmp(node->type, "STRING") == 0) {
        std::string strLiteral(node->value);
        if (!strLiteral.empty() && strLiteral.front() == '"' && strLiteral.back() == '"') {
            strLiteral = strLiteral.substr(1, strLiteral.size() - 2);
        }
        return Builder.CreateGlobalStringPtr(strLiteral, "strlit");
    }
    // Variable reference.
    if (strcmp(node->type, "IDENTIFIER") == 0) {
        Value* varPtr = NamedValues[node->value];
        if (!varPtr) { std::cerr << "Unknown variable: " << node->value << std::endl; return ConstantInt::get(Type::getInt32Ty(Context), 0); }
        if (AllocaInst *alloca = dyn_cast<AllocaInst>(varPtr)) {
            Type *elemType = alloca->getAllocatedType();
            return Builder.CreateLoad(elemType, varPtr, node->value);
        } else {
            PointerType *ptrType = dyn_cast<PointerType>(varPtr->getType());
            if (!ptrType || ptrType->getNumContainedTypes() < 1) { std::cerr << "Variable " << node->value << " is not a proper pointer type!" << std::endl; return ConstantInt::get(Type::getInt32Ty(Context), 0); }
            Type *elemType = ptrType->getContainedType(0);
            return Builder.CreateLoad(elemType, varPtr, node->value);
        }
    }
    // Unary minus (NEG) for negative numbers.
    if (strcmp(node->type, "NEG") == 0) {
        Value *val = generateIR(node->left, currentFunction);
        if (val->getType()->isFloatTy())
            return Builder.CreateFNeg(val, "fnegtmp");
        else
            return Builder.CreateNeg(val, "negtmp");
    }
    // Arithmetic operations.
    if (strcmp(node->type, "ADD") == 0) {
        Value *L = generateIR(node->left, currentFunction);
        Value *R = generateIR(node->right, currentFunction);
        if (L->getType() == PointerType::get(Type::getInt8Ty(Context), 0) &&
            R->getType() == PointerType::get(Type::getInt8Ty(Context), 0)) {
            Function *concatFunc = getConcatFunction();
            return Builder.CreateCall(concatFunc, {L, R}, "concat");
        }
        if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
            return Builder.CreateFAdd(L, R, "faddtmp");
        return Builder.CreateAdd(L, R, "addtmp");
    }
    if (strcmp(node->type, "SUB") == 0) {
        Value *L = generateIR(node->left, currentFunction);
        Value *R = generateIR(node->right, currentFunction);
        if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
            return Builder.CreateFSub(L, R, "fsubtmp");
        return Builder.CreateSub(L, R, "subtmp");
    }
    if (strcmp(node->type, "MUL") == 0) {
        Value *L = generateIR(node->left, currentFunction);
        Value *R = generateIR(node->right, currentFunction);
        if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
            return Builder.CreateFMul(L, R, "fmultmp");
        return Builder.CreateMul(L, R, "multmp");
    }
    if (strcmp(node->type, "DIV") == 0) {
        Value *L = generateIR(node->left, currentFunction);
        Value *R = generateIR(node->right, currentFunction);
        if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
            return Builder.CreateFDiv(L, R, "fdivtmp");
        return Builder.CreateSDiv(L, R, "divtmp");
    }
    // Assignment for integer variables.
    if (strcmp(node->type, "ASSIGN_INT") == 0) {
        std::string varName = node->value;
        Value *exprVal = generateIR(node->left, currentFunction);
        Value *varPtr = NamedValues[varName];
        if (!varPtr) { varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt32Ty(Context)); NamedValues[varName] = varPtr; }
        Builder.CreateStore(exprVal, varPtr);
        return exprVal;
    }
    if (strcmp(node->type, "DECL_FLOAT") == 0) return nullptr;
    if (strcmp(node->type, "ASSIGN_FLOAT") == 0) {
        std::string varName = node->value;
        Value *exprVal = generateIR(node->left, currentFunction);
        Value *varPtr = NamedValues[varName];
        if (!varPtr) { varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getFloatTy(Context)); NamedValues[varName] = varPtr; }
        Builder.CreateStore(exprVal, varPtr);
        return exprVal;
    }
    if (strcmp(node->type, "DECL_BOOL") == 0) return nullptr;
    if (strcmp(node->type, "ASSIGN_BOOL") == 0) {
        std::string varName = node->value;
        Value *exprVal = generateIR(node->left, currentFunction);
        if(exprVal->getType()->isIntegerTy(32))
            exprVal = Builder.CreateICmpNE(exprVal, ConstantInt::get(Type::getInt32Ty(Context), 0), "boolcast");
        Value *varPtr = NamedValues[varName];
        if (!varPtr) { varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt1Ty(Context)); NamedValues[varName] = varPtr; }
        Builder.CreateStore(exprVal, varPtr);
        return exprVal;
    }
    if (strcmp(node->type, "DECL_CHAR") == 0) return nullptr;
    if (strcmp(node->type, "ASSIGN_CHAR") == 0) {
        std::string varName = node->value;
        Value *exprVal = generateIR(node->left, currentFunction);
        Value *varPtr = NamedValues[varName];
        if (!varPtr) { varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt8Ty(Context)); NamedValues[varName] = varPtr; }
        Builder.CreateStore(exprVal, varPtr);
        return exprVal;
    }
    if (strcmp(node->type, "DECL_STRING") == 0) return nullptr;
    if (strcmp(node->type, "ASSIGN_STRING") == 0) {
        std::string varName = node->value;
        Value *exprVal = generateIR(node->left, currentFunction);
        Value *strVal = Builder.CreateBitCast(exprVal, PointerType::get(Type::getInt8Ty(Context), 0), "strcast");
        Value *varPtr = NamedValues[varName];
        if (!varPtr) { varPtr = CreateEntryBlockAlloca(currentFunction, varName, PointerType::get(Type::getInt8Ty(Context), 0)); NamedValues[varName] = varPtr; }
        Builder.CreateStore(strVal, varPtr);
        return strVal;
    }
    if (strcmp(node->type, "REASSIGN") == 0) {
        std::string varName = node->value;
        Value *varPtr = NamedValues[varName];
        if (!varPtr) { std::cerr << "Undeclared variable: " << varName << std::endl; return nullptr; }
        Value *exprVal = generateIR(node->left, currentFunction);
        Builder.CreateStore(exprVal, varPtr);
        return exprVal;
    }
    if (strcmp(node->type, "PRINT") == 0) {
        Value *exprVal = generateIR(node->left, currentFunction);
        Type *exprType = exprVal->getType();
        GlobalVariable *fmtStrVar = nullptr;
        if (exprType->isIntegerTy(1)) {
            GlobalVariable *trueStr = TheModule->getNamedGlobal(".str_true");
            if (!trueStr) { Constant *tstr = ConstantDataArray::getString(Context, "true", true); trueStr = new GlobalVariable(*TheModule, tstr->getType(), true, GlobalValue::PrivateLinkage, tstr, ".str_true"); }
            GlobalVariable *falseStr = TheModule->getNamedGlobal(".str_false");
            if (!falseStr) { Constant *fstr = ConstantDataArray::getString(Context, "false", true); falseStr = new GlobalVariable(*TheModule, fstr->getType(), true, GlobalValue::PrivateLinkage, fstr, ".str_false"); }
            Constant *zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
            std::vector<Constant*> indices = {zero, zero};
            Constant *trueStrPtr = ConstantExpr::getGetElementPtr(trueStr->getValueType(), trueStr, indices);
            Constant *falseStrPtr = ConstantExpr::getGetElementPtr(falseStr->getValueType(), falseStr, indices);
            Value *boolStr = Builder.CreateSelect(exprVal, trueStrPtr, falseStrPtr, "boolstr");
            GlobalVariable *fmtStrVarBool = TheModule->getNamedGlobal(".str_bool");
            if (!fmtStrVarBool) { Constant *formatStr = ConstantDataArray::getString(Context, "%s\n", true); fmtStrVarBool = new GlobalVariable(*TheModule, formatStr->getType(), true, GlobalValue::PrivateLinkage, formatStr, ".str_bool"); }
            fmtStrVar = fmtStrVarBool;
            exprVal = boolStr;
        } else if (exprType->isFloatTy()) {
            fmtStrVar = getFormatStringFloat();
            exprVal = Builder.CreateFPExt(exprVal, Type::getDoubleTy(Context), "toDouble");
        } else if (exprType->isIntegerTy(8)) {
            fmtStrVar = getFormatStringChar();
        } else if (exprType->isPointerTy()) {
            if (exprType == PointerType::get(Type::getInt8Ty(Context), 0))
                fmtStrVar = getFormatStringStr();
            else
                fmtStrVar = getFormatStringInt();
        } else {
            fmtStrVar = getFormatStringInt();
        }
        Constant *zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
        std::vector<Constant*> indices = {zero, zero};
        Constant *fmtStrPtr = ConstantExpr::getGetElementPtr(fmtStrVar->getValueType(), fmtStrVar, indices);
        Builder.CreateCall(getPrintfFunction(), {fmtStrPtr, exprVal});
        return exprVal;
    }
    // Loop construct.
    if (strcmp(node->type, "LOOP") == 0) {
        // Evaluate the loop count expression.
        Value *loopCountVal = generateIR(node->left, currentFunction);
        if (!loopCountVal) {
            std::cerr << "Error: Invalid loop count expression\n";
            return nullptr;
        }
        // Ensure the loop count is an i32.
        if (loopCountVal->getType() != Type::getInt32Ty(Context))
            loopCountVal = Builder.CreateIntCast(loopCountVal, Type::getInt32Ty(Context), true, "loopcount");
        
        // Create a new local variable "i" for the loop counter.
        AllocaInst *loopVar = CreateEntryBlockAlloca(currentFunction, "i", Type::getInt32Ty(Context));
        Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(Context), 0), loopVar);
        BasicBlock *loopCondBB = BasicBlock::Create(Context, "loopcond", currentFunction);
        BasicBlock *loopBodyBB = BasicBlock::Create(Context, "loopbody", currentFunction);
        BasicBlock *afterLoopBB = BasicBlock::Create(Context, "afterloop", currentFunction);
        Builder.CreateBr(loopCondBB);
        Builder.SetInsertPoint(loopCondBB);
        Value *currVal = Builder.CreateLoad(Type::getInt32Ty(Context), loopVar, "i");
        Value *cond = Builder.CreateICmpSLT(currVal, loopCountVal, "loopcond");
        Builder.CreateCondBr(cond, loopBodyBB, afterLoopBB);
        Builder.SetInsertPoint(loopBodyBB);
        // Generate IR for the loop body (node->right contains the statements).
        generateIR(node->right, currentFunction);
        Value *nextVal = Builder.CreateAdd(currVal, ConstantInt::get(Type::getInt32Ty(Context), 1), "inc");
        Builder.CreateStore(nextVal, loopVar);
        Builder.CreateBr(loopCondBB);
        Builder.SetInsertPoint(afterLoopBB);
        return ConstantInt::get(Type::getInt32Ty(Context), 0);
    }
    if (strcmp(node->type, "STATEMENT_LIST") == 0) {
        if (node->left)
            generateIR(node->left, currentFunction);
        if (node->right)
            return generateIR(node->right, currentFunction);
        return nullptr;
    }
    
    // --- Declaration cases ---
    if (strcmp(node->type, "DECL_INT") == 0) {
        std::string varName = node->value;
        if (NamedValues.find(varName) != NamedValues.end()) {
            std::cerr << "Variable " << varName << " already declared!" << std::endl;
            return nullptr;
        }
        Value* varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt32Ty(Context));
        NamedValues[varName] = varPtr;
        Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(Context), 0), varPtr);
        return varPtr;
    }
    if (strcmp(node->type, "DECL_FLOAT") == 0) {
        std::string varName = node->value;
        if (NamedValues.find(varName) != NamedValues.end()) {
            std::cerr << "Variable " << varName << " already declared!" << std::endl;
            return nullptr;
        }
        Value* varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getFloatTy(Context));
        NamedValues[varName] = varPtr;
        Builder.CreateStore(ConstantFP::get(Type::getFloatTy(Context), 0.0), varPtr);
        return varPtr;
    }
    if (strcmp(node->type, "DECL_BOOL") == 0) {
        std::string varName = node->value;
        if (NamedValues.find(varName) != NamedValues.end()) {
            std::cerr << "Variable " << varName << " already declared!" << std::endl;
            return nullptr;
        }
        Value* varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt1Ty(Context));
        NamedValues[varName] = varPtr;
        Builder.CreateStore(ConstantInt::get(Type::getInt1Ty(Context), 0), varPtr);
        return varPtr;
    }
    if (strcmp(node->type, "DECL_CHAR") == 0) {
        std::string varName = node->value;
        if (NamedValues.find(varName) != NamedValues.end()) {
            std::cerr << "Variable " << varName << " already declared!" << std::endl;
            return nullptr;
        }
        Value* varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt8Ty(Context));
        NamedValues[varName] = varPtr;
        Builder.CreateStore(ConstantInt::get(Type::getInt8Ty(Context), 0), varPtr);
        return varPtr;
    }
    if (strcmp(node->type, "DECL_STRING") == 0) {
        std::string varName = node->value;
        if (NamedValues.find(varName) != NamedValues.end()) {
            std::cerr << "Variable " << varName << " already declared!" << std::endl;
            return nullptr;
        }
        Value* varPtr = CreateEntryBlockAlloca(currentFunction, varName,
                           PointerType::get(Type::getInt8Ty(Context), 0));
        NamedValues[varName] = varPtr;
        Builder.CreateStore(ConstantPointerNull::get(PointerType::get(Type::getInt8Ty(Context), 0)), varPtr);
        return varPtr;
    }
    
    return nullptr;
}
int main() {
    if (yyparse() != 0) {
        return 1;
    }
    
    // Create the main function with return type i32.
    FunctionType *funcType = FunctionType::get(Type::getInt32Ty(Context), false);
    Function *mainFunc = Function::Create(funcType, Function::ExternalLinkage, "main", TheModule);
    BasicBlock *entryBB = BasicBlock::Create(Context, "entry", mainFunc);
    Builder.SetInsertPoint(entryBB);
    
    // Generate code for the AST. (We ignore the result.)
    generateIR(root, mainFunc);
    Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(Context), 0));
    std::string error;
    raw_string_ostream errorStream(error);
    if (verifyModule(*TheModule, &errorStream)) { std::cerr << "Error: " << errorStream.str() << "\n"; return 1; }
    TheModule->print(outs(), nullptr);
    delete TheModule;
    return 0;
}
