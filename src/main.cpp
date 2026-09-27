#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "codegen/codegen.h"
#include "parser/parser.h"
#include "sema/sema.h"

int main(int argc, char** argv) {
  std::string input;
  std::string output;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help") {
      std::cout << "Usage: compiler [options] <input-file>\n"
                << "Options:\n"
                << "  --help        Show this help message\n"
                << "  -o <file>     Write LLVM IR to file (default: stdout)\n";
      return 0;
    }
    if (arg == "-o" && i + 1 < argc) {
      output = argv[++i];
    } else {
      input = arg;
    }
  }
  if (input.empty()) {
    std::cerr << "No input provided. Use --help for usage.\n";
    return 1;
  }

  std::ifstream in(input);
  if (!in) {
    std::cerr << "cannot open " << input << "\n";
    return 1;
  }
  std::stringstream src;
  src << in.rdbuf();

  compiler::parser::Parser parser;
  auto unit = parser.parse(src.str(), input);
  if (!parser.errors().empty()) {
    for (const auto& e : parser.errors()) std::cerr << e.filename << ":" << e.line << ": " << e.message << "\n";
    return 1;
  }
  compiler::sema::SemanticAnalyzer sema;
  if (!sema.analyze(*unit)) {
    for (const auto& d : sema.diagnostics()) std::cerr << input << ": " << d << "\n";
    return 1;
  }
  compiler::codegen::CodeGenerator codegen(input);
  if (!codegen.generate(*unit)) {
    for (const auto& d : codegen.diagnostics()) std::cerr << input << ": " << d << "\n";
    return 1;
  }
  if (output.empty()) {
    std::cout << codegen.ir();
  } else {
    std::ofstream(output) << codegen.ir();
  }
  return 0;
}
