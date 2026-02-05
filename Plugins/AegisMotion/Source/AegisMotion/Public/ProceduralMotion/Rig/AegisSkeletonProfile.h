#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ProceduralMotion/Rig/Types/AegisBoneChainTypes.h"
#include "AegisSkeletonProfile.generated.h"

class USkeleton;

/**
 * A skeleton-specific profile describing named bone chains (spine, arm, leg, etc.).
 * Works with any imported skeleton (e.g., UE5 Manny/Quinn) by resolving bone names at runtime.
 */
UCLASS(BlueprintType)
class UAegisSkeletonProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    /** Optional: the skeleton this profile is authored for (helps validation and editor UX later). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Rig")
    TObjectPtr<USkeleton> TargetSkeleton = nullptr;

    /** Named chains (e.g., "Spine", "LeftArm"). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Rig")
    TArray<FAegisBoneChainDefinition> Chains;
};
