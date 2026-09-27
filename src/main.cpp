#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "autodiff/autodiff.h"
#include "codegen/codegen.h"
#include "lexer/lexer.h"
#include "optimizer/optimizer.h"
#include "parser/parser.h"
#include "sema/sema.h"

extern char** environ;

namespace {

constexpr const char* kUsage =
    "Usage: compiler [options] <input-file>\n"
    "Options:\n"
    "  --help          Show this help message\n"
    "  -o <file>       Output file (default: a.out). A .ll extension writes LLVM IR\n"
    "  --emit-ir       Write LLVM IR (to -o, or stdout) instead of an executable\n"
    "  -O              Enable optimizations\n"
    "  --dump-tokens   Print the token stream and stop\n"
    "  --dump-ast      Print the AST and stop\n"
    "  --dump-grad     Print the AST after autodiff (shows generated __grad_* functions) and stop\n"
    "  --no-fuse       Disable elementwise fusion (one loop per tensor operator, for comparison)\n"
    "  --no-tile       Disable cache-blocked matmul (always use the naive triple loop)\n";

bool endsWith(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Runs argv[0] with arguments; returns its exit status, or -1 if it could not be run.
int run(const std::vector<std::string>& args) {
  std::vector<char*> argv;
  for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
  argv.push_back(nullptr);
  pid_t pid;
  if (posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), environ) != 0) return -1;
  int status = 0;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

}  // namespace

int main(int argc, char** argv) {
  std::string input, output;
  bool emit_ir = false, optimize = false, dump_tokens = false, dump_ast = false, dump_grad = false;
  bool fuse = true, tile = true;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help") {
      std::cout << kUsage;
      return 0;
    } else if (arg == "-o" && i + 1 < argc) {
      output = argv[++i];
    } else if (arg == "--emit-ir") {
      emit_ir = true;
    } else if (arg == "-O") {
      optimize = true;
    } else if (arg == "--dump-tokens") {
      dump_tokens = true;
    } else if (arg == "--dump-ast") {
      dump_ast = true;
    } else if (arg == "--dump-grad") {
      dump_grad = true;
    } else if (arg == "--no-fuse") {
      fuse = false;
    } else if (arg == "--no-tile") {
      tile = false;
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "unknown option: " << arg << "\n" << kUsage;
      return 2;
    } else {
      input = arg;
    }
  }
  if (input.empty()) {
    std::cerr << "No input provided. Use --help for usage.\n";
    return 2;
  }

  std::ifstream in(input);
  if (!in) {
    std::cerr << "cannot open " << input << "\n";
    return 2;
  }
  std::stringstream buffer;
  buffer << in.rdbuf();
  const std::string source = buffer.str();

  if (dump_tokens) {
    compiler::lexer::Lexer lexer;
    for (const auto& t : lexer.tokenize(source, input)) std::cout << t.line << ": " << t.lexeme << "\n";
    for (const auto& e : lexer.errors()) std::cerr << e.filename << ":" << e.line << ": " << e.message << "\n";
    return lexer.errors().empty() ? 0 : 1;
  }

  compiler::parser::Parser parser;
  auto unit = parser.parse(source, input);
  if (!parser.errors().empty()) {
    for (const auto& e : parser.errors()) std::cerr << e.filename << ":" << e.line << ": error: " << e.message << "\n";
    return 1;
  }
  if (dump_ast) {
    std::cout << compiler::ast::prettyPrint(*unit);
    return 0;
  }

  std::vector<std::string> diagnostics;
  if (!compiler::autodiff::analyze(*unit, diagnostics)) {
    for (const auto& d : diagnostics) std::cerr << input << ": error: " << d << "\n";
    return 1;
  }
  if (dump_grad) {
    std::cout << compiler::ast::prettyPrint(*unit);
    return 0;
  }
  compiler::codegen::CodeGenerator codegen(input, fuse, tile);
  if (!codegen.generate(*unit)) {
    for (const auto& d : codegen.diagnostics()) std::cerr << input << ": error: " << d << "\n";
    return 1;
  }

  if (optimize) compiler::optimizer::optimize(codegen.module());

  if (emit_ir || endsWith(output, ".ll")) {
    if (output.empty()) {
      std::cout << codegen.ir();
    } else if (!(std::ofstream(output) << codegen.ir())) {
      std::cerr << "cannot write " << output << "\n";
      return 2;
    }
    return 0;
  }

  // Executable: hand the IR to clang for assembling and linking.
  if (output.empty()) output = "a.out";
  const std::string tmp = output + ".tmp.ll";
  if (!(std::ofstream(tmp) << codegen.ir())) {
    std::cerr << "cannot write " << tmp << "\n";
    return 2;
  }
  // Prefer the clang that shipped with the LLVM this compiler was built against (baked in by
  // CMakeLists.txt) so the version that parses this IR always matches the version that emitted
  // it; fall back to PATH's clang if that one isn't present (e.g. a relocated/packaged binary).
#ifdef COMPILER_LLVM_CLANG_PATH
  const std::string clang_path =
      std::filesystem::exists(COMPILER_LLVM_CLANG_PATH) ? COMPILER_LLVM_CLANG_PATH : "clang";
#else
  const std::string clang_path = "clang";
#endif
  const std::vector<std::string> cmd = {clang_path, "-Wno-override-module", "-x", "ir", tmp, "-o", output};
  const int rc = run(cmd);
  std::remove(tmp.c_str());
  if (rc != 0) {
    std::cerr << "clang failed (status " << rc << ")\n";
    return 2;
  }
  return 0;
}
