from __future__ import annotations

def build_contact_aware_loss(torch, pred, target, mask):
    """Contact-aware denoising loss scaffold.

    Tensor layout:
    - root: first 3 dims
    - rotations: middle dims
    - contacts: last 4 dims
    """
    m = mask[..., None]
    root_pred, root_tgt = pred[..., :3], target[..., :3]
    contact_pred, contact_tgt = pred[..., -4:], target[..., -4:]
    rot_pred, rot_tgt = pred[..., 3:-4], target[..., 3:-4]

    l_root = (((root_pred - root_tgt) ** 2) * m).sum() / m.sum().clamp_min(1.0)
    l_rot = (((rot_pred - rot_tgt) ** 2) * m).sum() / m.sum().clamp_min(1.0)
    l_contact = torch.nn.functional.binary_cross_entropy_with_logits(contact_pred, contact_tgt, reduction="none")
    l_contact = (l_contact * m).sum() / m.sum().clamp_min(1.0)

    vel_pred = pred[:, 1:] - pred[:, :-1]
    vel_tgt = target[:, 1:] - target[:, :-1]
    vm = mask[:, 1:, None] * mask[:, :-1, None]
    l_vel = (((vel_pred - vel_tgt) ** 2) * vm).sum() / vm.sum().clamp_min(1.0)

    acc_pred = vel_pred[:, 1:] - vel_pred[:, :-1]
    acc_tgt = vel_tgt[:, 1:] - vel_tgt[:, :-1]
    am = vm[:, 1:] * vm[:, :-1]
    l_acc = (((acc_pred - acc_tgt) ** 2) * am).sum() / am.sum().clamp_min(1.0)

    loss = l_rot + 0.5*l_root + 0.25*l_vel + 0.10*l_acc + 0.15*l_contact
    return loss, {
        "rot": float(l_rot.detach().cpu()),
        "root": float(l_root.detach().cpu()),
        "velocity": float(l_vel.detach().cpu()),
        "acceleration": float(l_acc.detach().cpu()),
        "contact": float(l_contact.detach().cpu()),
    }
