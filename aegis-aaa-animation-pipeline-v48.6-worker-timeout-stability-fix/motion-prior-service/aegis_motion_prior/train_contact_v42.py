from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

from .losses_v42 import build_contact_aware_loss
from .model import build_model, require_torch

def train(dataset_dir: str, out_dir: str, epochs: int = 150, lr: float = 1e-4) -> None:
    torch = require_torch()
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)

    x = np.load(Path(dataset_dir) / "motions.npy").astype("float32")
    mask = np.load(Path(dataset_dir) / "mask.npy").astype("float32")
    with (Path(dataset_dir) / "metadata.json").open("r", encoding="utf-8") as f:
        meta = json.load(f)

    xt = torch.from_numpy(x)
    mt = torch.from_numpy(mask)
    model = build_model(input_dim=x.shape[-1])
    opt = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=1e-4)

    for epoch in range(1, epochs + 1):
        sigma = max(0.005, 0.04 * (1.0 - epoch / max(1, epochs)))
        noisy = xt + torch.randn_like(xt) * sigma
        pred = model(noisy)
        loss, parts = build_contact_aware_loss(torch, pred, xt, mt)
        opt.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        opt.step()
        if epoch == 1 or epoch % 10 == 0:
            print(f"epoch={epoch:04d} loss={float(loss):.6f} parts={parts}")

    torch.save({"model": model.state_dict(), "inputDim": int(x.shape[-1]), "metadata": meta},
               out / "motion_prior_contact_v42.pt")
    print(f"Saved {out / 'motion_prior_contact_v42.pt'}")

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--dataset", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--epochs", type=int, default=150)
    p.add_argument("--lr", type=float, default=1e-4)
    a = p.parse_args()
    train(a.dataset, a.out, a.epochs, a.lr)

if __name__ == "__main__":
    main()
