#pragma once

#include "ProceduralMotion/Modifiers/AegisMotionModifier.h"
#include "Curves/CurveFloat.h"

// Adds a delta based on a float curve evaluated at Input.TimeSeconds.
// Curve output is interpreted as degrees before weighting.
class FAegisCurveModifier final : public IAegisMotionModifier
{
public:
    FAegisCurveModifier(const UCurveFloat* InCurve, float InWeight = 1.f);

    virtual float EvaluateDeltaDegrees(
        const FAegisProceduralMotionInput& Input
    ) const override;

    virtual FName GetDebugName() const override { return "CurveModifier"; }

private:
    // Weak pointer: we don't own the curve asset.
    TWeakObjectPtr<const UCurveFloat> Curve;
    float Weight = 1.f;
};
