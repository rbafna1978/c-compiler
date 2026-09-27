#!/usr/bin/env python3
"""Regenerates the expected stdout (*.out) of the tensor and autodiff integration programs with NumPy.

Each function below is the NumPy twin of the tensor_*.c program of the same name. Values are
multiples of 0.25 so float32 results are exact and match the compiler bit for bit.
Gradient programs are checked against float64 finite differences of the NumPy loss, which is
independent of the compiler's derivative rules; those .out files come with a .tol tolerance.
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


def tensor_tiled_matmul():
    i, j = np.meshgrid(np.arange(64), np.arange(64), indexing="ij")
    a = ((i * 64 + j) * 0.001 - 2).astype(F)
    b = ((i - j) * 0.01).astype(F)
    c = a @ b
    return "".join(map(fmt, [c.sum(), c[0][0], c[63][63], c[10][20]]))


def tensor_mlp():
    x = np.array([[1, -2, 0.5]], F)
    w1 = np.array([[0.5, -1, 0.25, 2], [1, 0.5, -0.5, 0], [-2, 1, 1, 0.5]], F)
    b1 = np.array([[0.25, 0.25, -1, 0]], F)
    w2 = np.array([[1, -1], [0.5, 2], [-0.25, 1], [1, 0.5]], F)
    return fmt(np.maximum(x @ w1 + b1, 0) @ w2)


# ---- math builtins (compared with a tolerance: libm and NumPy differ in the last ulp) ----
def tensor_math():
    a = np.array([[0.5, 1], [2, -1]], F)
    k = np.array([1, 2, 3], np.int32)
    return "".join(map(fmt, [np.exp(F(1)), np.log(F(10)), np.tanh(F(0.5)), np.exp(a), np.tanh(a * 2),
                             np.log(k.astype(F)), np.exp(a).sum()]))


# ---- autodiff: oracle is float64 central finite differences of the NumPy loss -----------
def fd(f, args, i, h=1e-6):
    a = [np.array(x, np.float64) for x in args]
    g = np.zeros_like(a[i])
    for idx in np.ndindex(*a[i].shape):
        hi = [x.copy() for x in a]
        lo = [x.copy() for x in a]
        hi[i][idx] += h
        lo[i][idx] -= h
        g[idx] = (f(*hi) - f(*lo)) / (2 * h)
    return g


def grad_scalar():
    f = lambda x, y: x * y + np.exp(x) / y
    g = lambda a, b: -(a / b) ** 2 + np.tanh(a) - np.log(b)
    return "".join(fmt(fd(fn, args, i)) for fn, args, i in
                   [(f, (1.5, 2.0), 0), (f, (1.5, 2.0), 1), (g, (0.75, 2.0), 0), (g, (0.75, 2.0), 1)])


def grad_tensor():
    def loss(A, B, s):
        C = A @ B * s
        E = np.exp(C * 0.25) / (C * C + 1)
        return E.sum() + np.tanh(A.T * B).sum() - np.log(C * C + 2).sum()
    A = np.array([[0.5, -1, 0.25], [1, 0.5, -0.5]])
    B = np.array([[1, 0.5], [-0.5, 1], [0.25, -1]])
    return "".join(fmt(fd(loss, (A, B, 0.5), i)) for i in range(3))


def grad_train():
    x = np.array([[1, 2], [2, 1], [3, 0.5], [0, 1]], F)
    y = np.array([[0.5], [3.5], [6], [-0.5]], F)
    W, b, out = np.zeros((2, 1), F), F(0), []
    for step in range(201):
        err = x @ W + b - y
        if step % 50 == 0:
            out.append((err * err).sum())
        W = W - (x.T @ (2 * err)) * F(0.02)
        b = b - (2 * err).sum() * F(0.02)
    return "".join(map(fmt, out + [W, b]))


def grad_mlp():
    W1 = np.array([[0.5, -0.25, 0.75, 0.1], [-0.5, 0.25, 0.3, -0.6], [0.2, 0.4, -0.7, 0.35]])
    W2 = np.array([[0.5], [-0.75], [0.25], [0.6]])
    x = np.array([[1, -1, 0.5], [0.5, 2, -1]])
    y = np.array([[1], [-1]])
    loss = lambda W1, W2: ((np.tanh(x @ W1) @ W2 - y) ** 2).sum()
    out = fmt(fd(loss, (W1, W2), 0)) + fmt(fd(loss, (W1, W2), 1))
    W1, W2, x, y = W1.astype(F), W2.astype(F), x.astype(F), y.astype(F)  # training runs in float32
    losses = []
    for step in range(101):
        h = np.tanh(x @ W1)
        d = h @ W2 - y
        if step % 25 == 0:
            losses.append((d * d).sum())
        dh = (2 * d) @ W2.T
        W2, W1 = W2 - (h.T @ (2 * d)) * F(0.05), W1 - (x.T @ (dh * (1 - h * h))) * F(0.05)
    return out + "".join(map(fmt, losses + [W2]))


EXACT = (tensor_basic, tensor_functions, tensor_loops, tensor_mlp)
TOLERANT_TIGHT = (tensor_tiled_matmul,)  # float32 accumulation order differs slightly from NumPy's BLAS
TOLERANT = {tensor_math: "1e-5", grad_scalar: "1e-4", grad_tensor: "2e-3", grad_train: "1e-3", grad_mlp: "2e-3"}
for fn in EXACT + TOLERANT_TIGHT + tuple(TOLERANT):
    (out_dir / (fn.__name__ + ".out")).write_text(fn())
    if fn in TOLERANT_TIGHT:
        (out_dir / (fn.__name__ + ".tol")).write_text("2e-5\n")
    if fn in TOLERANT:  # relative tolerance for run.sh (float32 compiler vs float64 / NumPy oracle)
        (out_dir / (fn.__name__ + ".tol")).write_text(TOLERANT[fn] + "\n")
    print("wrote", fn.__name__ + ".out")
