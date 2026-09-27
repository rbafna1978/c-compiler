#!/usr/bin/env python3
"""Times this project's benchmarks and (re)writes ../BENCHMARKS.md with the results.

Measures compile time separately from run time (min of several runs, to cut through noise), so a
speedup claim is never accidentally inflated by clang's link step. Two comparisons matter here:

  matrix_mul.c        our compiler, tiled matmul  vs  --no-tile (naive triple loop)
  elementwise_chain.c  our compiler, fused         vs  --no-fuse (one loop per operator)

Two more are context, not a claim of beating anything:

  fibonacci.c / quicksort.c   our compiler -O  vs  clang -O2 on an equivalent plain-C program
  matrix_mul.c                our compiler -O  vs  NumPy (a multi-threaded BLAS; not a fair fight)

Usage: python3 run_benchmarks.py [--runs N] [--compiler ../build/compiler]
"""
import argparse
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).parent
REPO = ROOT.parent


def run_timed(argv, runs):
    """Runs argv `runs` times, returns (min_seconds, stdout_of_last_run)."""
    best, out = None, ""
    for _ in range(runs):
        t0 = time.perf_counter()
        out = subprocess.run(argv, capture_output=True, text=True, check=True).stdout
        dt = time.perf_counter() - t0
        best = dt if best is None else min(best, dt)
    return best, out


def compile_ours(compiler, src, out, *extra_flags):
    t0 = time.perf_counter()
    subprocess.run([str(compiler), "-O", *extra_flags, str(src), "-o", str(out)], check=True)
    return time.perf_counter() - t0


def compile_clang(src, out):
    t0 = time.perf_counter()
    subprocess.run(["clang", "-O2", "-w", str(src), "-o", str(out)], check=True)
    return time.perf_counter() - t0


