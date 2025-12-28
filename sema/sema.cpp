#pragma once
#include "sema.hpp"
#include <optional>
#include <string>
#include <memory>
using namespace zap::sema;

void SymbolTable::addFunction(FunctionSymbol &&func)
{
    functions_.emplace(func.name, std::make_shared<FunctionSymbol>(std::move(func)));
}

void SymbolTable::addVariable(VariableSymbol &&var, Scope &scope)
{
    scope.variables.emplace(var.name, std::move(var));
}

std::shared_ptr<FunctionSymbol> SymbolTable::getFunction(const std::string &name)
{
    auto it = functions_.find(name);
    if (it != functions_.end())
    {
        return it->second;
    }
    return nullptr;
}