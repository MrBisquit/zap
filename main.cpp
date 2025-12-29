#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "compiler/codegen.hpp"
#include "sema/sema.hpp"
#include <fstream>
#include <iostream>
#define LEXER_DEBUG false
#define PARSER_DEBUG false
#define COMPILER_DEBUG false
using namespace zap;
int main(int argc, char *argv[])
{
  // zapc <file1>
  if (argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " zap " << std::endl;
    return 1;
  }
  else
  {
    std::string fileContent;
    std::ifstream file(argv[1]);
    if (!file)
    {
      printf("%s didnt exsist \n", argv[1]);
    }
    std::string line;

    while (std::getline(file, line))
    {
      fileContent += line + '\n';
    }
    file.close();
    Lexer lex;
    auto toks = lex.tokenize(fileContent);
    auto symTable = std::make_shared<sema::SymbolTable>();

    // Add built-in functions to symbol table before parsing
    zap::sema::FunctionSymbol printlnSymbol{
        "println",
        false, // isExtern
        false, // isStatic
        true,  // isPublic
        zap::sema::Scope()};
    symTable->addFunction(std::move(printlnSymbol));

    Parser parser(symTable);
    if (LEXER_DEBUG)
    {
      for (const auto &token : toks)
      {
        std::cout << "Token: " << token.type << " Value: " << token.value
                  << std::endl;
      }
    }
    auto root = parser.parse(toks);

    zap::Compiler compiler(symTable);
    compiler.compile(root);

    // Extract output filename from input without extension
    std::string outputName = argv[1];
    size_t lastDot = outputName.find_last_of(".");
    if (lastDot != std::string::npos)
    {
      outputName = outputName.substr(0, lastDot);
    }

    // Emit IR to .ll file
    std::string irFile = outputName + ".ll";
    compiler.emitIRToFile(irFile);

    compiler.compileIR(irFile, outputName);

    if (COMPILER_DEBUG)
    {
      std::cout << "Compilation successful!" << std::endl;
    }
  }
  return 0;
}