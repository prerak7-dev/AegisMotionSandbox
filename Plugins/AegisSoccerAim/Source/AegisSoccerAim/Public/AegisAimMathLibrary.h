#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AegisAimMathLibrary.generated.h"

UCLASS()
class AEGISSOCCERAIM_API UAegisAimMathLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Computes a signed yaw angle (degrees) from Source to Target in Source's space.
     * +Yaw = target is to the right, -Yaw = target is to the left (relative to Source forward).
     *
     * If bUseMeshForwardRight is true and Source is a Character, we use its Mesh component forward/right,
     * which is often more correct than Actor forward/right for Manny/Quinn.
     */
    UFUNCTION(BlueprintCallable, Category = "Aegis|SoccerAim")
    static bool ComputeAimYawDegrees_FromActors(
        AActor* SourceActor,
        AActor* TargetActor,
        float& OutYawDegrees,
        bool bUseMeshForwardRight = true,
        float MaxAbsYawDegrees = 180.f,
        bool bFlattenZ = true
    );
};
