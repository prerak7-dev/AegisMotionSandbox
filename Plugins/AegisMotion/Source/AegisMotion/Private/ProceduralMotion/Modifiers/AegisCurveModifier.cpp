#include "ProceduralMotion/Modifiers/AegisCurveModifier.h"

FAegisCurveModifier::FAegisCurveModifier(const UCurveFloat* InCurve, float InWeight)
    : Curve(InCurve)
    , Weight(InWeight)
{

}

float FAegisCurveModifier::EvaluateDeltaDegrees(const FAegisProceduralMotionInput& Input) const
{
    if (!Curve.IsValid())
    {
        return 0.f;
    }

    const float CurveValue = Curve->GetFloatValue(Input.TimeSeconds);
    return CurveValue * Weight;
}
