#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_AegisLocalHingeBone.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct AEGISMOTION_API FAnimNode_AegisLocalHingeBone : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

    // Bone to rotate
    UPROPERTY(EditAnywhere, Category = "Aegis|Hinge")
    FBoneReference Bone;

    // Desired hinge angle in degrees (input pin)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Hinge", meta = (PinShownByDefault))
    float AngleDegrees = 0.f;

    // Clamp range in degrees
    UPROPERTY(EditAnywhere, Category = "Aegis|Hinge")
    float MinDegrees = -90.f;

    UPROPERTY(EditAnywhere, Category = "Aegis|Hinge")
    float MaxDegrees = 90.f;

    // Hinge axis in *parent space* (choose one)
    UPROPERTY(EditAnywhere, Category = "Aegis|Hinge")
    FVector ParentSpaceAxis = FVector(0.f, 1.f, 0.f); // Y by default

public:
    virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
    virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
    virtual void GatherDebugData(FNodeDebugData& DebugData) override;

private:
};
