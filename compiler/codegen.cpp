#include "codegen.hpp"
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"
#include "ast/const/const_id.hpp"
#include "ast/const/const_float.hpp"
#include "ast/unary_expr.hpp"
#include "ast/fun_call.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>

void zap::Compiler::addCommonFunctions()
{
    // Add puts function declaration (from C standard library)
    llvm::Type *i8ptr = llvm::Type::getInt8Ty(context_)->getPointerTo();
    llvm::FunctionType *putsFuncType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context_), {i8ptr}, false);
    llvm::Function::Create(putsFuncType, llvm::Function::ExternalLinkage, "puts", &module_);

    llvm::FunctionType *printlnFuncType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(context_), {i8ptr}, false);
    llvm::Function *printlnFunc = llvm::Function::Create(
        printlnFuncType, llvm::Function::InternalLinkage, "println", &module_);

    llvm::BasicBlock *printlnEntry = llvm::BasicBlock::Create(context_, "entry", printlnFunc);
    llvm::IRBuilder<> builder(printlnEntry);

    llvm::Function *putsFunc = module_.getFunction("puts");

    auto argsIt = printlnFunc->arg_begin();
    builder.CreateCall(putsFunc, {&*argsIt});
    builder.CreateRetVoid();

    // Add println to symbol table
    zap::sema::FunctionSymbol printlnSymbol{
        "println",
        false, // isExtern
        false, // isStatic
        true,  // isPublic
        zap::sema::Scope()};
    symTable_->addFunction(std::move(printlnSymbol));
}

void zap::Compiler::compile(const std::unique_ptr<RootNode, std::default_delete<RootNode>> &root)
{

    addCommonFunctions();

    for (const auto &child : root->children)
    {
        if (auto *funDecl = dynamic_cast<FunDecl *>(child.get()))
        {
            generateFunction(*funDecl);
        }
    }
}

void zap::Compiler::generateFunction(const FunDecl &funDecl)
{
    std::vector<llvm::Type *> paramTypes;
    for (const auto &param : funDecl.params_)
    {
        paramTypes.push_back(mapType(*param->type));
    }

    llvm::Type *returnType = mapType(*funDecl.returnType_);
    llvm::FunctionType *funcType =
        llvm::FunctionType::get(returnType, paramTypes, false);

    llvm::Function *function =
        llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                               funDecl.name_, module_);

    unsigned idx = 0;
    for (auto &arg : function->args())
    {
        arg.setName(funDecl.params_[idx++]->name);
    }

    llvm::BasicBlock *entry =
        llvm::BasicBlock::Create(context_, "entry", function);
    builder_.SetInsertPoint(entry);

    currentScope_ = const_cast<zap::sema::Scope *>(funDecl.scope_.get());

    if (funDecl.body_)
    {
        generateBody(*funDecl.body_, funDecl.scope_.get());
    }
    else if (funDecl.isExtern_)
    {
        return;
    }

    // Reset scope
    currentScope_ = nullptr;
}

void zap::Compiler::generateBody(const BodyNode &body, zap::sema::Scope *scope)
{
    for (const auto &stmt : body.statements)
    {
        if (auto *retNode = dynamic_cast<ReturnNode *>(stmt.get()))
        {
            generateReturn(*retNode);
        }
        else if (auto *varDecl = dynamic_cast<VarDecl *>(stmt.get()))
        {
            generateLet(*varDecl);
        }
        else if (auto *assignNode = dynamic_cast<AssignNode *>(stmt.get()))
        {
            generateAssign(*assignNode);
        }
        else if (auto *funCall = dynamic_cast<FunCall *>(stmt.get()))
        {

            generateExpression(*funCall);
        }
    }
}

void zap::Compiler::generateLet(const VarDecl &varDecl)
{
    llvm::AllocaInst *var = builder_.CreateAlloca(mapType(*varDecl.type_), nullptr, varDecl.name_);
    llvm::Value *initValue;
    if (varDecl.initializer_)
    {
        initValue = generateExpression(*varDecl.initializer_);
        if (!initValue)
        {
            std::cerr << "Failed to generate initializer expression for variable '" << varDecl.name_ << "'" << std::endl;
            return;
        }
    }
    else
    {
        initValue = llvm::Constant::getNullValue(mapType(*varDecl.type_));
    }

    builder_.CreateStore(initValue, var);

    // Update the allocator in the scope
    if (currentScope_)
    {
        auto it = currentScope_->variables.find(varDecl.name_);
        if (it != currentScope_->variables.end())
        {
            it->second.allocator = var;
        }
    }
}

