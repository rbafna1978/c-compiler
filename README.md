# C Compiler with LLVM Backend

> Complete compiler for a subset of C, targeting LLVM IR with SSA-based optimizations

## 🚧 Status: Work in Progress

This project is currently under active development. Expected completion: **February 2026**

## 📋 Overview

A production-quality compiler implementing the full compilation pipeline from C source code to optimized LLVM IR. Supports functions, structs, pointers, arrays, and control flow with SSA-based optimization passes.

## ✨ Planned Features

- [x] Language specification design
- [x] Lexer implementation (Flex)
- [ ] Parser implementation (Bison)
- [ ] Abstract Syntax Tree (AST) construction
- [ ] Semantic analysis & type checking
- [ ] Symbol table management
- [ ] LLVM IR code generation
- [ ] SSA-based optimizations
- [ ] Target code generation via LLVM
- [ ] Standard library runtime support

## 🏗️ Architecture

```
┌──────────────┐
│  C Source    │
│   (.c file)  │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│    Lexer     │  ← Tokenization (Flex)
│  (Scanner)   │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│    Parser    │  ← Syntax Analysis (Bison)
│  (AST Build) │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│   Semantic   │  ← Type Checking
│   Analyzer   │    Symbol Tables
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ LLVM IR Gen  │  ← SSA Form
│  (CodeGen)   │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Optimization │  ← Constant Folding
│    Passes    │    Dead Code Elimination
└──────┬───────┘    Register Allocation
       │
       ▼
┌──────────────┐
│ Native Code  │  ← x86_64 Assembly
│  (via LLVM)  │
└──────────────┘
```

## 🛠️ Tech Stack

- **Language:** C++ 17
- **Lexer:** Flex
- **Parser:** Bison
- **IR/Backend:** LLVM 17
- **Build System:** CMake
- **Testing:** Google Test

## 📖 Supported Language Features

**Data Types:**
- Primitives: `int`, `float`, `char`, `void`
- Pointers: `int*`, `char*`, etc.
- Arrays: `int arr[10]`
- Structs: `struct Point { int x; int y; }`

**Control Flow:**
- `if`/`else` statements
- `while` loops
- `for` loops
- `return` statements
- Function calls

**Operators:**
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `&&`, `||`, `!`
- Assignment: `=`, `+=`, `-=`, etc.

**Example Program:**
```c
struct Point {
    int x;
    int y;
};

int distance(struct Point* p1, struct Point* p2) {
    int dx = p2->x - p1->x;
    int dy = p2->y - p1->y;
    return dx * dx + dy * dy;
}

int main() {
    struct Point p1 = {0, 0};
    struct Point p2 = {3, 4};
    return distance(&p1, &p2);
}
```

## 🎯 Optimization Passes

**Implemented SSA Optimizations:**
- **Constant Folding:** Evaluate constant expressions at compile-time
- **Constant Propagation:** Replace variables with known constant values
- **Dead Code Elimination:** Remove unreachable code
- **Common Subexpression Elimination:** Reuse computed values
- **Register Allocation:** Efficient register usage via graph coloring

**Performance Goal:** 15% runtime improvement over `-O0` baseline

## 📝 Implementation Roadmap

**Phase 1: Frontend (Weeks 1-2)**
- Lexer with Flex
- Parser with Bison
- AST construction

**Phase 2: Semantic Analysis (Week 3)**
- Type checking
- Symbol table management
- Scope resolution

**Phase 3: Code Generation (Week 4)**
- LLVM IR generation
- SSA form construction
- Basic blocks and control flow

**Phase 4: Optimization (Week 5)**
- Optimization pass implementation
- Benchmarking suite
- Performance tuning

## 🚀 Quick Start (Coming Soon)

Once complete, compiling a C program will be:

```bash
# Compile C to LLVM IR
./compiler input.c -o output.ll

# Optimize LLVM IR
opt -O2 output.ll -o output.opt.ll

# Generate native binary
llc output.opt.ll -o output.s
gcc output.s -o output
./output
```

## 📊 Benchmark Results

Performance comparison on standard benchmark suite (coming soon):

| Benchmark | Unoptimized | Optimized | Improvement |
|-----------|-------------|-----------|-------------|
| fibonacci | TBD | TBD | TBD |
| matrix_mul | TBD | TBD | TBD |
| quicksort | TBD | TBD | TBD |

Target: **15% average improvement**

## 📚 Resources

Key references for this implementation:
- [LLVM Language Reference](https://llvm.org/docs/LangRef.html)
- [SSA Book](https://pfalcon.github.io/ssabook/latest/)
- [Flex & Bison Documentation](https://www.gnu.org/software/bison/manual/)
- [Engineering a Compiler (Cooper & Torczon)](https://www.elsevier.com/books/engineering-a-compiler/cooper/978-0-12-815412-0)

## 📧 Contact

Questions? Reach out at bafnarishit@gmail.com

---

**Last Updated:** February 2026
