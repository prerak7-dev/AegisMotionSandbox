#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"

#include "AegisAction/AegisProceduralActionAsset.h"
#include "AegisAction/AegisProceduralActionComponent.h"

#include "AnimNode_AegisProceduralMotionDriver.generated.h"

USTRUCT()
struct FAegisSocketBoneRuntimeCache
{
	GENERATED_BODY()

	FName BoneName = NAME_None;
	FCompactPoseBoneIndex BoneIndex = FCompactPoseBoneIndex(INDEX_NONE);
	float BoneWeight = 1.0f;
	FVector MaxRotationDegrees = FVector::ZeroVector;
	FVector MaxTranslationCm = FVector::ZeroVector;
	FAegisPerBoneMotionProfile MotionProfile;

	FRotator SmoothedRotDeg = FRotator::ZeroRotator;
	FVector SmoothedTransPS = FVector::ZeroVector;
	FQuat SmoothedQuat = FQuat::Identity;

	float SmoothedIKLockAlpha = 0.0f;
	float SmoothedPlantLockAlpha = 0.0f;
	bool bHasPlantLockTarget = false;
	FVector PlantLockTargetCS = FVector::ZeroVector;

	FRotator RotVelocityDeg = FRotator::ZeroRotator;
	FVector TransVelocityPS = FVector::ZeroVector;

	FTransform CapturedLocalTransform = FTransform::Identity;
};

USTRUCT()
struct FAegisActionChainRuntimeCache
{
	GENERATED_BODY()

	TArray<FCompactPoseBoneIndex> ChainBones;
	TArray<FAegisSocketBoneRuntimeCache> SocketBones;

	void Reset()
	{
		ChainBones.Reset();
		SocketBones.Reset();
	}
};

USTRUCT(BlueprintInternalUseOnly)
struct AEGISMOTION_API FAnimNode_AegisProceduralMotionDriver : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Action", meta = (PinShownByDefault))
	TObjectPtr<UAegisProceduralActionComponent> ActionComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Action", meta = (PinHiddenByDefault))
	TObjectPtr<UAegisProceduralActionAsset> ActionAssetOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Action", meta = (PinHiddenByDefault))
	float ActionTime01Override = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Action", meta = (PinHiddenByDefault))
	float ActionAlphaOverride = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Debug", meta = (PinShownByDefault))
	bool bDebugDraw = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Debug", meta = (PinShownByDefault))
	bool bDebugDrawJointNumbers = true;

public:
	FAnimNode_AegisProceduralMotionDriver();

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;

private:
	TArray<FAegisActionChainRuntimeCache> ActionChainCaches;
	TWeakObjectPtr<const UAegisProceduralActionAsset> CachedAsset;
	uint32 CachedAssetSignature = 0u;
	uint32 LastSeenActionInstanceId = 0u;
	bool bHasCapturedStartPose = false;

private:
	void ResolveActionState(const UAegisProceduralActionAsset*& OutAsset, float& OutTime01, float& OutAlpha) const;
	void ResetAllSmoothedStates();
	void CaptureStartPose(FComponentSpacePoseContext& Output);
	void EnsureCachesBuilt(const UAegisProceduralActionAsset* Asset, const FBoneContainer& RequiredBones);
	void BuildCacheForChain(FAegisActionChainRuntimeCache& Cache, const FAegisChainDef_Inline& Chain, const FBoneContainer& RequiredBones);

	static float HalfLifeAlpha(float DeltaSeconds, float HalfLifeSeconds);
	static float EvalAutomaticPhaseWeight(const FAegisActionPhaseBlendDef& Phase, float Time01);

	static double StepSpringDamperFloat(
		double Current,
		double Target,
		double& Velocity,
		double DeltaSeconds,
		const FAegisPerBoneMotionProfile& Profile,
		double MaxSpeed);

	void EvalSocketBoneTargetParentSpace(
		const FAegisChainDef_Inline& Chain,
		const FAegisSocketBoneRuntimeCache& SocketBone,
		float Time01,
		float ActionAlpha,
		EAegisActionPlaybackMode PlaybackMode,
		FVector& OutRotDegXYZ,
		FQuat& OutRawQuat,
		bool& bOutUseRawQuaternion,
		FVector& OutTransPS,
		EAegisBvhRotationOrder& OutRotationOrder,
		bool& bOutBypassSmoothing,
		bool& bOutReplaceLocalRotation,
		bool& bOutUseCapturedStartPoseBase,
		bool& bOutUseLiveBasePose,
		float& OutIKLockAlpha,
		float& OutPlantLockAlpha) const;

	void ApplySocketBoneChain(
		FComponentSpacePoseContext& Output,
		TArray<FBoneTransform>& OutBoneTransforms,
		const FAegisChainDef_Inline& Chain,
		FAegisActionChainRuntimeCache& Cache,
		float Time01,
		float ActionAlpha,
		float DT,
		const FAegisGeneratedFootLockSettings& FootLockSettings,
		EAegisActionPlaybackMode PlaybackMode);
};
