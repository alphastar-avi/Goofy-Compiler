#include <map>
#include <vector>
#include <functional>
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
#include "llvm/Support/ErrorHandling.h"
#include "ast.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
using namespace llvm;

// Forward declarations for helper functions.
Value *generateIR(ASTNode *node, Function* currentFunction);
void extractParams(ASTNode* paramNode, std::vector<Type*>& types, std::vector<std::string>& names);
void extractArgs(ASTNode* argNode, std::vector<Value*>& args, Function* currentFunction);
void generateFunctions(ASTNode* node);
void generateGlobalStatements(ASTNode* node, Function* mainFunc);

extern "C" int yyparse();
extern ASTNode* root;
extern void printAST(ASTNode* node, int level);

LLVMContext Context;
Module *TheModule = new Module("GoofyLang", Context);
IRBuilder<> Builder(Context);
std::map<std::string, Value*> NamedValues;

// This stack keeps track of the merge block for the currently active switch statement.
static std::vector<BasicBlock*> SwitchMergeStack;

// Utility: Create an alloca in the entry block.
static AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const std::string &VarName, Type *type) {
  IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(type, nullptr, VarName);
}

// Helper: Get or create declaration for printf.
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

// Global format strings.
GlobalVariable* getFormatStringInt() {
  GlobalVariable *fmtStrVar = TheModule->getNamedGlobal(".str_int");
  if (!fmtStrVar) {
    Constant *formatStr = ConstantDataArray::getString(Context, "%d\n", true);
    fmtStrVar = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                   GlobalValue::PrivateLinkage, formatStr, ".str_int");
  }
  return fmtStrVar;
}

GlobalVariable* getFormatStringFloat() {
  GlobalVariable *fmtStrVar = TheModule->getNamedGlobal(".str_float");
  if (!fmtStrVar) {
    Constant *formatStr = ConstantDataArray::getString(Context, "%.1f\n", true);
    fmtStrVar = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                   GlobalValue::PrivateLinkage, formatStr, ".str_float");
  }
  return fmtStrVar;
}

GlobalVariable* getFormatStringChar() {
  GlobalVariable *fmtStrVar = TheModule->getNamedGlobal(".str_char");
  if (!fmtStrVar) {
    Constant *formatStr = ConstantDataArray::getString(Context, "%c\n", true);
    fmtStrVar = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                   GlobalValue::PrivateLinkage, formatStr, ".str_char");
  }
  return fmtStrVar;
}

GlobalVariable* getFormatStringStr() {
  GlobalVariable *fmtStrVar = TheModule->getNamedGlobal(".str_string");
  if (!fmtStrVar) {
    Constant *formatStr = ConstantDataArray::getString(Context, "%s\n", true);
    fmtStrVar = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                   GlobalValue::PrivateLinkage, formatStr, ".str_string");
  }
  return fmtStrVar;
}

// Helper: String concatenation.
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

// Input runtime functions.
Function* getReadIntFunction() {
  Function *readIntFunc = TheModule->getFunction("read_int");
  if (!readIntFunc) {
    FunctionType* funcType = FunctionType::get(Type::getInt32Ty(Context), false);
    readIntFunc = Function::Create(funcType, Function::ExternalLinkage, "read_int", TheModule);
  }
  return readIntFunc;
}
Function* getReadFloatFunction() {
  Function *readFloatFunc = TheModule->getFunction("read_float");
  if (!readFloatFunc) {
    FunctionType* funcType = FunctionType::get(Type::getFloatTy(Context), false);
    readFloatFunc = Function::Create(funcType, Function::ExternalLinkage, "read_float", TheModule);
  }
  return readFloatFunc;
}
Function* getReadBoolFunction() {
  Function *readBoolFunc = TheModule->getFunction("read_bool");
  if (!readBoolFunc) {
    FunctionType* funcType = FunctionType::get(Type::getInt1Ty(Context), false);
    readBoolFunc = Function::Create(funcType, Function::ExternalLinkage, "read_bool", TheModule);
  }
  return readBoolFunc;
}
Function* getReadCharFunction() {
  Function *readCharFunc = TheModule->getFunction("read_char");
  if (!readCharFunc) {
    FunctionType* funcType = FunctionType::get(Type::getInt8Ty(Context), false);
    readCharFunc = Function::Create(funcType, Function::ExternalLinkage, "read_char", TheModule);
  }
  return readCharFunc;
}
Function* getReadStringFunction() {
  Function *readStrFunc = TheModule->getFunction("read_string");
  if (!readStrFunc) {
    FunctionType* funcType = FunctionType::get(PointerType::get(Type::getInt8Ty(Context), 0), false);
    readStrFunc = Function::Create(funcType, Function::ExternalLinkage, "read_string", TheModule);
  }
  return readStrFunc;
}

// --- Helper Passes for IR Generation ---
// Generate IR for all function definitions.
void generateFunctions(ASTNode* node) {
  if (!node) return;
  if (strcmp(node->type, "GLOBAL_LIST") == 0) {
    generateFunctions(node->left);
    generateFunctions(node->right);
  } else if (strcmp(node->type, "FUNC_DEF") == 0) {
    generateIR(node, nullptr);
  }
}

// Generate IR for all non-function (global) statements into main().
void generateGlobalStatements(ASTNode* node, Function* mainFunc) {
  if (!node) return;
  if (strcmp(node->type, "GLOBAL_LIST") == 0) {
    generateGlobalStatements(node->left, mainFunc);
    generateGlobalStatements(node->right, mainFunc);
  } else if (strcmp(node->type, "FUNC_DEF") == 0) {
    return;
  } else {
    generateIR(node, mainFunc);
  }
}

