#include "ProceduralMotion/Modifiers/AegisStatScalarModifier.h"

FAegisStatScalarModifier::FAegisStatScalarModifier(FName InStatName, float InMaxDeltaDegrees)
    : StatName(InStatName)
    , MaxDeltaDegrees(InMaxDeltaDegrees)
{
}

float FAegisStatScalarModifier::EvaluateDeltaDegrees(const FAegisProceduralMotionInput& Input) const
{
    float Value01 = 0.f;
    if (!AegisMotionStatUtils::TryGetStat01(Input.Stats, StatName, Value01))
    {
        return 0.f;
    }

    return Value01 * MaxDeltaDegrees;
}
