from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict, List

import numpy as np

from .model import require_torch


def _vocab(values: List[str]) -> Dict[str, int]:
    vals = sorted(set(values) | {"unknown"})
    return {v: i for i, v in enumerate(vals)}


def _one_hot(indexes: np.ndarray, size: int) -> np.ndarray:
    out = np.zeros((len(indexes), size), dtype=np.float32)
    out[np.arange(len(indexes)), np.clip(indexes, 0, size - 1)] = 1.0
    return out


def _build_conditions(meta: Dict, mask: np.ndarray) -> tuple[np.ndarray, Dict]:
    clips = meta.get("clips", []) or []
    style_vocab = _vocab([str(c.get("style", "unknown")) for c in clips])
    leg_vocab = _vocab([str(c.get("dominantLeg", "unknown")) for c in clips])
    action_vocab = _vocab([str(c.get("action", "unknown")) for c in clips])
    rows = []
    frame_clip_index = []
    for ci, c in enumerate(clips):
        valid = int(np.sum(mask[ci] > 0.5)) if ci < len(mask) else 0
        frame_clip_index.extend([ci] * valid)
        rows.append([
            action_vocab.get(str(c.get("action", "unknown")), action_vocab["unknown"]),
            style_vocab.get(str(c.get("style", "unknown")), style_vocab["unknown"]),
            leg_vocab.get(str(c.get("dominantLeg", "unknown")), leg_vocab["unknown"]),
        ])
    if not frame_clip_index:
        frame_clip_index = [0]
        rows = [[0, 0, 0]]
    rows = np.asarray(rows, dtype=np.int64)
    frame_clip_index = np.asarray(frame_clip_index, dtype=np.int64)
    cond = np.concatenate([
        _one_hot(rows[frame_clip_index, 0], len(action_vocab)),
        _one_hot(rows[frame_clip_index, 1], len(style_vocab)),
        _one_hot(rows[frame_clip_index, 2], len(leg_vocab)),
    ], axis=1)
    return cond.astype(np.float32), {"action": action_vocab, "style": style_vocab, "dominantLeg": leg_vocab}


def train(dataset_dir: str, out_dir: str, epochs: int = 40, lr: float = 1e-3, hidden_dim: int = 256, batch_size: int = 512, noise_sigma: float = 0.035) -> None:
    torch = require_torch()
    try:
        torch.set_num_threads(1)
        torch.set_num_interop_threads(1)
    except Exception:
        pass
    nn = torch.nn
    dataset_dir = Path(dataset_dir)
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    motions = np.load(dataset_dir / "motions.npy").astype("float32")
    mask = np.load(dataset_dir / "mask.npy").astype("float32")
    meta = json.loads((dataset_dir / "metadata.json").read_text(encoding="utf-8"))

    frames = []
    for i in range(motions.shape[0]):
        valid = int(np.sum(mask[i] > 0.5))
        if valid > 0:
            frames.append(motions[i, :valid])
    x = np.concatenate(frames, axis=0).astype("float32")
    cond, vocab = _build_conditions(meta, mask)
    if len(cond) != len(x):
        cond = np.resize(cond, (len(x), cond.shape[1])).astype("float32")

    input_dim = int(x.shape[1])
    cond_dim = int(cond.shape[1])

    class V48FrameDenoiser(nn.Module):
        def __init__(self):
            super().__init__()
            self.net = nn.Sequential(
                nn.Linear(input_dim + cond_dim, hidden_dim), nn.SiLU(), nn.LayerNorm(hidden_dim),
                nn.Linear(hidden_dim, hidden_dim), nn.SiLU(), nn.LayerNorm(hidden_dim),
                nn.Linear(hidden_dim, input_dim),
            )
            self.delta_scale = nn.Parameter(torch.tensor(0.20))
        def forward(self, y, c):
            delta = self.net(torch.cat([y, c], dim=-1)) * self.delta_scale.tanh()
            return y + delta

    model = V48FrameDenoiser()
    opt = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=1e-4)
    xt = torch.from_numpy(x)
    ct = torch.from_numpy(cond)
    n = xt.shape[0]
    best = float("inf")
    best_state = None
    for epoch in range(1, max(1, int(epochs)) + 1):
        perm = torch.randperm(n)
        total = 0.0
        seen = 0
        for start in range(0, n, batch_size):
            ids = perm[start:start + batch_size]
            target = xt[ids]
            c = ct[ids]
            noisy = target + torch.randn_like(target) * noise_sigma
            pred = model(noisy, c)
            loss = torch.mean((pred - target) ** 2)
            opt.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            opt.step()
            total += float(loss.detach().cpu()) * len(ids)
            seen += len(ids)
        avg = total / max(1, seen)
        if avg < best:
            best = avg
            best_state = {k: v.detach().cpu().clone() for k, v in model.state_dict().items()}
        if epoch == 1 or epoch % 5 == 0 or epoch == epochs:
            print(f"epoch={epoch:04d} frame_denoise_mse={avg:.7f}")
    if best_state is not None:
        model.load_state_dict(best_state)
    ckpt = {
        "format": "aegis.quaternion_kick_prior.v48.frame_denoiser",
        "model": model.state_dict(),
        "inputDim": input_dim,
        "condDim": cond_dim,
        "hiddenDim": hidden_dim,
        "conditionVocab": vocab,
        "metadata": meta,
        "metrics": {"bestFrameMse": best, "epochs": epochs, "lr": lr, "noiseSigma": noise_sigma},
    }
    ckpt_path = out / "quaternion_kick_prior_v48.pt"
    torch.save(ckpt, ckpt_path)
    (out / "quaternion_kick_prior_v48.metrics.json").write_text(json.dumps(ckpt["metrics"], indent=2), encoding="utf-8")
    print(f"Saved checkpoint: {ckpt_path}")


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--dataset", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--epochs", type=int, default=40)
    p.add_argument("--lr", type=float, default=1e-3)
    p.add_argument("--hidden-dim", type=int, default=256)
    p.add_argument("--batch-size", type=int, default=512)
    p.add_argument("--noise-sigma", type=float, default=0.035)
    a = p.parse_args()
    train(a.dataset, a.out, a.epochs, a.lr, a.hidden_dim, a.batch_size, a.noise_sigma)
    import sys, os
    sys.stdout.flush(); sys.stderr.flush()
    os._exit(0)


if __name__ == "__main__":
    main()
