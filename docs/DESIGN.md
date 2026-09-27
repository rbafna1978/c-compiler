# Design notes: why the two hard parts are correct

The [README](../README.md) states the results; this is the reasoning behind them, for the two
pieces of this compiler that are actual analysis rather than mechanical lowering. Both are backed
by tests, not just argument — this document says which test proves which claim.

## Autodiff correctness

`grad(f, i, args...)` ([`src/autodiff/autodiff.cpp`](../src/autodiff/autodiff.cpp)) builds a new
function `__grad_f_i` by reverse-mode differentiation, as an AST-to-AST rewrite:

1. **Forward pass**: copy `f`'s declarations verbatim (they're required to be plain `let`-style
   declarations with initializers — see "why straight-line" below).
2. **Adjoint variables**: one `__d_x` per differentiable local/parameter `x`, initialized to zero.
3. **Seed**: the return expression gets adjoint `1.0` (∂(return value)/∂(return value) = 1).
4. **Backward pass**: walk the declarations in *reverse* order. For each `y = expr(x1, x2, ...)`
   with adjoint `ȳ` already accumulated, add `∂expr/∂xᵢ · ȳ` into each `x̄ᵢ`, via the standard
   per-operator chain rule:

   | Expression | Adjoint pushed to each operand |
   |---|---|
   | `a + b` | `ā += ḡ`, `b̄ += ḡ` |
   | `a - b` | `ā += ḡ`, `b̄ += -ḡ` |
   | `a * b` | `ā += ḡ * b`, `b̄ += ḡ * a` |
   | `a / b` | `ā += ḡ / b`, `b̄ += -ḡ * a / b²` |
   | `exp(a)` | `ā += ḡ * exp(a)` |
   | `log(a)` | `ā += ḡ / a` |
   | `tanh(a)` | `ā += ḡ * (1 - tanh(a)²)` |
   | `matmul(A, B)` | `Ā += ḡ · Bᵀ`, `B̄ += Aᵀ · ḡ` (standard matrix-calculus identity) |
   | `transpose(A)` | `Ā += transpose(ḡ)` |
   | `sum(A)` | `Ā += ḡ` (broadcast: every element of `A` contributed equally to the sum) |

   This is exactly the multivariable chain rule applied mechanically, term by term — the same
   thing PyTorch's autograd or JAX's `grad` do, just implemented here as a source-to-source AST
   rewrite instead of at runtime over a traced tape.
5. Return `x̄ᵢ` for the requested parameter `i`.

**Why straight-line only.** Reverse-mode AD needs to walk the computation backward once the forward
values are known. For a loop or a conditional, that requires either unrolling (only sound if the
trip count is static) or a real tape (recording which branch executed, at runtime). Restricting to
straight-line code sidesteps that entirely — every statement's position in the reverse walk is
known at compile time — at the cost of not being able to differentiate through control flow *within
one call*. (Using the gradient *inside* a loop, e.g. one gradient-descent step per iteration, is
unaffected: that loop is in the caller, not in the differentiated function.) A function that isn't
straight-line is rejected with a specific error naming the offending line
(`AutodiffTest.RejectsFunctionsItCannotDifferentiate`), not silently miscompiled.

**Why a comparison's gradient is zero.** `a < b` has type `int` (0 or 1), not `float`, so
`backprop` recognizes it as non-differentiable input and stops (`isDiff(et)` is false for `int`).
This is mathematically correct almost everywhere — a step function's derivative is 0 except at the
discontinuity — and is exercised by `AutodiffTest.TreatsNonFloatSubexpressionsAsConstants`.

**How it's validated.** Every gradient test compares against **float64 central finite differences of
an independent NumPy re-implementation of the same loss** (`tests/integration/gen_expected.py`) —
not against another run of this compiler's own rules, which would only prove the rules are
self-consistent, not correct. `grad_scalar.c`, `grad_tensor.c`, `grad_train.c` and `grad_mlp.c`
cover scalar expressions, tensor expressions with `matmul`/`transpose`/broadcast, and two actual
training loops (linear regression and a `tanh` MLP, both converging) — see the README's example.

## Fusion correctness

The risk with fusing `x * 2 - x / 3 + 1` into one loop is subtle, not obvious: the *unfused* codegen
computes each sub-expression's result in **its own promoted type** before combining it with the
next operator (e.g. an `int` sub-result is truncated *before* being promoted to `float` by the
outer `+`). A naive fusion that just picks one common element type for the whole expression would
silently change results for any expression that mixes `int` and `float` sub-computations.

`buildFusedElem` ([`src/codegen/codegen.cpp`](../src/codegen/codegen.cpp)) avoids this by computing
each AST node's element type from **that node's own `expr_type`** (already resolved by sema) and
converting each child to *its parent's* element type immediately before combining — which is
exactly what the unfused path does one call frame at a time, just inlined into a single loop
instead of spread across several. No conversion is forced earlier or later than the unfused path
would do it.

This is checked, not just argued: every existing integration program (including ones that mix
`int` and `float` tensors) is run through the compiler twice, with `--no-fuse` on and off, in
`benchmarks/run_benchmarks.py`'s and the manual verification behind Phase 10's commit — output must
be byte-identical. `CodegenTest.FusesElementwiseChainsIntoOneLoop` checks the structural claim (one
loop instead of four) so a regression that silently stopped fusing would fail a fast unit test
instead of only showing up as a missed speedup.

**Known gap**: fusion stops at `sum`/`matmul`/`transpose` — `sum(a * b + c)` fuses `a * b + c` into
one loop, then reduces it in a separate loop, rather than fusing the reduction too. Real ML
compilers (XLA, TVM) do fuse elementwise producers into a consuming reduction; it wasn't done here
to keep the change bounded and because the correctness argument above (per-node element types) gets
more intricate once the loop also carries a running accumulator across iterations. It's a genuine
next step, not a hidden limitation.

## Tiled matmul correctness

The blocked matmul decomposes the reduction dimension `k` into blocks and accumulates each output
cell across those blocks with a read-modify-write (`out[i][j] += Σ over this block`). This is exact
regardless of block size or order: summing `Σₖ a[i][k]·b[k][j]` in any partition of `k` into
contiguous ranges gives the same total — blocking only changes the *order* memory is visited (for
cache locality), never *what* is summed. The result is checked bit-identical against the naive
triple loop on a 64×64 case (`tensor_tiled_matmul` integration test) and structurally distinguished
from the naive path in `CodegenTest.TilesLargeMatmulButNotSmall` (the tiled path zero-fills its
output up front; the naive path doesn't need to). Tiling only applies when every dimension is an
exact multiple of the block size (32) — there is deliberately no partial-tile remainder path, so
there's no edge case to get subtly wrong; dimensions that don't divide evenly fall back to the
naive loop.
