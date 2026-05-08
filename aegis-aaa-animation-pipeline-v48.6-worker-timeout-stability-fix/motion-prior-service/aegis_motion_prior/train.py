from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

from .model import build_model, require_torch

def train(dataset_dir: str, out_dir: str, epochs: int = 100, lr: float = 1e-4) -> None:
    torch = require_torch()
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)

    x = np.load(Path(dataset_dir) / "motions.npy").astype("float32")
    mask = np.load(Path(dataset_dir) / "mask.npy").astype("float32")
    with (Path(dataset_dir) / "metadata.json").open("r", encoding="utf-8") as f:
        meta = json.load(f)

    xt = torch.from_numpy(x)
    mt = torch.from_numpy(mask)[..., None]
    model = build_model(input_dim=x.shape[-1])
    opt = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=1e-4)

    for epoch in range(1, epochs + 1):
        model.train()
        noise = torch.randn_like(xt) * 0.025
        noisy = xt + noise
        pred = model(noisy)
        loss = (((pred - xt) ** 2) * mt).sum() / mt.sum().clamp_min(1.0)
        opt.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        opt.step()

        if epoch == 1 or epoch % 10 == 0:
            print(f"epoch={epoch:04d} loss={float(loss):.6f}")

    checkpoint = {
        "model": model.state_dict(),
        "inputDim": int(x.shape[-1]),
        "metadata": meta,
    }
    torch.save(checkpoint, out / "motion_prior_v37.pt")
    print(f"Saved checkpoint: {out / 'motion_prior_v37.pt'}")

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--lr", type=float, default=1e-4)
    args = parser.parse_args()
    train(args.dataset, args.out, args.epochs, args.lr)

if __name__ == "__main__":
    main()
