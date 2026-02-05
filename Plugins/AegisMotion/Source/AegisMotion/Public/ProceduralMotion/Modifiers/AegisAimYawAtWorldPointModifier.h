#pragma once

#include "ProceduralMotion/Modifiers/AegisMotionModifier.h"

/**
 * Computes a yaw delta (degrees) that aims the character toward
 * a world-space point (e.g. soccer ball).
 *
 * Yaw-only, component-space, deterministic.
 */
class FAegisAimYawAtWorldPointModifier final : public IAegisMotionModifier
{
public:
    FAegisAimYawAtWorldPointModifier(float InMaxYawDegrees = 60.f);

    virtual float EvaluateDeltaDegrees(
        const FAegisProceduralMotionInput& Input
    ) const override;

    virtual FName GetDebugName() const override
    {
        return "AimYawAtWorldPoint";
    }

private:
    /** Safety clamp for yaw (physical plausibility). */
    float MaxYawDegrees = 60.f;
};
