#pragma once

#include "CoreMinimal.h"
#include "AegisBoneChainTypes.generated.h"

/** How a chain should be defined. */
UENUM(BlueprintType)
enum class EAegisChainDefinitionMode : uint8
{
    /** Resolve chain by walking parents from EndBone up to StartBone. */
    StartEnd UMETA(DisplayName = "Start + End"),

    /** Use an explicit list of bones (must be contiguous in hierarchy). */
    ExplicitList UMETA(DisplayName = "Explicit Bone List")
};

USTRUCT(BlueprintType)
struct FAegisBoneChainDefinition
{
    GENERATED_BODY()

    /** Friendly chain name like "Spine", "LeftLeg". */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Rig")
    FName ChainName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Rig")
    EAegisChainDefinitionMode Mode = EAegisChainDefinitionMode::StartEnd;

    /** Used when Mode=StartEnd */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Rig", meta = (EditCondition = "Mode==EAegisChainDefinitionMode::StartEnd"))
    FName StartBone = NAME_None;

    /** Used when Mode=StartEnd */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Rig", meta = (EditCondition = "Mode==EAegisChainDefinitionMode::StartEnd"))
    FName EndBone = NAME_None;

    /** Used when Mode=ExplicitList */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Rig", meta = (EditCondition = "Mode==EAegisChainDefinitionMode::ExplicitList"))
    TArray<FName> Bones;

    /**
     * Optional weights (same length as resolved bones). If empty, we’ll generate defaults later.
     * Not used yet, but included now for forward compatibility (good portfolio signal).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Rig")
    TArray<float> Weights;
};

/** A resolved chain returned by the resolver. Pure runtime data. */
struct FAegisResolvedBoneChain
{
    FName ChainName = NAME_None;
    TArray<int32> BoneIndices;
    TArray<FName> BoneNames;

    bool IsValid() const { return BoneIndices.Num() > 0 && BoneIndices.Num() == BoneNames.Num(); }
};
