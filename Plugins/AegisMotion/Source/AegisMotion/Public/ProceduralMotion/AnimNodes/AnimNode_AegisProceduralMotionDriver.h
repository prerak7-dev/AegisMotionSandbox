#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"

#include "AegisAction/AegisProceduralActionAsset.h"
#include "AegisAction/AegisProceduralActionComponent.h"

#include "AnimNode_AegisProceduralMotionDriver.generated.h"

struct FAegisActionChainRuntimeCache
{
	// Pivot chain bones (Start->End)
	TArray<FCompactPoseBoneIndex> ChainBones;
	FCompactPoseBoneIndex PivotBone = FCompactPoseBoneIndex(INDEX_NONE);

	// Pivot smoothing
	FRotator SmoothedPivotDelta = FRotator::ZeroRotator;

	// Hinge bones list (unique)
	TArray<FCompactPoseBoneIndex> HingeBones;

	// Smoothed hinge angles in degrees (index matches HingeBones order)
	TArray<float> SmoothedHingeAnglesDeg;

	void Reset()
	{
		ChainBones.Reset();
		PivotBone = FCompactPoseBoneIndex(INDEX_NONE);
		SmoothedPivotDelta = FRotator::ZeroRotator;
		HingeBones.Reset();
		SmoothedHingeAnglesDeg.Reset();
	}
};

USTRUCT(BlueprintInternalUseOnly)
struct AEGISMOTION_API FAnimNode_AegisProceduralMotionDriver : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	/** Primary workflow: Character BP calls StartAction(ActionAsset) on this component */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Action", meta = (PinShownByDefault))
	TObjectPtr<UAegisProceduralActionComponent> ActionComponent = nullptr;

	/** Fallback override for direct testing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Action", meta = (PinHiddenByDefault))
	TObjectPtr<UAegisProceduralActionAsset> ActionAssetOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Action", meta = (PinHiddenByDefault))
	float ActionTime01Override = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Action", meta = (PinHiddenByDefault))
	float ActionAlphaOverride = 1.f;

	/** Per-node debug draw toggle (in addition to console var aegis.Motion.DebugProceduralDriver) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Debug", meta = (PinShownByDefault))
	bool bDebugDraw = false;

public:
	FAnimNode_AegisProceduralMotionDriver();

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;

	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

	virtual void EvaluateSkeletalControl_AnyThread(
		FComponentSpacePoseContext& Output,
		TArray<FBoneTransform>& OutBoneTransforms) override;

private:
	TArray<FAegisActionChainRuntimeCache> ActionChainCaches;

private:
	// Action state resolution
	void ResolveActionState(const UAegisProceduralActionAsset*& OutAsset, float& OutTime01, float& OutAlpha) const;

	// Cache build
	void EnsureCachesBuilt(const UAegisProceduralActionAsset* Asset, const FBoneContainer& RequiredBones);
	void BuildCacheForChain(FAegisActionChainRuntimeCache& Cache, const FAegisChainDef_Inline& Chain, const FBoneContainer& RequiredBones);

	// Smoothing
	static float HalfLifeAlpha(float DeltaSeconds, float HalfLifeSeconds);

	// Pivot solve
	FRotator EvalPivotTargetDeg(const FAegisChainDef_Inline& Chain, float Time01, float ActionAlpha) const;
	float DistributionWeight(EAegisChainDistributionMode Mode, int32 Index, int32 Num, int32 PivotIndex) const;

	void ApplyPivotChain(
		FComponentSpacePoseContext& Output,
		TArray<FBoneTransform>& OutBoneTransforms,
		const FAegisChainDef_Inline& Chain,
		FAegisActionChainRuntimeCache& Cache,
		const FRotator& TargetDeg,
		float DT);

	// Hinge solve (true hinge: local rotation + parent propagation)
	void ApplyHingeChain(
		FComponentSpacePoseContext& Output,
		TArray<FBoneTransform>& OutBoneTransforms,
		const FAegisChainDef_Inline& Chain,
		FAegisActionChainRuntimeCache& Cache,
		float Time01,
		float ActionAlpha,
		float DT);
};
