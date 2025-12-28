#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "llvm/IR/IRBuilder.h"
namespace zap::sema
{
    struct VariableSymbol
    {
        std::string name;
        std::string type;
        llvm::AllocaInst *allocator;
    };
    struct Scope
    {
        std::unordered_map<std::string, VariableSymbol> variables;
    };
    struct FunctionSymbol
    {
        std::string name;
        bool isExtern_ = false;
        bool isStatic_ = false;
        bool isPublic_ = false;
        Scope scope;
    };

    class SymbolTable
    {
    private:
        std::unordered_map<std::string, std::shared_ptr<FunctionSymbol>> functions_;

    public:
        bool found_main = false;
        void addFunction(FunctionSymbol &&func);
        void addVariable(VariableSymbol &&var, Scope &scope);
        std::shared_ptr<FunctionSymbol> getFunction(const std::string &name);
        void analyze();
    };
} // namespace sema