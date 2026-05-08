from __future__ import annotations

def require_torch():
    try:
        import torch
        return torch
    except Exception as exc:
        raise RuntimeError(
            "PyTorch is required for training/inference. Install torch for your CPU/CUDA setup, "
            "then rerun the command."
        ) from exc

def build_model(input_dim: int, hidden_dim: int = 384, layers: int = 4, heads: int = 6):
    torch = require_torch()
    nn = torch.nn

    class MotionPriorTransformer(nn.Module):
        def __init__(self):
            super().__init__()
            self.in_proj = nn.Linear(input_dim, hidden_dim)
            encoder_layer = nn.TransformerEncoderLayer(
                d_model=hidden_dim,
                nhead=heads,
                dim_feedforward=hidden_dim * 4,
                dropout=0.1,
                batch_first=True,
                activation="gelu",
            )
            self.encoder = nn.TransformerEncoder(encoder_layer, num_layers=layers)
            self.out_proj = nn.Linear(hidden_dim, input_dim)

        def forward(self, x, mask=None):
            h = self.in_proj(x)
            # mask currently reserved; sequence lengths are padded but small.
            h = self.encoder(h)
            return self.out_proj(h)

    return MotionPriorTransformer()