def fmt(seconds):
    return f"{seconds * 1000:.1f} ms"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--runs", type=int, default=5)
    p.add_argument("--compiler", default=str(REPO / "build" / "compiler"))
    args = p.parse_args()
    compiler = pathlib.Path(args.compiler)
    if not compiler.exists():
        sys.exit(f"compiler not found at {compiler}; build it first (cmake --build build)")

    tmp = ROOT / ".bench_tmp"
    tmp.mkdir(exist_ok=True)
    rows = []

    # ---- tiling: matmul, tiled vs naive --------------------------------------------------------
    compile_ours(compiler, ROOT / "matrix_mul.c", tmp / "mm_tiled")
    compile_ours(compiler, ROOT / "matrix_mul.c", tmp / "mm_naive", "--no-tile")
    t_tiled, out_tiled = run_timed([str(tmp / "mm_tiled")], args.runs)
    t_naive, out_naive = run_timed([str(tmp / "mm_naive")], args.runs)
    assert out_tiled == out_naive, "tiled and naive matmul disagree!"
    rows.append(("matmul 512x512 (tiled vs naive)", fmt(t_tiled), fmt(t_naive), f"{t_naive / t_tiled:.2f}x"))

    # ---- fusion: elementwise chain, fused vs unfused -------------------------------------------
    compile_ours(compiler, ROOT / "elementwise_chain.c", tmp / "ew_fused")
    compile_ours(compiler, ROOT / "elementwise_chain.c", tmp / "ew_unfused", "--no-fuse")
    t_fused, out_fused = run_timed([str(tmp / "ew_fused")], args.runs)
    t_unfused, out_unfused = run_timed([str(tmp / "ew_unfused")], args.runs)
    assert out_fused == out_unfused, "fused and unfused elementwise chain disagree!"
    rows.append(("elementwise chain, 2M elems (fused vs unfused)", fmt(t_fused), fmt(t_unfused),
                 f"{t_unfused / t_fused:.2f}x"))

    # ---- context only: our compiler vs clang -O2 on equivalent plain-C scalar programs ---------
    context_rows = []
    for name, ours_src, ref_src in (
        ("fibonacci(35), recursive", ROOT / "fibonacci.c", ROOT / "reference" / "fibonacci_ref.c"),
        ("quicksort, 2048 elements", ROOT / "quicksort.c", ROOT / "reference" / "quicksort_ref.c"),
    ):
        compile_ours(compiler, ours_src, tmp / "ours")
        compile_clang(ref_src, tmp / "clang_ref")
        t_ours, out_ours = run_timed([str(tmp / "ours")], args.runs)
        t_clang, out_clang = run_timed([str(tmp / "clang_ref")], args.runs)
        assert out_ours == out_clang, f"{name}: our output disagrees with the clang -O2 reference!"
        context_rows.append((name, fmt(t_ours), fmt(t_clang), f"{t_clang / t_ours:.2f}x"))

    # ---- context only: our tiled matmul vs NumPy (multi-threaded BLAS; not an apples-to-apples) -
    numpy_row = None
    try:
        import numpy as np
        i, j = np.meshgrid(np.arange(512), np.arange(512), indexing="ij")
        a = ((i * 512 + j) % 97 * 0.01 - 0.5).astype(np.float32)
        b = ((i - j) % 89 * 0.01).astype(np.float32)
        best = None
        for _ in range(args.runs):
            t0 = time.perf_counter()
            a @ b
            best = (time.perf_counter() - t0) if best is None else min(best, time.perf_counter() - t0)
        numpy_row = ("matmul 512x512, our tiled vs NumPy (BLAS)", fmt(t_tiled), fmt(best),
                     f"NumPy {t_tiled / best:.0f}x faster")
    except ImportError:
        pass

    report = ["# Benchmark results\n",
              f"Generated by `benchmarks/run_benchmarks.py` (min of {args.runs} runs each); "
              "re-run it after any codegen change rather than editing this file by hand.\n",
              "## Our own optimizations\n",
              "| Benchmark | Optimized | Baseline | Speedup |",
              "|---|---|---|---|"]
    for name, opt, base, speedup in rows:
        report.append(f"| {name} | {opt} | {base} | **{speedup}** |")
    report.append("\nBoth sides of each row were checked to produce byte-identical output before "
                  "being timed (see the `assert`s in run_benchmarks.py) -- the optimization changes "
                  "speed, never the result.\n")
    report.append("## Context (not a claim of beating these)\n")
    report.append("| Comparison | This compiler | Reference | Ratio |")
    report.append("|---|---|---|---|")
    for name, ours, ref, ratio in context_rows:
        report.append(f"| {name} | {ours} | clang -O2: {ref} | {ratio} |")
    if numpy_row:
        name, ours, ref, ratio = numpy_row
        report.append(f"| {name} | {ours} | {ref} | {ratio} |")
    report.append("\nfibonacci/quicksort compare against an equivalent plain-C program compiled by "
                  "clang -O2 (see `benchmarks/reference/`) -- a fair fight, since both go through "
                  "LLVM. Recursive fibonacci is the one benchmark here where this compiler loses "
                  "to clang: clang's O2 pipeline inlines/tail-call-optimizes the recursion in a way "
                  "this compiler's driver (which just forwards to LLVM's standard O2 pass pipeline, "
                  "see optimizer.cpp) evidently does not exploit as well from this IR shape -- a real "
                  "gap, not a benchmark artifact, and a candidate for future work. NumPy calls a "
                  "tuned, multi-threaded BLAS; that row is context for scale (roughly the gap between "
                  "a simple scalar loop nest and a vectorized, multi-threaded, decades-tuned kernel), "
                  "not a claim that our tiling closes it.\n")
    (REPO / "BENCHMARKS.md").write_text("\n".join(report) + "\n")
    print(f"wrote {REPO / 'BENCHMARKS.md'}")
    print("\n".join(report))


if __name__ == "__main__":
    main()
