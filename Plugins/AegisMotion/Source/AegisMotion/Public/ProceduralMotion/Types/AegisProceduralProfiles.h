#pragma once

#include "CoreMinimal.h"
#include "AegisProceduralProfiles.generated.h"

USTRUCT(BlueprintType)
struct AEGISMOTION_API FAegisLimbReachDampingProfile
{
    GENERATED_BODY()

    /** Name used to match a chain (e.g., Arm_L, Leg_R) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aegis|Profiles")
    FName ChainName = NAME_None;

    /** Scales the limb effect (0..1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aegis|Profiles", meta=(ClampMin="0", ClampMax="1"))
    float ReachAlpha = 0.5f;

    /** Additional half-life damping for limbs (seconds). 0 disables extra damping. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aegis|Profiles", meta=(ClampMin="0"))
    float DampingHalfLife = 0.12f;

    /** Soft clamp on limb angle (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aegis|Profiles", meta=(ClampMin="0"))
    float MaxAngleDegrees = 25.f;
};
