#pragma once

#include <string>
#include <vector>

#include "ast/ast.h"

namespace compiler::autodiff {

/**
 * Runs semantic analysis; if the program calls grad(f, i, args...), differentiates f
 * (reverse mode, as an AST-to-AST rewrite into a generated function `__grad_f_i`), rewrites
 * the calls, and re-analyzes the lowered program. Returns false and fills `diagnostics` on error.
 *
 * Differentiable functions are straight-line: local declarations with initializers followed
 * by a single `return` of a float. Supported ops: + - * / unary -, matmul, transpose, sum,
 * exp, log, tanh, on floats and float tensors (scalars broadcast).
 */
bool analyze(ast::TranslationUnit& unit, std::vector<std::string>& diagnostics);

}  // namespace compiler::autodiff
