#pragma once

#include "CoreMinimal.h"
#include "AegisMotionModel.generated.h"

USTRUCT(BlueprintType)
struct AEGISMOTION_API FAegisMotionModel
{
    GENERATED_BODY()

    /** World-space velocity (cm/s) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aegis|Motion")
    FVector VelocityWS = FVector::ZeroVector;

    /** World-space acceleration (cm/s^2) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aegis|Motion")
    FVector AccelerationWS = FVector::ZeroVector;

    /** Signed yaw turn rate (deg/s). Positive = turning right (clockwise looking down). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aegis|Motion")
    float TurnRateYawDegPerSec = 0.f;

    /** 1 when grounded, 0 when airborne. Can be smoothed before passing in. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aegis|Motion", meta=(ClampMin="0", ClampMax="1"))
    float GroundedAlpha = 1.f;

    /** 0..1 recoil amount from gameplay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aegis|Motion", meta=(ClampMin="0", ClampMax="1"))
    float Recoil01 = 0.f;

    /** 0..1 stride phase, optional (typically from anim curve). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aegis|Motion", meta=(ClampMin="0", ClampMax="1"))
    float StridePhase01 = 0.f;
};
