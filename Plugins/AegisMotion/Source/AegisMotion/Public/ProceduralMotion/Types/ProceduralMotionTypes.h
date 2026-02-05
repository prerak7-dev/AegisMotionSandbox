#pragma once

#include "CoreMinimal.h"
#include "ProceduralMotion/Types/AegisWorldContext.h"
#include "ProceduralMotionTypes.generated.h"


// Keep inputs deterministic and replicate-friendly.
// Avoid any random sources or engine-global state in evaluation.

USTRUCT(BlueprintType)
struct FAegisStatSample
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Name = NAME_None;

    // Recommended normalized range: 0..1
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Value = 0.f;
};

USTRUCT(BlueprintType)
struct FAegisProceduralMotionInput
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TimeSeconds = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName BoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseAngleDegrees = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FAegisStatSample> Stats;

    /** NEW: world-space context (optional but powerful) */
    FAegisWorldContext World;
};
