from __future__ import annotations

import numpy as np

def resample_tensor(tensor: np.ndarray, out_frames: int) -> np.ndarray:
    """Time-warp/resample [T,D] tensor to out_frames with linear sampling.

    This is intentionally simple and deterministic. It is used after retrieval so retrieved
    motion clips can match the requested overlay duration.
    """
    x = np.asarray(tensor, dtype=np.float32)
    if x.shape[0] == out_frames:
        return x.copy()
    src_t = np.linspace(0.0, 1.0, x.shape[0], dtype=np.float32)
    dst_t = np.linspace(0.0, 1.0, out_frames, dtype=np.float32)
    out = np.empty((out_frames, x.shape[1]), dtype=np.float32)
    for d in range(x.shape[1]):
        out[:, d] = np.interp(dst_t, src_t, x[:, d])
    return out

def smooth_tensor(tensor: np.ndarray, passes: int = 1, strength: float = 0.25) -> np.ndarray:
    """Light temporal smoothing. Avoid overdoing this or it will remove strike impact."""
    x = np.asarray(tensor, dtype=np.float32).copy()
    if x.shape[0] < 3:
        return x
    for _ in range(max(0, passes)):
        prev = np.vstack([x[0:1], x[:-1]])
        nxt = np.vstack([x[1:], x[-1:]])
        x = (1.0 - strength) * x + strength * 0.5 * (prev + nxt)
    return x
