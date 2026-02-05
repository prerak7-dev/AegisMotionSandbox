#pragma once

#include "ProceduralMotion/Modifiers/AegisMotionModifier.h"
#include "ProceduralMotion/Types/ProceduralMotionStatUtils.h"

class FAegisStatScalarModifier final : public IAegisMotionModifier
{
public:
    // MaxDeltaDegrees is the peak delta applied when the stat is 1.0
    FAegisStatScalarModifier(FName InStatName, float InMaxDeltaDegrees);

    virtual float EvaluateDeltaDegrees(
        const FAegisProceduralMotionInput& Input
    ) const override;

    virtual FName GetDebugName() const override { return "StatScalarModifier"; }

private:
    FName StatName = NAME_None;
    float MaxDeltaDegrees = 0.f;
};
