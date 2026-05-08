from __future__ import annotations

import numpy as np

def normalize_quat(q: np.ndarray, eps: float = 1e-8) -> np.ndarray:
    norm = np.linalg.norm(q, axis=-1, keepdims=True)
    return q / np.maximum(norm, eps)

def ensure_quat_continuity(q: np.ndarray) -> np.ndarray:
    """Flip quaternion signs over time so adjacent samples stay in the same hemisphere."""
    out = np.array(q, dtype=np.float32, copy=True)
    for t in range(1, out.shape[0]):
        if np.dot(out[t - 1], out[t]) < 0.0:
            out[t] *= -1.0
    return out



def quat_conjugate(q: np.ndarray) -> np.ndarray:
    q = np.asarray(q, dtype=np.float32)
    out = np.array(q, dtype=np.float32, copy=True)
    out[..., 0:3] *= -1.0
    return out

def quat_inverse(q: np.ndarray, eps: float = 1e-8) -> np.ndarray:
    q = np.asarray(q, dtype=np.float32)
    conj = quat_conjugate(q)
    norm_sq = np.sum(q * q, axis=-1, keepdims=True)
    return conj / np.maximum(norm_sq, eps)

def quat_multiply(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    """Hamilton product for [x, y, z, w] quaternions with NumPy broadcasting."""
    a = np.asarray(a, dtype=np.float32)
    b = np.asarray(b, dtype=np.float32)
    ax, ay, az, aw = np.moveaxis(a, -1, 0)
    bx, by, bz, bw = np.moveaxis(b, -1, 0)
    out = np.stack([
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    ], axis=-1).astype(np.float32)
    return normalize_quat(out)

def quat_relative_to_first(q: np.ndarray) -> np.ndarray:
    """Convert an absolute local-rotation sequence to additive delta rotations.

    The Aegis overlay importer applies curves on top of the live/base pose.  Offline
    retarget exports store absolute target-skeleton local rotations, including rest-pose
    orientation offsets such as +/-90 or 180 degrees.  Training/exporting those absolute
    rotations as additive overlay curves makes the Unreal pose explode.  This helper
    removes the first-frame local orientation per bone, so frame zero is identity and
    the remaining frames describe only the animation delta from the clip start.
    """
    q = normalize_quat(ensure_quat_continuity(q))
    if q.shape[0] == 0:
        return q
    base_inv = quat_inverse(q[0:1])
    rel = quat_multiply(base_inv, q)
    return normalize_quat(ensure_quat_continuity(rel))

def quat_to_matrix(q: np.ndarray) -> np.ndarray:
    q = normalize_quat(q)
    x, y, z, w = np.moveaxis(q, -1, 0)
    xx, yy, zz = x*x, y*y, z*z
    xy, xz, yz = x*y, x*z, y*z
    wx, wy, wz = w*x, w*y, w*z
    m = np.empty(q.shape[:-1] + (3, 3), dtype=np.float32)
    m[..., 0, 0] = 1 - 2*(yy + zz)
    m[..., 0, 1] = 2*(xy - wz)
    m[..., 0, 2] = 2*(xz + wy)
    m[..., 1, 0] = 2*(xy + wz)
    m[..., 1, 1] = 1 - 2*(xx + zz)
    m[..., 1, 2] = 2*(yz - wx)
    m[..., 2, 0] = 2*(xz - wy)
    m[..., 2, 1] = 2*(yz + wx)
    m[..., 2, 2] = 1 - 2*(xx + yy)
    return m

def quat_to_6d(q: np.ndarray) -> np.ndarray:
    """6D rotation representation: first two matrix columns.

    Important: the 6D representation is [column0.xyz, column1.xyz].  A plain
    row-major reshape of m[..., :, :2] interleaves rows and columns, which
    corrupts rotations when sixd_to_quat reconstructs them. V47.2 fixes that
    packing bug explicitly.
    """
    m = quat_to_matrix(q)
    return np.concatenate([m[..., :, 0], m[..., :, 1]], axis=-1).astype(np.float32)

def matrix_to_quat(m: np.ndarray) -> np.ndarray:
    """Numerically stable enough for offline export."""
    m = np.asarray(m, dtype=np.float32)
    out = np.empty(m.shape[:-2] + (4,), dtype=np.float32)
    flat_m = m.reshape(-1, 3, 3)
    flat_q = out.reshape(-1, 4)
    for i, R in enumerate(flat_m):
        tr = float(np.trace(R))
        if tr > 0:
            S = np.sqrt(tr + 1.0) * 2.0
            w = 0.25 * S
            x = (R[2, 1] - R[1, 2]) / S
            y = (R[0, 2] - R[2, 0]) / S
            z = (R[1, 0] - R[0, 1]) / S
        else:
            idx = int(np.argmax([R[0, 0], R[1, 1], R[2, 2]]))
            if idx == 0:
                S = np.sqrt(max(1e-8, 1.0 + R[0, 0] - R[1, 1] - R[2, 2])) * 2.0
                w = (R[2, 1] - R[1, 2]) / S
                x = 0.25 * S
                y = (R[0, 1] + R[1, 0]) / S
                z = (R[0, 2] + R[2, 0]) / S
            elif idx == 1:
                S = np.sqrt(max(1e-8, 1.0 + R[1, 1] - R[0, 0] - R[2, 2])) * 2.0
                w = (R[0, 2] - R[2, 0]) / S
                x = (R[0, 1] + R[1, 0]) / S
                y = 0.25 * S
                z = (R[1, 2] + R[2, 1]) / S
            else:
                S = np.sqrt(max(1e-8, 1.0 + R[2, 2] - R[0, 0] - R[1, 1])) * 2.0
                w = (R[1, 0] - R[0, 1]) / S
                x = (R[0, 2] + R[2, 0]) / S
                y = (R[1, 2] + R[2, 1]) / S
                z = 0.25 * S
        flat_q[i] = np.array([x, y, z, w], dtype=np.float32)
    return normalize_quat(out)

def sixd_to_quat(x: np.ndarray) -> np.ndarray:
    """Convert 6D first-two-column rotation representation back to quaternion."""
    x = np.asarray(x, dtype=np.float32)
    a1 = x[..., 0:3]
    a2 = x[..., 3:6]
    b1 = a1 / np.maximum(np.linalg.norm(a1, axis=-1, keepdims=True), 1e-8)
    dot = np.sum(b1 * a2, axis=-1, keepdims=True)
    b2 = a2 - dot * b1
    b2 = b2 / np.maximum(np.linalg.norm(b2, axis=-1, keepdims=True), 1e-8)
    b3 = np.cross(b1, b2)
    m = np.stack([b1, b2, b3], axis=-1)
    return matrix_to_quat(m)
