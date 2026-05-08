# C++ Target Implementation

Implement in a future compile-tested pass:

```cpp
UAnimSequence::GetBoneTransform(...)
```

for each frame and each target bone, then export local-space quaternion curves.

Required output curves:

- pelvis.loc_x/y/z
- <bone>.rot_qx/qy/qz/qw
- foot_l.ik_lock_alpha
- foot_r.ik_lock_alpha
- foot_l.plant_lock_alpha
- foot_r.plant_lock_alpha

This markdown file is intentionally non-compiled so the V43 package remains safe to drop into the project.
