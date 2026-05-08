from __future__ import annotations

from dataclasses import dataclass
from typing import Dict

from .model import require_torch


@dataclass
class V47ModelConfig:
    input_dim: int
    hidden_dim: int = 384
    layers: int = 4
    heads: int = 6
    cond_dim: int = 64
    dropout: float = 0.08


def build_condition_vocab(metadata: Dict) -> Dict[str, Dict[str, int]]:
    clips = metadata.get("clips", []) or []
    def vocab_for(key: str, fallback: str) -> Dict[str, int]:
        vals = sorted({str(c.get(key, fallback) or fallback) for c in clips} | {fallback})
        return {v: i for i, v in enumerate(vals)}
    return {
        "action": vocab_for("action", "soccer_kick_overlay"),
        "style": vocab_for("style", "active"),
        "dominantLeg": vocab_for("dominantLeg", "right"),
    }


def condition_ids_for_clips(metadata: Dict, vocab: Dict[str, Dict[str, int]]):
    torch = require_torch()
    clips = metadata.get("clips", []) or []
    rows = []
    for c in clips:
        rows.append([
            vocab["action"].get(str(c.get("action", "soccer_kick_overlay")), 0),
            vocab["style"].get(str(c.get("style", "active")), 0),
            vocab["dominantLeg"].get(str(c.get("dominantLeg", "right")), 0),
        ])
    if not rows:
        rows = [[0, 0, 0]]
    return torch.tensor(rows, dtype=torch.long)


def condition_ids_from_request(condition: Dict, vocab: Dict[str, Dict[str, int]]):
    torch = require_torch()
    return torch.tensor([[
        vocab["action"].get(str(condition.get("action", "soccer_kick_overlay")), 0),
        vocab["style"].get(str(condition.get("style", "active")), 0),
        vocab["dominantLeg"].get(str(condition.get("dominantLeg", "right")), 0),
    ]], dtype=torch.long)


def build_neural_overlay_model(config: V47ModelConfig, vocab: Dict[str, Dict[str, int]]):
    torch = require_torch()
    nn = torch.nn

    class AegisNeuralOverlayPriorV47(nn.Module):
        def __init__(self):
            super().__init__()
            self.config = config
            self.action_emb = nn.Embedding(max(1, len(vocab.get("action", {}))), config.cond_dim)
            self.style_emb = nn.Embedding(max(1, len(vocab.get("style", {}))), config.cond_dim)
            self.leg_emb = nn.Embedding(max(1, len(vocab.get("dominantLeg", {}))), config.cond_dim)
            self.cond_proj = nn.Sequential(
                nn.Linear(config.cond_dim * 3, config.hidden_dim),
                nn.GELU(),
                nn.LayerNorm(config.hidden_dim),
            )
            self.in_proj = nn.Linear(config.input_dim, config.hidden_dim)
            encoder_layer = nn.TransformerEncoderLayer(
                d_model=config.hidden_dim,
                nhead=config.heads,
                dim_feedforward=config.hidden_dim * 4,
                dropout=config.dropout,
                batch_first=True,
                activation="gelu",
                norm_first=True,
            )
            self.encoder = nn.TransformerEncoder(encoder_layer, num_layers=config.layers)
            self.out_norm = nn.LayerNorm(config.hidden_dim)
            self.delta_proj = nn.Linear(config.hidden_dim, config.input_dim)
            self.delta_scale = nn.Parameter(torch.tensor(0.25))

        def forward(self, x, cond_ids, key_padding_mask=None):
            cond = torch.cat([
                self.action_emb(cond_ids[:, 0]),
                self.style_emb(cond_ids[:, 1]),
                self.leg_emb(cond_ids[:, 2]),
            ], dim=-1)
            cond = self.cond_proj(cond)[:, None, :]
            h = self.in_proj(x) + cond
            h = self.encoder(h, src_key_padding_mask=key_padding_mask)
            delta = self.delta_proj(self.out_norm(h)) * self.delta_scale.tanh()
            return x + delta

    return AegisNeuralOverlayPriorV47()
