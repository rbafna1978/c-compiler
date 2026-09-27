#!/usr/bin/env python3
"""Regenerates the expected stdout (tensor_*.out) of the tensor integration programs with NumPy.

Each function below is the NumPy twin of the tensor_*.c program of the same name. Values are
multiples of 0.25 so float32 results are exact and match the compiler bit for bit.
Run: python3 gen_expected.py   (needs numpy)
"""
import pathlib
import numpy as np

F = np.float32
out_dir = pathlib.Path(__file__).parent / "test_programs"


def fmt(v):
    """Formats like the compiler's print(): %g for floats, %d for ints, one innermost row per line."""
    v = np.asarray(v)
    conv = (lambda x: "%g" % float(x)) if v.dtype.kind == "f" else (lambda x: "%d" % int(x))
    if v.ndim == 0:
        return conv(v) + "\n"
    rows = v.reshape(-1, v.shape[-1])
    return "".join(" ".join(conv(x) for x in row) + "\n" for row in rows)


def tensor_basic():
    a = np.array([[1, 2, 3], [4, 5, 6]], F)
    b = a.T.copy()
    c = a @ b
    r = [c, b, c.sum()]
    d = c * 2 - c / 4 + np.ones((2, 2), F)
    r.append(d)
    a[0][1] = 9
    r += [a[0], -a]
    return "".join(map(fmt, r)) + "42\nhello\n"


def tensor_functions():
    m = np.array([[1, 2], [3, 4]], np.int32)
    s = m * 3
    x = np.array([1, 2, 3], F)
    y = np.array([0.5, 0.25, 0.125], F)
    a = np.array([[1, 2, 3], [4, 5, 6]], F)
    a[1] = x * 2
    g = np.zeros((2, 2), F)
    g[1][1] = 7
    g += 1
    h = g.copy()
    h[0][0] = 100
    return "".join(map(fmt, [m, s, x * F(2) + y, (x * y).sum(), a, g, g[0][0], h[0][0]]))


def tensor_loops():
    a = np.array([[i * 4 + j for j in range(4)] for i in range(3)], F)
    b = np.array([[(i - j) * 0.5 for j in range(2)] for i in range(4)], F)
    c = a @ b
    t = c.T.copy()
    return "".join(map(fmt, [c, c.sum(), t, t[0][2] + t[1][2]]))


def tensor_mlp():
    x = np.array([[1, -2, 0.5]], F)
    w1 = np.array([[0.5, -1, 0.25, 2], [1, 0.5, -0.5, 0], [-2, 1, 1, 0.5]], F)
    b1 = np.array([[0.25, 0.25, -1, 0]], F)
    w2 = np.array([[1, -1], [0.5, 2], [-0.25, 1], [1, 0.5]], F)
    return fmt(np.maximum(x @ w1 + b1, 0) @ w2)


for fn in (tensor_basic, tensor_functions, tensor_loops, tensor_mlp):
    (out_dir / (fn.__name__ + ".out")).write_text(fn())
    print("wrote", fn.__name__ + ".out")
