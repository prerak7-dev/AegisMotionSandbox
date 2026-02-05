#pragma once

#include "ProceduralMotion/Modifiers/AegisMotionModifier.h"

class FAegisProceduralMotionEvaluator
{
public:
    void AddModifier(TSharedRef<IAegisMotionModifier> Modifier);

    // Returns final angle in degrees (Base + sum(deltas))
    float EvaluateAngleDegrees(
        const FAegisProceduralMotionInput& Input
    ) const;

    int32 GetModifierCount() const { return Modifiers.Num(); }

private:
    TArray<TSharedRef<IAegisMotionModifier>> Modifiers;
};