// --- Main IR Generation Function ---
Value *generateIR(ASTNode *node, Function* currentFunction) {
  if (!node) return nullptr;
  
  // --- Literals ---
  if (strcmp(node->type, "NUMBER") == 0)
    return ConstantInt::get(Type::getInt32Ty(Context), atoi(node->value));
  
  if (strcmp(node->type, "FLOAT") == 0)
    return ConstantFP::get(Type::getFloatTy(Context), strtof(node->value, nullptr));
  
  if (strcmp(node->type, "BOOLEAN") == 0)
    return ConstantInt::get(Type::getInt1Ty(Context), (strcmp(node->value, "true") == 0) ? 1 : 0);
  
  if (strcmp(node->type, "CHAR") == 0) {
    if (strlen(node->value) < 3) {
      std::cerr << "Invalid char literal: " << node->value << "\n";
      return ConstantInt::get(Type::getInt8Ty(Context), 0);
    }
    return ConstantInt::get(Type::getInt8Ty(Context), node->value[1]);
  }
  
  if (strcmp(node->type, "STRING") == 0) {
    std::string strLiteral(node->value);
    if (!strLiteral.empty() && strLiteral.front() == '"' && strLiteral.back() == '"')
      strLiteral = strLiteral.substr(1, strLiteral.size() - 2);
    return Builder.CreateGlobalStringPtr(strLiteral, "strlit");
  }
  
  // --- Handle DEFAULT node ---
  if (strcmp(node->type, "DEFAULT") == 0) {
    return generateIR(node->left, currentFunction);
  }
  
  // --- BREAK Statement ---
  if (strcmp(node->type, "BREAK") == 0) {
    if (SwitchMergeStack.empty())
      report_fatal_error("Break statement not within switch-case");
    BasicBlock *mergeBB = SwitchMergeStack.back();
    Builder.CreateBr(mergeBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // --- FOR_LOOP --- (merged version)
  if (strcmp(node->type, "FOR_LOOP") == 0) {
    Value *startVal, *endVal;
    ASTNode *rangeNode = node->left;
    if (node->value != NULL) {
      Value *existing = NamedValues[node->value];
      if (existing)
        startVal = Builder.CreateLoad(Type::getInt32Ty(Context), existing, node->value);
      else
        startVal = ConstantInt::get(Type::getInt32Ty(Context), 1);
      endVal = generateIR(rangeNode->right, currentFunction);
    } else {
      startVal = generateIR(rangeNode->left, currentFunction);
      endVal = generateIR(rangeNode->right, currentFunction);
    }
    if (!startVal->getType()->isIntegerTy(32))
      startVal = Builder.CreateIntCast(startVal, Type::getInt32Ty(Context), true, "start_cast");
    if (!endVal->getType()->isIntegerTy(32)) {
      if (endVal->getType()->isArrayTy()) {
        ArrayType *arrTy = dyn_cast<ArrayType>(endVal->getType());
        unsigned len = arrTy->getNumElements();
        endVal = ConstantInt::get(Type::getInt32Ty(Context), len);
      } else {
        endVal = Builder.CreateIntCast(endVal, Type::getInt32Ty(Context), true, "end_cast");
      }
    }
    
    AllocaInst *forVar = nullptr;
    if (node->value != NULL) {
      Value *existing = NamedValues[node->value];
      if (existing) {
        forVar = dyn_cast<AllocaInst>(existing);
        if (!forVar) {
          forVar = CreateEntryBlockAlloca(currentFunction, node->value, Type::getInt32Ty(Context));
          NamedValues[node->value] = forVar;
        }
      } else {
        forVar = CreateEntryBlockAlloca(currentFunction, node->value, Type::getInt32Ty(Context));
        NamedValues[node->value] = forVar;
      }
    } else {
      forVar = CreateEntryBlockAlloca(currentFunction, "for_iter", Type::getInt32Ty(Context));
    }
    Builder.CreateStore(startVal, forVar);
    
    BasicBlock *condBB = BasicBlock::Create(Context, "for.cond", currentFunction);
    BasicBlock *loopBB = BasicBlock::Create(Context, "for.body", currentFunction);
    BasicBlock *afterBB = BasicBlock::Create(Context, "for.end", currentFunction);
    
    Builder.CreateBr(condBB);
    Builder.SetInsertPoint(condBB);
    Value *currVal = Builder.CreateLoad(Type::getInt32Ty(Context), forVar, (node->value ? node->value : "for_iter"));
    Value *cond = Builder.CreateICmpSLE(currVal, endVal, "forcond");
    Builder.CreateCondBr(cond, loopBB, afterBB);
    
    Builder.SetInsertPoint(loopBB);
    generateIR(node->right, currentFunction);
    currVal = Builder.CreateLoad(Type::getInt32Ty(Context), forVar, (node->value ? node->value : "for_iter"));
    Value *nextVal = Builder.CreateAdd(currVal, ConstantInt::get(Type::getInt32Ty(Context), 1), "forinc");
    Builder.CreateStore(nextVal, forVar);
    Builder.CreateBr(condBB);
    
    Builder.SetInsertPoint(afterBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // --- ARRAY_ITERATOR --- (New support for array iteration)
  if (strcmp(node->type, "ARRAY_ITERATOR") == 0) {
    std::string loopVarName = node->value;
    std::string arrayName = node->left->value;
    Value *arrPtr = NamedValues[arrayName];
    if (!arrPtr) {
      report_fatal_error(Twine("Error: Undefined array '") + arrayName + "'");
    }
    AllocaInst *arrAlloca = dyn_cast<AllocaInst>(arrPtr);
    if (!arrAlloca) {
      report_fatal_error("Error: Array variable is not an alloca in ARRAY_ITERATOR");
    }
    ArrayType *arrType = dyn_cast<ArrayType>(arrAlloca->getAllocatedType());
    if (!arrType) {
      report_fatal_error("Error: Variable is not an array type in ARRAY_ITERATOR");
    }
    unsigned arraySize = arrType->getNumElements();
    
    Function *curFunc = currentFunction;
    AllocaInst *indexAlloca = CreateEntryBlockAlloca(curFunc, "array_iter_index", Type::getInt32Ty(Context));
    Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(Context), 0), indexAlloca);
    
    BasicBlock *condBB = BasicBlock::Create(Context, "array_iter.cond", curFunc);
    BasicBlock *bodyBB = BasicBlock::Create(Context, "array_iter.body", curFunc);
    BasicBlock *afterBB = BasicBlock::Create(Context, "array_iter.after", curFunc);
    
    Builder.CreateBr(condBB);
    Builder.SetInsertPoint(condBB);
    Value *curIndex = Builder.CreateLoad(Type::getInt32Ty(Context), indexAlloca, "cur_index");
    Value *iterCond = Builder.CreateICmpSLT(curIndex, ConstantInt::get(Type::getInt32Ty(Context), arraySize), "array_iter_cond");
    Builder.CreateCondBr(iterCond, bodyBB, afterBB);
    
    Builder.SetInsertPoint(bodyBB);
    std::vector<Value*> indices;
    indices.push_back(ConstantInt::get(Type::getInt32Ty(Context), 0));
    indices.push_back(curIndex);
    Value *elemPtr = Builder.CreateGEP(arrType, arrAlloca, indices, "array_elem_ptr");
    Value *elemVal = Builder.CreateLoad(arrType->getElementType(), elemPtr, "array_elem");
    
    Value *loopVarAlloca = NamedValues[loopVarName];
    if (!loopVarAlloca) {
      loopVarAlloca = CreateEntryBlockAlloca(curFunc, loopVarName, elemVal->getType());
      NamedValues[loopVarName] = loopVarAlloca;
    }
    Builder.CreateStore(elemVal, loopVarAlloca);
    
    // Generate the loop body (which typically calls inline(i) for each element).
    generateIR(node->right, curFunc);
    
    // Removed automatic newline insertion here so that inline(i) prints consecutively.
    
    curIndex = Builder.CreateLoad(Type::getInt32Ty(Context), indexAlloca, "cur_index");
    Value *nextIndex = Builder.CreateAdd(curIndex, ConstantInt::get(Type::getInt32Ty(Context), 1), "next_index");
    Builder.CreateStore(nextIndex, indexAlloca);
    Builder.CreateBr(condBB);
    
    Builder.SetInsertPoint(afterBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // --- Identifier lookup ---
  if (strcmp(node->type, "IDENTIFIER") == 0) {
    Value* varPtr = NamedValues[node->value];
    if (!varPtr) {
      report_fatal_error(Twine("Error: Unknown variable '") + node->value + "'");
    }
    if (AllocaInst *alloca = dyn_cast<AllocaInst>(varPtr))
      return Builder.CreateLoad(alloca->getAllocatedType(), varPtr, node->value);
    else {
      PointerType *ptrType = dyn_cast<PointerType>(varPtr->getType());
      if (!ptrType || ptrType->getNumContainedTypes() < 1)
        report_fatal_error(Twine("Error: Variable ") + node->value + " is not a proper pointer type!");
      return Builder.CreateLoad(ptrType->getContainedType(0), varPtr, node->value);
    }
  }
  
  // --- Unary minus ---
  if (strcmp(node->type, "NEG") == 0) {
    Value *val = generateIR(node->left, currentFunction);
    if (val->getType()->isFloatTy())
      return Builder.CreateFNeg(val, "fnegtmp");
    else
      return Builder.CreateNeg(val, "negtmp");
  }
  
  // --- Binary Arithmetic Operations ---
  if (strcmp(node->type, "ADD") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType() == PointerType::get(Type::getInt8Ty(Context), 0) &&
        R->getType() == PointerType::get(Type::getInt8Ty(Context), 0)) {
      Function *concatFunc = getConcatFunction();
      return Builder.CreateCall(concatFunc, {L, R}, "concat");
    }
    if (L->getType()->isFloatTy() && R->getType()->isIntegerTy())
      R = Builder.CreateSIToFP(R, Type::getFloatTy(Context), "intToFloat");
    else if (L->getType()->isIntegerTy() && R->getType()->isFloatTy())
      L = Builder.CreateSIToFP(L, Type::getFloatTy(Context), "intToFloat");
      
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
  
  // --- Relational Operators ---
  if (strcmp(node->type, "LT") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFCmpOLT(L, R, "cmptmp");
    else
      return Builder.CreateICmpSLT(L, R, "cmptmp");
  }
  
  if (strcmp(node->type, "GT") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFCmpOGT(L, R, "cmptmp");
    else
      return Builder.CreateICmpSGT(L, R, "cmptmp");
  }
  
  if (strcmp(node->type, "LE") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFCmpOLE(L, R, "cmptmp");
    else
      return Builder.CreateICmpSLE(L, R, "cmptmp");
  }
  
  if (strcmp(node->type, "GE") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFCmpOGE(L, R, "cmptmp");
    else
      return Builder.CreateICmpSGE(L, R, "cmptmp");
  }
  
  if (strcmp(node->type, "EQ") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFCmpOEQ(L, R, "eqtmp");
    else
      return Builder.CreateICmpEQ(L, R, "eqtmp");
  }
  
  // --- Logical Operators ---
  if (strcmp(node->type, "AND") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    return Builder.CreateAnd(L, R, "andtmp");
  }
  
  if (strcmp(node->type, "OR") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    return Builder.CreateOr(L, R, "ortmp");
  }
  
  // --- Not Equals ---
  if (strcmp(node->type, "NE") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFCmpONE(L, R, "netmp");
    else
      return Builder.CreateICmpNE(L, R, "netmp");
  }
  
  // --- Unary NOT ---
  if (strcmp(node->type, "NOT") == 0) {
    Value *val = generateIR(node->left, currentFunction);
    if (val->getType()->isIntegerTy() && val->getType()->getIntegerBitWidth() == 1)
      return Builder.CreateNot(val, "nottmp");
    return Builder.CreateICmpEQ(val, ConstantInt::get(val->getType(), 0), "nottmp");
  }
  
  // --- Assignments ---
  if (strcmp(node->type, "ASSIGN_INT") == 0 ||
      strcmp(node->type, "ASSIGN_FLOAT") == 0 ||
      strcmp(node->type, "ASSIGN_BOOL") == 0 ||
      strcmp(node->type, "ASSIGN_CHAR") == 0 ||
      strcmp(node->type, "ASSIGN_STRING") == 0) {
    std::string varName = node->value;
    Value *exprVal = generateIR(node->left, currentFunction);
    Value *varPtr = NamedValues[varName];
    if (!varPtr) {
      if (strcmp(node->type, "ASSIGN_INT") == 0)
        varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt32Ty(Context));
      else if (strcmp(node->type, "ASSIGN_FLOAT") == 0)
        varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getFloatTy(Context));
      else if (strcmp(node->type, "ASSIGN_BOOL") == 0)
        varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt1Ty(Context));
      else if (strcmp(node->type, "ASSIGN_CHAR") == 0)
        varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt8Ty(Context));
      else if (strcmp(node->type, "ASSIGN_STRING") == 0)
        varPtr = CreateEntryBlockAlloca(currentFunction, varName, PointerType::get(Type::getInt8Ty(Context), 0));
      NamedValues[varName] = varPtr;
    }
    if (strcmp(node->type, "ASSIGN_FLOAT") == 0) {
      if (exprVal->getType()->isIntegerTy())
        exprVal = Builder.CreateSIToFP(exprVal, Type::getFloatTy(Context), "intToFloat");
    }
    if (strcmp(node->type, "ASSIGN_STRING") == 0) {
      Value *strVal = Builder.CreateBitCast(exprVal, PointerType::get(Type::getInt8Ty(Context), 0), "strcast");
      Builder.CreateStore(strVal, varPtr);
      return strVal;
    }
    Builder.CreateStore(exprVal, varPtr);
    return exprVal;
  }
  
  if (strcmp(node->type, "REASSIGN") == 0) {
    std::string varName = node->value;
    Value *varPtr = NamedValues[varName];
    if (!varPtr) {
      report_fatal_error(Twine("Error: Undeclared variable '") + varName + "'");
    }
    Value *exprVal = generateIR(node->left, currentFunction);
    if (AllocaInst *AI = dyn_cast<AllocaInst>(varPtr)) {
      if (AI->getAllocatedType()->isFloatTy() && exprVal->getType()->isIntegerTy())
        exprVal = Builder.CreateSIToFP(exprVal, Type::getFloatTy(Context), "intToFloat");
    } else if (PointerType *PT = dyn_cast<PointerType>(varPtr->getType())) {
      if (PT->getContainedType(0)->isFloatTy() && exprVal->getType()->isIntegerTy())
        exprVal = Builder.CreateSIToFP(exprVal, Type::getFloatTy(Context), "intToFloat");
    }
    Builder.CreateStore(exprVal, varPtr);
    return exprVal;
  }
  
  // --- PRINT ---
  if (strcmp(node->type, "PRINT") == 0) {
    Value *exprVal = generateIR(node->left, currentFunction);
    Type *exprType = exprVal->getType();
    GlobalVariable *fmtStrVar = nullptr;
    if (exprType->isIntegerTy(1)) {
      GlobalVariable *trueStr = TheModule->getNamedGlobal(".str_true");
      if (!trueStr) {
        Constant *tstr = ConstantDataArray::getString(Context, "true", true);
        trueStr = new GlobalVariable(*TheModule, tstr->getType(), true,
                                   GlobalValue::PrivateLinkage, tstr, ".str_true");
      }
      GlobalVariable *falseStr = TheModule->getNamedGlobal(".str_false");
      if (!falseStr) {
        Constant *fstr = ConstantDataArray::getString(Context, "false", true);
        falseStr = new GlobalVariable(*TheModule, fstr->getType(), true,
                                    GlobalValue::PrivateLinkage, fstr, ".str_false");
      }
      Constant *zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
      std::vector<Constant*> indices = {zero, zero};
      Constant *trueStrPtr = ConstantExpr::getGetElementPtr(trueStr->getValueType(), trueStr, indices);
      Constant *falseStrPtr = ConstantExpr::getGetElementPtr(falseStr->getValueType(), falseStr, indices);
      Value *boolStr = Builder.CreateSelect(exprVal, trueStrPtr, falseStrPtr, "boolstr");
      GlobalVariable *fmtStrVarBool = TheModule->getNamedGlobal(".str_bool");
      if (!fmtStrVarBool) {
        Constant *formatStr = ConstantDataArray::getString(Context, "%s\n", true);
        fmtStrVarBool = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                           GlobalValue::PrivateLinkage, formatStr, ".str_bool");
      }
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
  
  // --- PRINT_NEWLINE ---
  if (strcmp(node->type, "PRINT_NEWLINE") == 0) {
    Constant *newlineStr = ConstantDataArray::getString(Context, "\n", true);
    GlobalVariable *nlVar = TheModule->getNamedGlobal(".str_newline");
    if (!nlVar) {
      nlVar = new GlobalVariable(*TheModule, newlineStr->getType(), true,
                                 GlobalValue::PrivateLinkage, newlineStr, ".str_newline");
    }
    Constant *zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
    std::vector<Constant*> nlIndices = {zero, zero};
    Constant *nlPtr = ConstantExpr::getGetElementPtr(nlVar->getValueType(), nlVar, nlIndices);
    Builder.CreateCall(getPrintfFunction(), {nlPtr});
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // --- INPUT ---
  if (strcmp(node->type, "INPUT") == 0) {
    std::string varName = node->value;
    Value *varPtr = NamedValues[varName];
    if (!varPtr) {
      varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt32Ty(Context));
      NamedValues[varName] = varPtr;
    }
    if (AllocaInst *allocaInst = dyn_cast<AllocaInst>(varPtr)) {
      Type *allocatedType = allocaInst->getAllocatedType();
      Value *inputVal = nullptr;
      if (allocatedType == Type::getInt32Ty(Context)) {
        inputVal = Builder.CreateCall(getReadIntFunction(), {}, "readInt");
      } else if (allocatedType == Type::getFloatTy(Context)) {
        inputVal = Builder.CreateCall(getReadFloatFunction(), {}, "readFloat");
      } else if (allocatedType == Type::getInt1Ty(Context)) {
        inputVal = Builder.CreateCall(getReadBoolFunction(), {}, "readBool");
      } else if (allocatedType == Type::getInt8Ty(Context)) {
        inputVal = Builder.CreateCall(getReadCharFunction(), {}, "readChar");
      } else if (allocatedType->isPointerTy() &&
                 allocatedType == PointerType::get(Type::getInt8Ty(Context), 0)) {
        inputVal = Builder.CreateCall(getReadStringFunction(), {}, "readStr");
      } else {
        GlobalVariable *fmtStrVar = getFormatStringStr();
        Constant *errMsg = Builder.CreateGlobalStringPtr("Input Is not of the expected type\n", "errMsg");
        Constant *zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
        std::vector<Constant*> indices = {zero, zero};
        Constant *fmtStrPtr = ConstantExpr::getGetElementPtr(fmtStrVar->getValueType(), fmtStrVar, indices);
        Builder.CreateCall(getPrintfFunction(), {fmtStrPtr, errMsg});
        inputVal = ConstantInt::get(Type::getInt32Ty(Context), 0);
      }
      Builder.CreateStore(inputVal, varPtr);
      return inputVal;
    }
  }
  
  // --- LOOP ---
  if (strcmp(node->type, "LOOP") == 0) {
    Value *loopCountVal = generateIR(node->left, currentFunction);
    if (!loopCountVal) {
      std::cerr << "Error: Invalid loop count expression\n";
      return nullptr;
    }
    if (loopCountVal->getType() != Type::getInt32Ty(Context))
      loopCountVal = Builder.CreateIntCast(loopCountVal, Type::getInt32Ty(Context), true, "loopcount");
      
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
    generateIR(node->right, currentFunction);
    Value *nextVal = Builder.CreateAdd(currVal, ConstantInt::get(Type::getInt32Ty(Context), 1), "inc");
    Builder.CreateStore(nextVal, loopVar);
    Builder.CreateBr(loopCondBB);
    Builder.SetInsertPoint(afterLoopBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // --- LOOP_UNTIL ---
  if (strcmp(node->type, "LOOP_UNTIL") == 0) {
    BasicBlock *condBB = BasicBlock::Create(Context, "until.cond", currentFunction);
    BasicBlock *loopBB = BasicBlock::Create(Context, "until.body", currentFunction);
    BasicBlock *afterBB = BasicBlock::Create(Context, "until.after", currentFunction);
    Builder.CreateBr(condBB);
    Builder.SetInsertPoint(condBB);
    Value *condVal = generateIR(node->left, currentFunction);
    if (condVal->getType()->isIntegerTy() && condVal->getType()->getIntegerBitWidth() != 1)
      condVal = Builder.CreateICmpNE(condVal, ConstantInt::get(condVal->getType(), 0), "untilcond");
    Value *notCond = Builder.CreateNot(condVal, "untilnot");
    Builder.CreateCondBr(notCond, loopBB, afterBB);
    Builder.SetInsertPoint(loopBB);
    generateIR(node->right, currentFunction);
    Builder.CreateBr(condBB);
    Builder.SetInsertPoint(afterBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // --- SWITCH-CASE ---
  if (strcmp(node->type, "SWITCH") == 0) {
    Value *switchVal = generateIR(node->left, currentFunction);
    if (!switchVal->getType()->isIntegerTy(32))
      switchVal = Builder.CreateIntCast(switchVal, Type::getInt32Ty(Context), true, "switchcond");
    
    ASTNode *switchBody = node->right;
    ASTNode *caseList = switchBody ? switchBody->left : nullptr;
    ASTNode *defaultClause = switchBody ? switchBody->right : nullptr;
    
    BasicBlock *curBB = Builder.GetInsertBlock();
    BasicBlock *mergeBB = BasicBlock::Create(Context, "switch.merge", currentFunction);
    BasicBlock *defaultBB = BasicBlock::Create(Context, "switch.default", currentFunction);
    
    SwitchMergeStack.push_back(mergeBB);
    
    Builder.SetInsertPoint(curBB);
    SwitchInst *switchInst = Builder.CreateSwitch(switchVal, defaultBB, 0);
    
    std::vector<ASTNode*> caseNodes;
    std::function<void(ASTNode*)> collectCases = [&](ASTNode *n) {
         if (!n) return;
         if (strcmp(n->type, "CASE") == 0)
             caseNodes.push_back(n);
         else if (strcmp(n->type, "CASE_LIST") == 0) {
             collectCases(n->left);
             collectCases(n->right);
         }
    };
    collectCases(caseList);
    
    for (ASTNode *caseNode : caseNodes) {
         Value *caseLiteral = generateIR(caseNode->left, currentFunction);
         ConstantInt *caseConst = dyn_cast<ConstantInt>(caseLiteral);
         if (!caseConst) {
             std::cerr << "Non constant case literal in switch\n";
             continue;
         }
         BasicBlock *caseBB = BasicBlock::Create(Context, "case", currentFunction);
         switchInst->addCase(caseConst, caseBB);
         Builder.SetInsertPoint(caseBB);
         generateIR(caseNode->right, currentFunction);
         if (!Builder.GetInsertBlock()->getTerminator())
           Builder.CreateBr(mergeBB);
    }
    
    Builder.SetInsertPoint(defaultBB);
    if (defaultClause)
       generateIR(defaultClause, currentFunction);
    if (!Builder.GetInsertBlock()->getTerminator())
       Builder.CreateBr(mergeBB);
    
    SwitchMergeStack.pop_back();
    
    Builder.SetInsertPoint(mergeBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // --- STATEMENT_LIST ---
  if (strcmp(node->type, "STATEMENT_LIST") == 0) {
    if (node->left)
      generateIR(node->left, currentFunction);
    if (node->right)
      generateIR(node->right, currentFunction);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // --- VAR_DECL ---
  if (strcmp(node->type, "VAR_DECL") == 0) {
    std::string varName = node->value;
    if (NamedValues.find(varName) != NamedValues.end()) {
      std::cerr << "Variable " << varName << " already declared!" << std::endl;
      return nullptr;
    }
    Value *exprVal = generateIR(node->left, currentFunction);
    if (!exprVal) {
      std::cerr << "Error evaluating expression for var " << varName << std::endl;
      return nullptr;
    }
    Type *varType = exprVal->getType();
    Value *varPtr = CreateEntryBlockAlloca(currentFunction, varName, varType);
    NamedValues[varName] = varPtr;
    Builder.CreateStore(exprVal, varPtr);
    return exprVal;
  }
  
  // --- Array Declarations ---
  if (strcmp(node->type, "DECL_ARRAY") == 0) {
    std::string varName = node->value;
    Value *sizeVal = generateIR(node->left, currentFunction);
    if (!sizeVal)
      report_fatal_error("Invalid array size expression");
    if (!sizeVal->getType()->isIntegerTy(32))
      sizeVal = Builder.CreateIntCast(sizeVal, Type::getInt32Ty(Context), true, "arraysize");
    if (ConstantInt *CI = dyn_cast<ConstantInt>(sizeVal)) {
      unsigned arraySize = CI->getZExtValue();
      ArrayType *arrType = ArrayType::get(Type::getInt32Ty(Context), arraySize);
      AllocaInst *varPtr = Builder.CreateAlloca(arrType, nullptr, varName);
      NamedValues[varName] = varPtr;
      return varPtr;
    } else {
      report_fatal_error("Only constant array sizes are supported in DECL_ARRAY");
    }
  }
  
  if (strcmp(node->type, "DECL_ARRAY_INIT") == 0) {
    std::string varName = node->value;
    int count = 0;
    std::function<void(ASTNode*)> countElements = [&](ASTNode* n) {
         if (!n) return;
         if (strcmp(n->type, "ARRAY_ELEM_LIST") == 0) {
             countElements(n->left);
             countElements(n->right);
         } else {
             count++;
         }
    };
    countElements(node->left);
    Value *countVal = ConstantInt::get(Type::getInt32Ty(Context), count);
    AllocaInst *varPtr = Builder.CreateAlloca(ArrayType::get(Type::getInt32Ty(Context), count), nullptr, varName);
    NamedValues[varName] = varPtr;
    int index = 0;
    std::function<void(ASTNode*)> storeElements = [&](ASTNode* n) {
         if (!n) return;
         if (strcmp(n->type, "ARRAY_ELEM_LIST") == 0) {
             storeElements(n->left);
             storeElements(n->right);
         } else {
             Value *elemVal = generateIR(n, currentFunction);
             std::vector<Value*> indices;
             indices.push_back(ConstantInt::get(Type::getInt32Ty(Context), 0));
             indices.push_back(ConstantInt::get(Type::getInt32Ty(Context), index));
             AllocaInst *AI = dyn_cast<AllocaInst>(varPtr);
             if (!AI)
               report_fatal_error("Array variable is not an alloca!");
             Value *elemPtr = Builder.CreateGEP(AI->getAllocatedType(), varPtr, indices, "arrayelem");
             Builder.CreateStore(elemVal, elemPtr);
             index++;
         }
    };
    storeElements(node->left);
    return varPtr;
  }
  
  // --- Array Assignment ---
  if (strcmp(node->type, "ARRAY_ASSIGN") == 0) {
    std::string varName = node->value;
    Value *varPtr = NamedValues[varName];
    if (!varPtr) {
      report_fatal_error(Twine("Error: Undeclared array '") + varName + "'");
    }
    Value *indexVal = generateIR(node->left, currentFunction);
    indexVal = Builder.CreateSub(indexVal, ConstantInt::get(Type::getInt32Ty(Context), 1), "arrayindex");
    std::vector<Value*> indices;
    indices.push_back(ConstantInt::get(Type::getInt32Ty(Context), 0));
    indices.push_back(indexVal);
    AllocaInst *AI = dyn_cast<AllocaInst>(varPtr);
    if (!AI)
      report_fatal_error("Array variable is not an alloca!");
    Value *elemPtr = Builder.CreateGEP(AI->getAllocatedType(), varPtr, indices, "arrayelem");
    Value *newVal = generateIR(node->right, currentFunction);
    Builder.CreateStore(newVal, elemPtr);
    return newVal;
  }
  
  // --- Array Access ---
  if (strcmp(node->type, "ARRAY_ACCESS") == 0) {
    std::string varName = node->value;
    Value *varPtr = NamedValues[varName];
    if (!varPtr)
      report_fatal_error(Twine("Error: Undeclared array '") + varName + "'");
    Value *indexVal = generateIR(node->left, currentFunction);
    indexVal = Builder.CreateSub(indexVal, ConstantInt::get(Type::getInt32Ty(Context), 1), "arrayindex");
    std::vector<Value*> indices;
    indices.push_back(ConstantInt::get(Type::getInt32Ty(Context), 0));
    indices.push_back(indexVal);
    AllocaInst *AI = dyn_cast<AllocaInst>(varPtr);
    if (!AI)
      report_fatal_error("Array variable is not an alloca!");
    return Builder.CreateLoad(AI->getAllocatedType()->getArrayElementType(), 
                              Builder.CreateGEP(AI->getAllocatedType(), varPtr, indices, "arrayelem"),
                              varName.c_str());
  }
  
  // --- TYPE operator ---
  if (strcmp(node->type, "TYPE") == 0) {
    Type *targetType = nullptr;
    if (strcmp(node->left->type, "IDENTIFIER") == 0) {
      Value *varPtr = NamedValues[node->left->value];
      if (!varPtr) {
        std::cerr << "Unknown variable in type(): " << node->left->value << std::endl;
        return Builder.CreateGlobalStringPtr("unknown", "type_unknown");
      }
      if (AllocaInst *alloca = dyn_cast<AllocaInst>(varPtr))
        targetType = alloca->getAllocatedType();
      else
        targetType = varPtr->getType()->getContainedType(0);
    } else {
      Value *temp = generateIR(node->left, currentFunction);
      targetType = temp->getType();
    }
      
    const char *typeName = "unknown";
    if (targetType->isIntegerTy(32))
      typeName = "int";
    else if (targetType->isFloatTy())
      typeName = "float";
    else if (targetType->isIntegerTy(1))
      typeName = "bool";
    else if (targetType->isIntegerTy(8))
      typeName = "char";
    else if (targetType->isPointerTy() &&
             targetType == PointerType::get(Type::getInt8Ty(Context), 0))
      typeName = "string";
      
    Value *typeStr = Builder.CreateGlobalStringPtr(typeName, "typeStr");
    return typeStr;
  }
  
  // --- Specific Declarations ---
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
    Value* varPtr = CreateEntryBlockAlloca(currentFunction, varName, PointerType::get(Type::getInt8Ty(Context), 0));
    NamedValues[varName] = varPtr;
    Builder.CreateStore(ConstantPointerNull::get(PointerType::get(Type::getInt8Ty(Context), 0)), varPtr);
    return varPtr;
  }
  
  // --- Conditional Constructs ---
  if (strcmp(node->type, "IF") == 0) {
    Value *condVal = generateIR(node->left, currentFunction);
    if (!condVal) {
      std::cerr << "Error: Invalid condition in if statement\n";
      return nullptr;
    }
    if (condVal->getType()->isIntegerTy() && condVal->getType()->getIntegerBitWidth() != 1)
      condVal = Builder.CreateICmpNE(condVal, ConstantInt::get(condVal->getType(), 0), "ifcond");
    BasicBlock *thenBB = BasicBlock::Create(Context, "then", currentFunction);
    BasicBlock *mergeBB = BasicBlock::Create(Context, "ifcont", currentFunction);
    Builder.CreateCondBr(condVal, thenBB, mergeBB);
    Builder.SetInsertPoint(thenBB);
    generateIR(node->right, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(mergeBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  if (strcmp(node->type, "IF_ELSE") == 0) {
    Value *condVal = generateIR(node->left, currentFunction);
    if (!condVal) {
      std::cerr << "Error: Invalid condition in if-else statement\n";
      return nullptr;
    }
    if (condVal->getType()->isIntegerTy() && condVal->getType()->getIntegerBitWidth() != 1)
      condVal = Builder.CreateICmpNE(condVal, ConstantInt::get(condVal->getType(), 0), "ifelsecond");
    BasicBlock *thenBB = BasicBlock::Create(Context, "then", currentFunction);
    BasicBlock *elseBB = BasicBlock::Create(Context, "else", currentFunction);
    BasicBlock *mergeBB = BasicBlock::Create(Context, "ifelsecont", currentFunction);
    Builder.CreateCondBr(condVal, thenBB, elseBB);
    Builder.SetInsertPoint(thenBB);
    generateIR(node->right->left, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(elseBB);
    generateIR(node->right->right, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(mergeBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  if (strcmp(node->type, "IF_CHAIN") == 0) {
    Value *condVal = generateIR(node->left->left, currentFunction);
    if (!condVal) {
      std::cerr << "Error: Invalid condition in if-chain\n";
      return nullptr;
    }
    if (condVal->getType()->isIntegerTy() && condVal->getType()->getIntegerBitWidth() != 1)
      condVal = Builder.CreateICmpNE(condVal, ConstantInt::get(condVal->getType(), 0), "ifchaincond");
    BasicBlock *thenBB = BasicBlock::Create(Context, "if.chain.then", currentFunction);
    BasicBlock *elseBB = BasicBlock::Create(Context, "if.chain.else", currentFunction);
    BasicBlock *mergeBB = BasicBlock::Create(Context, "if.chain.merge", currentFunction);
    Builder.CreateCondBr(condVal, thenBB, elseBB);
    Builder.SetInsertPoint(thenBB);
    generateIR(node->left->right, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(elseBB);
    generateIR(node->right, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(mergeBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  if (strcmp(node->type, "ELSE_IF") == 0) {
    Value *condVal = generateIR(node->left, currentFunction);
    if (!condVal) {
      std::cerr << "Error: Invalid condition in else-if statement\n";
      return nullptr;
    }
    if (condVal->getType()->isIntegerTy() && condVal->getType()->getIntegerBitWidth() != 1)
      condVal = Builder.CreateICmpNE(condVal, ConstantInt::get(condVal->getType(), 0), "elseifcond");
    BasicBlock *thenBB = BasicBlock::Create(Context, "elseif.then", currentFunction);
    BasicBlock *elseBB = BasicBlock::Create(Context, "elseif.else", currentFunction);
    BasicBlock *mergeBB = BasicBlock::Create(Context, "elseif.merge", currentFunction);
    Builder.CreateCondBr(condVal, thenBB, elseBB);
    Builder.SetInsertPoint(thenBB);
    generateIR(node->right->left, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(elseBB);
    if (node->right->right)
      generateIR(node->right->right, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(mergeBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  if (strcmp(node->type, "ELSE") == 0) {
    generateIR(node->left, currentFunction);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  if (strcmp(node->type, "IF_ELSE_BODY") == 0) {
    if (node->left)
      generateIR(node->left, currentFunction);
    if (node->right)
      generateIR(node->right, currentFunction);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // --- Function Definition ---
  if (strcmp(node->type, "FUNC_DEF") == 0) {
    std::string funcName = node->value;
    std::vector<Type*> paramTypes;
    std::vector<std::string> paramNames;
    extractParams(node->left, paramTypes, paramNames);
    FunctionType *funcType = FunctionType::get(Type::getInt32Ty(Context), paramTypes, false);
    Function *func = Function::Create(funcType, Function::ExternalLinkage, funcName, TheModule);
    unsigned idx = 0;
    for (auto &arg : func->args()) {
      arg.setName(paramNames[idx++]);
    }
    BasicBlock *BB = BasicBlock::Create(Context, "entry", func);
    std::map<std::string, Value*> oldNamedValues = NamedValues;
    NamedValues.clear();
    Builder.SetInsertPoint(BB);
    for (auto &arg : func->args()) {
      AllocaInst *alloca = CreateEntryBlockAlloca(func, std::string(arg.getName()), arg.getType());
      Builder.CreateStore(&arg, alloca);
      NamedValues[std::string(arg.getName())] = alloca;
    }
    generateIR(node->right, func);
    if (!BB->getTerminator())
      Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(Context), 0));
    NamedValues = oldNamedValues;
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // --- Return Statement ---
  if (strcmp(node->type, "RETURN") == 0) {
    Value *retVal = generateIR(node->left, currentFunction);
    Builder.CreateRet(retVal);
    return retVal;
  }
  
  // --- Function Call ---
  if (strcmp(node->type, "CALL") == 0) {
    Function *callee = TheModule->getFunction(node->value);
    if (!callee) {
      report_fatal_error("Unknown function referenced");
    }
    std::vector<Value*> argsV;
    extractArgs(node->left, argsV, currentFunction);
    return Builder.CreateCall(callee, argsV, "calltmp");
  }
  
  // --- INLINE ---
  if (strcmp(node->type, "INLINE") == 0) {
    Value *exprVal = generateIR(node->left, currentFunction);
    Type *exprType = exprVal->getType();
    GlobalVariable *fmtStrVar = nullptr;
    if (exprType->isIntegerTy(1)) {
      GlobalVariable *trueStr = TheModule->getNamedGlobal(".str_true");
      if (!trueStr) {
        Constant *tstr = ConstantDataArray::getString(Context, "true", true);
        trueStr = new GlobalVariable(*TheModule, tstr->getType(), true,
                                   GlobalValue::PrivateLinkage, tstr, ".str_true");
      }
      GlobalVariable *falseStr = TheModule->getNamedGlobal(".str_false");
      if (!falseStr) {
        Constant *fstr = ConstantDataArray::getString(Context, "false", true);
        falseStr = new GlobalVariable(*TheModule, fstr->getType(), true,
                                    GlobalValue::PrivateLinkage, fstr, ".str_false");
      }
      Constant *zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
      std::vector<Constant*> indices = {zero, zero};
      Constant *trueStrPtr = ConstantExpr::getGetElementPtr(trueStr->getValueType(), trueStr, indices);
      Constant *falseStrPtr = ConstantExpr::getGetElementPtr(falseStr->getValueType(), falseStr, indices);
      Value *boolStr = Builder.CreateSelect(exprVal, trueStrPtr, falseStrPtr, "boolstr");
      GlobalVariable *fmtStrVarBool = TheModule->getNamedGlobal(".str_bool");
      if (!fmtStrVarBool) {
        Constant *formatStr = ConstantDataArray::getString(Context, "%s", true);
        fmtStrVarBool = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                           GlobalValue::PrivateLinkage, formatStr, ".str_bool");
      }
      fmtStrVar = fmtStrVarBool;
      exprVal = boolStr;
    } else if (exprType->isFloatTy()) {
      Constant *formatStr = ConstantDataArray::getString(Context, "%.1f", true);
      GlobalVariable *fmt = TheModule->getNamedGlobal(".str_inline_float");
      if (!fmt) {
        fmt = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                 GlobalValue::PrivateLinkage, formatStr, ".str_inline_float");
      }
      fmtStrVar = fmt;
      exprVal = Builder.CreateFPExt(exprVal, Type::getDoubleTy(Context), "toDouble");
    } else if (exprType->isIntegerTy(8)) {
      Constant *formatStr = ConstantDataArray::getString(Context, "%c", true);
      GlobalVariable *fmt = TheModule->getNamedGlobal(".str_inline_char");
      if (!fmt) {
        fmt = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                 GlobalValue::PrivateLinkage, formatStr, ".str_inline_char");
      }
      fmtStrVar = fmt;
    } else if (exprType->isPointerTy()) {
      if (exprType == PointerType::get(Type::getInt8Ty(Context), 0)) {
        Constant *formatStr = ConstantDataArray::getString(Context, "%s", true);
        GlobalVariable *fmt = TheModule->getNamedGlobal(".str_inline_string");
        if (!fmt) {
          fmt = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                 GlobalValue::PrivateLinkage, formatStr, ".str_inline_string");
        }
        fmtStrVar = fmt;
      } else {
        Constant *formatStr = ConstantDataArray::getString(Context, "%d", true);
        GlobalVariable *fmt = TheModule->getNamedGlobal(".str_inline_int");
        if (!fmt) {
          fmt = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                 GlobalValue::PrivateLinkage, formatStr, ".str_inline_int");
        }
        fmtStrVar = fmt;
      }
    } else {
      Constant *formatStr = ConstantDataArray::getString(Context, "%d", true);
      GlobalVariable *fmt = TheModule->getNamedGlobal(".str_inline_int");
      if (!fmt) {
        fmt = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                 GlobalValue::PrivateLinkage, formatStr, ".str_inline_int");
      }
      fmtStrVar = fmt;
    }
    Constant *zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
    std::vector<Constant*> indices = {zero, zero};
    Constant *fmtStrPtr = ConstantExpr::getGetElementPtr(fmtStrVar->getValueType(), fmtStrVar, indices);
    Builder.CreateCall(getPrintfFunction(), {fmtStrPtr, exprVal});
    return exprVal;
  }
  
  // --- PRINT_NEWLINE ---
  if (strcmp(node->type, "PRINT_NEWLINE") == 0) {
    Constant *newlineStr = ConstantDataArray::getString(Context, "\n", true);
    GlobalVariable *nlVar = TheModule->getNamedGlobal(".str_newline");
    if (!nlVar) {
      nlVar = new GlobalVariable(*TheModule, newlineStr->getType(), true,
      GlobalValue::PrivateLinkage, newlineStr, ".str_newline");
    }
    Constant *zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
    std::vector<Constant*> indices = {zero, zero};
    Constant *nlPtr = ConstantExpr::getGetElementPtr(nlVar->getValueType(), nlVar, indices);
    Builder.CreateCall(getPrintfFunction(), {nlPtr});
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  return nullptr;
}

void extractParams(ASTNode* paramNode, std::vector<Type*>& types, std::vector<std::string>& names) {
  if (!paramNode) return;
  if (strcmp(paramNode->type, "PARAM") == 0) {
    std::string typeStr = (paramNode->left && paramNode->left->value) ? paramNode->left->value : "int";
    if (typeStr == "int") types.push_back(Type::getInt32Ty(Context));
    else if (typeStr == "float") types.push_back(Type::getFloatTy(Context));
    else if (typeStr == "bool") types.push_back(Type::getInt1Ty(Context));
    else if (typeStr == "char") types.push_back(Type::getInt8Ty(Context));
    else if (typeStr == "string") types.push_back(PointerType::get(Type::getInt8Ty(Context), 0));
    else types.push_back(Type::getInt32Ty(Context));
    names.push_back(paramNode->value);
  } else if (strcmp(paramNode->type, "PARAM_LIST") == 0) {
    extractParams(paramNode->left, types, names);
    extractParams(paramNode->right, types, names);
  }
}

void extractArgs(ASTNode* argNode, std::vector<Value*>& args, Function* currentFunction) {
  if (!argNode) return;
  if (strcmp(argNode->type, "ARG_LIST") == 0) {
    extractArgs(argNode->left, args, currentFunction);
    extractArgs(argNode->right, args, currentFunction);
  } else {
    Value *argVal = generateIR(argNode, currentFunction);
    args.push_back(argVal);
  }
}

// --- Main ---
// Generate IR for function definitions then generate global statements in main().
int main() {
  if (yyparse() != 0) {
    return 1;
  }
  
  generateFunctions(root);
  
  FunctionType *mainType = FunctionType::get(Type::getInt32Ty(Context), false);
  Function *mainFunc = Function::Create(mainType, Function::ExternalLinkage, "main", TheModule);
  BasicBlock *globalBB = BasicBlock::Create(Context, "global", mainFunc);
  Builder.SetInsertPoint(globalBB);
  generateGlobalStatements(root, mainFunc);
  
  BasicBlock *curBB = Builder.GetInsertBlock();
  if (!curBB->getTerminator())
    Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(Context), 0));
  
  std::string error;
  raw_string_ostream errorStream(error);
  if (verifyModule(*TheModule, &errorStream)) {
    std::cerr << "Error: " << errorStream.str() << "\n";
    return 1;
  }
  TheModule->print(outs(), nullptr);
  delete TheModule;
  return 0;
}