void zap::Compiler::generateAssign(const AssignNode &assignNode)
{
    auto it = currentScope_->variables.find(assignNode.target_);
    if (it != currentScope_->variables.end() && it->second.allocator)
    {
        llvm::Value *exprValue = generateExpression(*assignNode.expr_);
        if (!exprValue)
        {
            std::cerr << "Failed to generate expression for assignment to '" << assignNode.target_ << "'" << std::endl;
            return;
        }
        builder_.CreateStore(exprValue, it->second.allocator);
    }
}

void zap::Compiler::generateReturn(const ReturnNode &retNode)
{
    if (retNode.returnValue)
    {
        // TODO: check functon return type
        llvm::Value *retValue = generateExpression(*retNode.returnValue);
        if (!retValue)
        {
            std::cerr << "Failed to generate return value expression" << std::endl;
            return;
        }
        builder_.CreateRet(retValue);
    }
    else
    {
        builder_.CreateRetVoid();
    }
}

llvm::Value *zap::Compiler::generateExpression(const ExpressionNode &expr)
{
    if (auto *constInt = dynamic_cast<const ConstInt *>(&expr))
    {
        llvm::Value *value = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context_), constInt->value_);
        return value;
    }
    else if (auto *constFloat = dynamic_cast<const ConstFloat *>(&expr))
    {
        llvm::Value *value = llvm::ConstantFP::get(
            llvm::Type::getFloatTy(context_), constFloat->value_);
        return value;
    }
    else if (auto *constString = dynamic_cast<const ConstString *>(&expr))
    {
        llvm::Value *value = builder_.CreateGlobalStringPtr(constString->value_);
        return value;
    }
    else if (auto *constId = dynamic_cast<const ConstId *>(&expr))
    {
        // Look up the variable in current scope
        if (currentScope_)
        {
            auto it = currentScope_->variables.find(constId->value_);
            if (it != currentScope_->variables.end() && it->second.allocator)
            {
                // Load the value from the allocated memory
                return builder_.CreateLoad(
                    mapType(TypeNode(it->second.type)),
                    it->second.allocator,
                    constId->value_);
            }
        }
        std::cerr << "Variable '" << constId->value_ << "' not found in scope" << std::endl;
        return nullptr;
    }
    else if (auto *binExpr = dynamic_cast<const BinExpr *>(&expr))
    {
        llvm::Value *leftValue = generateExpression(*binExpr->left_);
        llvm::Value *rightValue = generateExpression(*binExpr->right_);
        llvm::Value *result = nullptr;
        if (binExpr->op_ == "+")
        {
            result = builder_.CreateAdd(leftValue, rightValue);
        }
        else if (binExpr->op_ == "-")
        {
            result = builder_.CreateSub(leftValue, rightValue);
        }
        else if (binExpr->op_ == "*")
        {
            result = builder_.CreateMul(leftValue, rightValue);
        }
        else if (binExpr->op_ == "/")
        {
            result = builder_.CreateSDiv(leftValue, rightValue);
        }
        else if (binExpr->op_ == "~")
        {

            result = builder_.CreateCall(getStringConcat(&this->module_, context_), {leftValue, rightValue});
        }

        return result;
    }
    else if (auto *unaryExpr = dynamic_cast<const UnaryExpr *>(&expr))
    {
        llvm::Value *operandValue = generateExpression(*unaryExpr->expr_);
        if (!operandValue)
            return nullptr;

        if (unaryExpr->op_ == "-")
        {
            return builder_.CreateNeg(operandValue);
        }
        else if (unaryExpr->op_ == "!")
        {
            return builder_.CreateNot(operandValue);
        }
        else if (unaryExpr->op_ == "*")
        {
            // TODO: track actual pointed-to type
            llvm::Type *elementType = llvm::Type::getInt32Ty(context_);
            if (operandValue->getType()->isPointerTy())
            {

                return builder_.CreateLoad(elementType, operandValue);
            }
            std::cerr << "Dereference operator applied to non-pointer type" << std::endl;
            return nullptr;
        }
        else
        {
            std::cerr << "Unsupported unary operator: " << unaryExpr->op_ << std::endl;
            return nullptr;
        }
    }
    else if (auto *funCall = dynamic_cast<const FunCall *>(&expr))
    {
        // Look up the function in the symbol table
        auto funcSymbol = symTable_->getFunction(funCall->funcName_);
        if (!funcSymbol)
        {
            // Try to get from module directly (for built-in functions)
            llvm::Function *callee = module_.getFunction(funCall->funcName_);
            if (!callee)
            {
                std::cerr << "Function '" << funCall->funcName_ << "' not found" << std::endl;
                return nullptr;
            }

            std::vector<llvm::Value *> args;
            for (const auto &param : funCall->params_)
            {
                llvm::Value *argValue = generateExpression(*param);
                if (!argValue)
                    return nullptr;
                args.push_back(argValue);
            }

            return builder_.CreateCall(callee, args);
        }

        std::vector<llvm::Value *> args;
        for (const auto &param : funCall->params_)
        {
            llvm::Value *argValue = generateExpression(*param);
            if (!argValue)
                return nullptr;
            args.push_back(argValue);
        }

        llvm::Function *callee = module_.getFunction(funCall->funcName_);
        if (!callee)
        {
            std::cerr << "LLVM function '" << funCall->funcName_ << "' not found" << std::endl;
            return nullptr;
        }

        return builder_.CreateCall(callee, args);
    }
    else
    {
        std::cerr << "Unsupported expression type in code generation" << std::endl;
        return nullptr;
    }
}

