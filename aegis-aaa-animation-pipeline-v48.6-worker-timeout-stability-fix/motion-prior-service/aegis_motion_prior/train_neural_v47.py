from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

from .losses_v42 import build_contact_aware_loss
from .model import require_torch
from .neural_model_v47 import (
    V47ModelConfig,
    build_condition_vocab,
    build_neural_overlay_model,
    condition_ids_for_clips,
)


def _velocity_loss(torch, pred, target, mask):
    if pred.shape[1] < 2:
        return torch.tensor(0.0, dtype=pred.dtype, device=pred.device)
    vp = pred[:, 1:] - pred[:, :-1]
    vt = target[:, 1:] - target[:, :-1]
    vm = mask[:, 1:]
    return (((vp - vt) ** 2).mean(dim=-1) * vm).sum() / vm.sum().clamp_min(1.0)


def train(dataset_dir: str, out_dir: str, epochs: int = 180, lr: float = 1e-4, hidden_dim: int = 384, layers: int = 4, heads: int = 6) -> None:
    torch = require_torch()
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)

    dataset_dir = Path(dataset_dir)
    x = np.load(dataset_dir / "motions.npy").astype("float32")
    mask = np.load(dataset_dir / "mask.npy").astype("float32")
    with (dataset_dir / "metadata.json").open("r", encoding="utf-8") as f:
        meta = json.load(f)

    vocab = build_condition_vocab(meta)
    config = V47ModelConfig(input_dim=int(x.shape[-1]), hidden_dim=hidden_dim, layers=layers, heads=heads)
    model = build_neural_overlay_model(config, vocab)

    xt = torch.from_numpy(x)
    mt = torch.from_numpy(mask)
    cond_ids = condition_ids_for_clips(meta, vocab)
    if cond_ids.shape[0] != xt.shape[0]:
        # Metadata and tensors should match. Fallback keeps training alive but logs the data issue.
        print(f"WARNING: metadata clips={cond_ids.shape[0]} but tensor batch={xt.shape[0]}; repeating first condition.")
        cond_ids = cond_ids[:1].repeat(xt.shape[0], 1)

    opt = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=1e-4)
    best_loss = float("inf")
    best_state = None

    for epoch in range(1, epochs + 1):
        model.train()
        sigma = max(0.006, 0.055 * (1.0 - epoch / max(1, epochs)))
        noisy = xt + torch.randn_like(xt) * sigma
        key_padding_mask = mt <= 0.0
        pred = model(noisy, cond_ids, key_padding_mask=key_padding_mask)
        recon_loss, parts = build_contact_aware_loss(torch, pred, xt, mt)
        vel_loss = _velocity_loss(torch, pred, xt, mt)
        loss = recon_loss + 0.22 * vel_loss

        opt.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        opt.step()

        loss_value = float(loss.detach().cpu())
        if loss_value < best_loss:
            best_loss = loss_value
            best_state = {k: v.detach().cpu().clone() for k, v in model.state_dict().items()}

        if epoch == 1 or epoch % 10 == 0 or epoch == epochs:
            print(f"epoch={epoch:04d} loss={loss_value:.6f} recon={float(recon_loss.detach().cpu()):.6f} vel={float(vel_loss.detach().cpu()):.6f} parts={parts}")

    if best_state is not None:
        model.load_state_dict(best_state)

    checkpoint = {
        "format": "aegis.neural_overlay_prior.v47",
        "model": model.state_dict(),
        "inputDim": int(x.shape[-1]),
        "config": config.__dict__,
        "conditionVocab": vocab,
        "metadata": meta,
        "metrics": {"bestLoss": best_loss, "epochs": epochs, "lr": lr},
    }
    checkpoint_path = out / "neural_overlay_prior_v47.pt"
    torch.save(checkpoint, checkpoint_path)
    (out / "neural_overlay_prior_v47.metrics.json").write_text(json.dumps(checkpoint["metrics"], indent=2), encoding="utf-8")
    print(f"Saved checkpoint: {checkpoint_path}")


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--dataset", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--epochs", type=int, default=180)
    p.add_argument("--lr", type=float, default=1e-4)
    p.add_argument("--hidden-dim", type=int, default=384)
    p.add_argument("--layers", type=int, default=4)
    p.add_argument("--heads", type=int, default=6)
    a = p.parse_args()
    train(a.dataset, a.out, a.epochs, a.lr, a.hidden_dim, a.layers, a.heads)


if __name__ == "__main__":
    main()
