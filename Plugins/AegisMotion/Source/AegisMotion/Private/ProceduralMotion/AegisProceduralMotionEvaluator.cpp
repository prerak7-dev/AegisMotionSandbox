#include "ProceduralMotion/AegisProceduralMotionEvaluator.h"

void FAegisProceduralMotionEvaluator::AddModifier(TSharedRef<IAegisMotionModifier> Modifier)
{
    Modifiers.Add(Modifier);
}

float FAegisProceduralMotionEvaluator::EvaluateAngleDegrees(
    const FAegisProceduralMotionInput& Input
) const
{
    float Result = Input.BaseAngleDegrees;

    for (const TSharedRef<IAegisMotionModifier>& M : Modifiers)
    {
        Result += M->EvaluateDeltaDegrees(Input);
    }

    return Result;
}
