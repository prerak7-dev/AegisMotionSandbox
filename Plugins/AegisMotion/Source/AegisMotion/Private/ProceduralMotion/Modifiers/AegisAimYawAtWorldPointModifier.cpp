#include "ProceduralMotion/Modifiers/AegisAimYawAtWorldPointModifier.h"

#include "Math/UnrealMathUtility.h"

FAegisAimYawAtWorldPointModifier::FAegisAimYawAtWorldPointModifier(float InMaxYawDegrees)
    : MaxYawDegrees(InMaxYawDegrees)
{
}

float FAegisAimYawAtWorldPointModifier::EvaluateDeltaDegrees(
    const FAegisProceduralMotionInput& Input
) const
{
    if (!Input.World.IsValid())
    {
        return 0.f;
    }

    // Convert ball position into component space
    const FVector BallCS =
        Input.World.MeshComponentTransform.InverseTransformPosition(
            Input.World.BallWorldPosition
        );

    // Direction from anchor to ball (component space)
    FVector DirCS = BallCS - Input.World.AnchorComponentSpace;
    DirCS.Z = 0.f; // Yaw-only: flatten vertically

    if (DirCS.IsNearlyZero())
    {
        return 0.f;
    }

    DirCS.Normalize();

    // Forward vector in component space is +X
    const float YawRadians = FMath::Atan2(DirCS.Y, DirCS.X);
    const float YawDegrees = FMath::RadiansToDegrees(YawRadians);

    // Clamp to physical range
    return FMath::Clamp(YawDegrees, -MaxYawDegrees, MaxYawDegrees);
}
