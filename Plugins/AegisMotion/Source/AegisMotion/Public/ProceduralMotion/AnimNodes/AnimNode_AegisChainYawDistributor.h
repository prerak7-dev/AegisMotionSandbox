#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "BoneContainer.h"
#include "ProceduralMotion/Types/AegisRigEnums.h"
#include "AnimNode_AegisChainYawDistributor.generated.h"

/**
 * Distributes a yaw delta across a bone chain (StartBone -> EndBone).
 * Designed for soccer-style upper-body tracking (yaw-only).
 */
USTRUCT(BlueprintInternalUseOnly)
struct AEGISMOTION_API FAnimNode_AegisChainYawDistributor : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

public:
    /** Start of chain (ancestor) */
  /** Start of chain (ancestor). Edited in node details (bone picker). */
    UPROPERTY(EditAnywhere, Category = "Aegis|Chain")
    FBoneReference StartBone;

    /** End of chain (descendant). Edited in node details (bone picker). */
    UPROPERTY(EditAnywhere, Category = "Aegis|Chain")
    FBoneReference EndBone;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Chain")
    FName PivotBoneName = NAME_None;

    /** Total yaw delta (degrees) to distribute across the chain. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Aim", meta = (PinShownByDefault))
    float TotalYawDeltaDegrees = 0.f;

    /** Distribution weighting mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Chain")
    EAegisDistributionMode DistributionMode = EAegisDistributionMode::RampToEnd;

    /** Clamp for safety/comfort */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Aim")
    float MaxAbsYawDegrees = 60.f;

    /** If true, node outputs no transforms until a valid chain is resolved. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Debug")
    bool bRequireValidChain = true;

    /** Total roll/lean delta (degrees) to distribute across the chain. (+ = lean right) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Aim", meta = (PinShownByDefault))
    float TotalRollDeltaDegrees = 0.f;

    /** Clamp for roll safety */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Aim")
    float MaxAbsRollDegrees = 20.f;


public:
    // FAnimNode_SkeletalControlBase
    virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
    virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;

protected:
    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
    virtual void EvaluateSkeletalControl_AnyThread(
        FComponentSpacePoseContext& Output,
        TArray<FBoneTransform>& OutBoneTransforms
    ) override;

    virtual int32 GetLODThreshold() const override { return 0; }

private:
    /** Cached chain in compact pose indices */
    TArray<FCompactPoseBoneIndex> ChainCompact;

    /** Cached weights matching ChainCompact length */
    TArray<float> Weights;

    void RebuildChainCache(const FBoneContainer& RequiredBones);
    void RebuildWeights();
};