llvm::Function *zap::Compiler::getStringConcat(llvm::Module *module, llvm::LLVMContext &context)
{
    if (!module)
        return nullptr;
    llvm::Function *fn = module->getFunction("str_concat");
    if (fn)
        return fn;

    llvm::Type *i8ptr = llvm::Type::getInt8Ty(context)->getPointerTo();
    llvm::FunctionType *ft = llvm::FunctionType::get(i8ptr, {i8ptr, i8ptr}, false);
    fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "str_concat", module);
    return fn;
}

llvm::Type *zap::Compiler::mapType(const TypeNode &typeNode)
{
    llvm::Type *baseType = nullptr;

    // Map base type
    if (typeNode.typeName == "i1")
    {
        baseType = llvm::Type::getInt1Ty(context_);
    }
    else if (typeNode.typeName == "i8")
    {
        baseType = llvm::Type::getInt8Ty(context_);
    }
    else if (typeNode.typeName == "i16")
    {
        baseType = llvm::Type::getInt16Ty(context_);
    }
    else if (typeNode.typeName == "i32")
    {
        baseType = llvm::Type::getInt32Ty(context_);
    }
    else if (typeNode.typeName == "i64")
    {
        baseType = llvm::Type::getInt64Ty(context_);
    }
    else if (typeNode.typeName == "f32")
    {
        baseType = llvm::Type::getFloatTy(context_);
    }
    else if (typeNode.typeName == "f64")
    {
        baseType = llvm::Type::getDoubleTy(context_);
    }
    else if (typeNode.typeName == "void")
    {
        baseType = llvm::Type::getVoidTy(context_);
    }
    else if (typeNode.typeName == "str")
    {
        baseType = llvm::Type::getInt8Ty(context_)->getPointerTo();
    }
    else
    {

        baseType = llvm::Type::getInt32Ty(context_);
    }

    if (typeNode.isArray)
    {
        // TODO: Handle array size
        return llvm::ArrayType::get(baseType, 0);
    }

    if (typeNode.isPointer)
    {
        return baseType->getPointerTo();
    }

    // Handle reference type
    if (typeNode.isReference)
    {
        return baseType->getPointerTo();
    }

    return baseType;
}

void zap::Compiler::emitIRToFile(const std::string &filename)
{
    std::error_code EC;
    llvm::raw_fd_ostream OS(filename, EC);
    if (EC)
    {
        std::cerr << "Error writing to file: " << EC.message() << std::endl;
        return;
    }
    module_.print(OS, nullptr);
    OS.close();
    std::cout << "IR written to " << filename << std::endl;
}

void zap::Compiler::compileIR(const std::string &irFilename, const std::string &outputFilename)
{

    std::string objFile = outputFilename + ".o";
    std::string command = "llc -filetype=obj -relocation-model=pic -o " + objFile + " " + irFilename;
    std::cout << "Running: " << command << std::endl;
    int result = system(command.c_str());

    if (result != 0)
    {
        std::cerr << "Error compiling IR with llc" << std::endl;
        return;
    }

    std::string linkCommand = "gcc -fPIE -pie -o " + outputFilename + " " + objFile;
    std::cout << "Running: " << linkCommand << std::endl;
    result = system(linkCommand.c_str());

    if (result != 0)
    {
        std::cerr << "Error linking executable with gcc" << std::endl;
        return;
    }

    std::cout << "Compilation successful! Output: " << outputFilename << std::endl;
}