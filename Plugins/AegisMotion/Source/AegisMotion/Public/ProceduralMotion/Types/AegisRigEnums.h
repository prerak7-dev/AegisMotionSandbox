#pragma once

#include "CoreMinimal.h"
#include "AegisRigEnums.generated.h"

UENUM(BlueprintType)
enum class EAegisDistributionMode : uint8
{
    /** Equal weight for each bone in the chain. */
    Uniform UMETA(DisplayName = "Uniform"),

    /** Weight increases along the chain (more motion toward the end bone). */
    RampToEnd UMETA(DisplayName = "Ramp To End"),

    /** Weight divided by pivot bone. */
    PivotToEnd UMETA(DisplayName = "Pivot To End")
};
