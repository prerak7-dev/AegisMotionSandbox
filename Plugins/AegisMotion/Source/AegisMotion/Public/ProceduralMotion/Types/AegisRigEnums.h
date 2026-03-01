#pragma once

#include "CoreMinimal.h"
#include "AegisRigEnums.generated.h"

UENUM(BlueprintType)
enum class EAegisDistributionMode : uint8
{
    Uniform UMETA(DisplayName="Uniform"),
    RampToEnd UMETA(DisplayName="Ramp To End"),
    RampFromStart UMETA(DisplayName="Ramp From Start"),
    Pivoted UMETA(DisplayName="Pivoted")
};

/** Bitmask channels for procedural rotation driving */
UENUM(BlueprintType, meta=(Bitflags))
enum class EAegisDrivenChannels : uint8
{
    None  = 0 UMETA(Hidden),
    Pitch = 1 << 0,
    Yaw   = 1 << 1,
    Roll  = 1 << 2,
};

ENUM_CLASS_FLAGS(EAegisDrivenChannels);
