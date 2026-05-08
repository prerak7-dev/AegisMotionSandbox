#include "ProceduralMotion/AnimNodes/AnimNode_AegisProceduralMotionDriver.h"

#include "AegisMotionModule.h"
#include "Algo/Reverse.h"
#include "Animation/AnimInstanceProxy.h"
#include "Async/Async.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Crc.h"
#include "Templates/TypeHash.h"

static uint32 HashCombineFastU32(uint32 A, uint32 B)
{
	return HashCombineFast(A, B);
}

static uint32 HashFloatStable(float Value)
{
	return FCrc::MemCrc32(&Value, sizeof(float));
}

static uint32 HashNameStable(const FName& Name)
{
	const FString AsString = Name.ToString();
	return FCrc::StrCrc32(*AsString);
}

static uint32 HashObjectStable(const UObject* Obj)
{
	return PointerHash(Obj);
}

static uint32 CalculateActionAssetSignature(const UAegisProceduralActionAsset* Asset)
{
	if (!Asset)
	{
		return 0u;
	}

	uint32 Sig = 0u;
	Sig = HashCombineFastU32(Sig, HashFloatStable(Asset->DurationSeconds));
	Sig = HashCombineFastU32(Sig, HashObjectStable(Asset->SkeletalMesh.Get()));
	Sig = HashCombineFastU32(Sig, static_cast<uint32>(Asset->Chains.Num()));

	for (const FAegisChainDef_Inline& Chain : Asset->Chains)
	{
		Sig = HashCombineFastU32(Sig, HashNameStable(Chain.ChainName));
		Sig = HashCombineFastU32(Sig, HashNameStable(Chain.StartBone));
		Sig = HashCombineFastU32(Sig, HashNameStable(Chain.EndBone));
		Sig = HashCombineFastU32(Sig, static_cast<uint32>(Chain.SocketBones.Num()));
		Sig = HashCombineFastU32(Sig, static_cast<uint32>(Chain.Phases.Num()));

		for (const FAegisSocketBoneDef& SocketBone : Chain.SocketBones)
		{
			Sig = HashCombineFastU32(Sig, HashNameStable(SocketBone.BoneName));
			Sig = HashCombineFastU32(Sig, HashFloatStable(SocketBone.BoneWeight));
			Sig = HashCombineFastU32(Sig, HashFloatStable(SocketBone.MotionProfile.DampingHalfLife));
			Sig = HashCombineFastU32(Sig, HashFloatStable(SocketBone.MotionProfile.SpringStrength));
			Sig = HashCombineFastU32(Sig, HashFloatStable(SocketBone.MotionProfile.Inertia));
			Sig = HashCombineFastU32(Sig, HashFloatStable(SocketBone.MotionProfile.MaxRotationSpeedDegPerSec));
			Sig = HashCombineFastU32(Sig, HashFloatStable(SocketBone.MotionProfile.MaxTranslationSpeedCmPerSec));
		}

		for (const FAegisActionPhaseBlendDef& Phase : Chain.Phases)
		{
			Sig = HashCombineFastU32(Sig, HashNameStable(Phase.PhaseName));
			Sig = HashCombineFastU32(Sig, HashFloatStable(Phase.StartTime01));
			Sig = HashCombineFastU32(Sig, HashFloatStable(Phase.PeakTime01));
			Sig = HashCombineFastU32(Sig, HashFloatStable(Phase.EndTime01));
			Sig = HashCombineFastU32(Sig, HashFloatStable(Phase.EaseInExponent));
			Sig = HashCombineFastU32(Sig, HashFloatStable(Phase.EaseOutExponent));
			Sig = HashCombineFastU32(Sig, Phase.bUseAutomaticPhaseWeight ? 1u : 0u);
			Sig = HashCombineFastU32(Sig, static_cast<uint32>(Phase.BoneCurves.Num()));

			for (const FAegisSocketBonePhaseCurves& Slot : Phase.BoneCurves)
			{
				Sig = HashCombineFastU32(Sig, HashNameStable(Slot.BoneName));
			}
		}
	}

	return Sig;
}

static TAutoConsoleVariable<int32> CVarAegisDebugProceduralDriver(
	TEXT("aegis.Motion.DebugProceduralDriver"),
	0,
	TEXT("0=off, 1=log, 2=draw debug"),
	ECVF_Default);

static FORCEINLINE float CurveToSignedSmart(float V)
{
	if (V < 0.f || V > 1.f)
	{
		return V;
	}
	return (V - 0.5f) * 2.f;
}

static FORCEINLINE float CurveToUnsigned01(float V)
{
	return FMath::Clamp(V, 0.f, 1.f);
}

static FORCEINLINE bool ShouldDrawDebug(const FAnimNode_AegisProceduralMotionDriver& Node)
{
	return Node.bDebugDraw || CVarAegisDebugProceduralDriver.GetValueOnAnyThread() >= 2;
}

static FORCEINLINE bool PoseIndexToCompact(
	const FBoneContainer& Bones,
	int32 PoseIndex,
	FCompactPoseBoneIndex& OutCompact)
{
	if (PoseIndex == INDEX_NONE)
	{
		OutCompact = FCompactPoseBoneIndex(INDEX_NONE);
		return false;
	}

	const FMeshPoseBoneIndex MeshIdx(PoseIndex);
	OutCompact = Bones.MakeCompactPoseIndex(MeshIdx);
	return OutCompact != INDEX_NONE;
}

static void EnqueueDebugString(UWorld* World, const FVector& WorldPos, const FString& Text, const FColor& Color)
{
	if (!World)
	{
		return;
	}

	TWeakObjectPtr<UWorld> WeakWorld(World);
	AsyncTask(ENamedThreads::GameThread, [WeakWorld, WorldPos, Text, Color]()
		{
			if (UWorld* W = WeakWorld.Get())
			{
				DrawDebugString(W, WorldPos, Text, nullptr, Color, 0.f, false);
			}
		});
}

static void AnimDrawDebugLineSafe(FAnimInstanceProxy* Proxy, const FVector& A, const FVector& B, const FColor& Color, float Thickness)
{
	if (Proxy)
	{
		Proxy->AnimDrawDebugLine(A, B, Color, false, 0.f, Thickness, SDPG_Foreground);
	}
}

static void DrawDebugConeApprox(
	FAnimInstanceProxy* Proxy,
	const FVector& Origin,
	const FVector& Axis,
	float Length,
	float AngleDeg,
	const FColor& Color)
{
	if (!Proxy || Length <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector NAxis = Axis.GetSafeNormal();
	if (NAxis.IsNearlyZero())
	{
		return;
	}

	FVector BasisA = FVector::CrossProduct(NAxis, FVector::UpVector);
	if (BasisA.IsNearlyZero())
	{
		BasisA = FVector::CrossProduct(NAxis, FVector::RightVector);
	}
	BasisA = BasisA.GetSafeNormal();
	const FVector BasisB = FVector::CrossProduct(NAxis, BasisA).GetSafeNormal();

	const float Radius = Length * FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(AngleDeg, 1.f, 89.f)));
	const FVector Tip = Origin + NAxis * Length;

	constexpr int32 Segments = 12;
	FVector First = FVector::ZeroVector;
	FVector Prev = FVector::ZeroVector;

	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float Angle = (2.f * PI * static_cast<float>(Index)) / static_cast<float>(Segments);
		const FVector Rim = Tip + (BasisA * FMath::Cos(Angle) + BasisB * FMath::Sin(Angle)) * Radius;

		AnimDrawDebugLineSafe(Proxy, Origin, Rim, Color, 0.8f);

		if (Index > 0)
		{
			AnimDrawDebugLineSafe(Proxy, Prev, Rim, Color, 0.6f);
		}
		else
		{
			First = Rim;
		}

		Prev = Rim;
	}

	AnimDrawDebugLineSafe(Proxy, Prev, First, Color, 0.6f);
}


static FORCEINLINE bool IsValidCompactBoneIndex(const FCompactPoseBoneIndex BoneIndex)
{
	return BoneIndex != INDEX_NONE;
}

static bool IsDescendantOrSelfCompact(const FBoneContainer& BoneContainer, FCompactPoseBoneIndex Child, FCompactPoseBoneIndex Ancestor)
{
	if (!IsValidCompactBoneIndex(Child) || !IsValidCompactBoneIndex(Ancestor))
	{
		return false;
	}

	FCompactPoseBoneIndex Cursor = Child;
	while (Cursor != INDEX_NONE)
	{
		if (Cursor == Ancestor)
		{
			return true;
		}
		Cursor = BoneContainer.GetParentBoneIndex(Cursor);
	}

	return false;
}

static FVector ClampVectorMagnitude(const FVector& V, float MaxLength)
{
	if (MaxLength <= 0.0f)
	{
		return V;
	}

	const float Size = V.Size();
	if (Size <= MaxLength || Size <= KINDA_SMALL_NUMBER)
	{
		return V;
	}

	return V * (MaxLength / Size);
}

static bool IsGeneratedFootLockBoneName(const FName BoneName)
{
	// V30: generated contact curves are only allowed to drive the ankle/foot bones.
	// Earlier V28 code could accidentally treat any generated socket with a contact-like curve
	// as an IK target, which made upper-body/head tracks stretch when IK was enabled.
	return BoneName == FName(TEXT("foot_l")) || BoneName == FName(TEXT("foot_r"));
}

static bool SolveTwoBonePositionsCS(
	const FVector& Root,
	const FVector& Joint,
	const FVector& End,
	const FVector& DesiredEnd,
	FVector& OutJoint,
	FVector& OutEnd)
{
	const float UpperLen = FVector::Distance(Root, Joint);
	const float LowerLen = FVector::Distance(Joint, End);
	if (UpperLen <= KINDA_SMALL_NUMBER || LowerLen <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FVector RootToDesired = DesiredEnd - Root;
	float DesiredDist = RootToDesired.Size();
	if (DesiredDist <= KINDA_SMALL_NUMBER)
	{
		RootToDesired = (End - Root).GetSafeNormal();
		DesiredDist = KINDA_SMALL_NUMBER;
	}

	const FVector DesiredDir = RootToDesired.GetSafeNormal();
	const float MaxReach = FMath::Max(KINDA_SMALL_NUMBER, UpperLen + LowerLen - 0.05f);
	const float MinReach = FMath::Max(0.0f, FMath::Abs(UpperLen - LowerLen) + 0.05f);
	const float ClampedDist = FMath::Clamp(DesiredDist, MinReach, MaxReach);
	OutEnd = Root + DesiredDir * ClampedDist;

	const FVector OldRootToEnd = End - Root;
	const FVector OldDir = OldRootToEnd.GetSafeNormal();

	FVector OldPole = Joint - (Root + OldDir * FVector::DotProduct(Joint - Root, OldDir));
	if (OldPole.IsNearlyZero())
	{
		OldPole = FVector::CrossProduct(OldDir, FVector::UpVector);
		if (OldPole.IsNearlyZero())
		{
			OldPole = FVector::CrossProduct(OldDir, FVector::RightVector);
		}
	}

	FVector PoleDir = OldPole.GetSafeNormal();
	if (!OldDir.IsNearlyZero())
	{
		const FQuat SwingToDesired = FQuat::FindBetweenNormals(OldDir, DesiredDir);
		PoleDir = SwingToDesired.RotateVector(PoleDir);
	}

	// Re-orthogonalize pole to the desired root->end axis.
	PoleDir = (PoleDir - DesiredDir * FVector::DotProduct(PoleDir, DesiredDir)).GetSafeNormal();
	if (PoleDir.IsNearlyZero())
	{
		PoleDir = FVector::CrossProduct(DesiredDir, FVector::UpVector);
		if (PoleDir.IsNearlyZero())
		{
			PoleDir = FVector::CrossProduct(DesiredDir, FVector::RightVector);
		}
		PoleDir.Normalize();
	}

	const float Along = (FMath::Square(UpperLen) + FMath::Square(ClampedDist) - FMath::Square(LowerLen)) / (2.0f * ClampedDist);
	const float HeightSq = FMath::Max(0.0f, FMath::Square(UpperLen) - FMath::Square(Along));
	const float Height = FMath::Sqrt(HeightSq);

	OutJoint = Root + DesiredDir * Along + PoleDir * Height;
	return true;
}

static void ApplyGeneratedLegTwoBoneIKCS(
	FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms,
	FAegisActionChainRuntimeCache& Cache,
	FAegisSocketBoneRuntimeCache& FootSocket,
	float RawLockAlpha,
	const FAegisGeneratedFootLockSettings& FootLockSettings,
	FAnimInstanceProxy* DebugProxy)
{
	if (!FootLockSettings.bEnableGeneratedTwoBoneIK || !FootSocket.bHasPlantLockTarget || FootSocket.BoneIndex == INDEX_NONE)
	{
		return;
	}

	if (!IsGeneratedFootLockBoneName(FootSocket.BoneName))
	{
		return;
	}

	const FBoneContainer& BC = Output.Pose.GetPose().GetBoneContainer();
	const FCompactPoseBoneIndex FootIdx = FootSocket.BoneIndex;
	const FCompactPoseBoneIndex CalfIdx = BC.GetParentBoneIndex(FootIdx);
	if (CalfIdx == INDEX_NONE)
	{
		return;
	}

	const FCompactPoseBoneIndex ThighIdx = BC.GetParentBoneIndex(CalfIdx);
	if (ThighIdx == INDEX_NONE)
	{
		return;
	}

	const float ReleaseThreshold = FMath::Clamp(FootLockSettings.LockReleaseThreshold, 0.0f, 0.95f);
	const float NormalizedLock = FMath::Clamp((RawLockAlpha - ReleaseThreshold) / FMath::Max(KINDA_SMALL_NUMBER, 1.0f - ReleaseThreshold), 0.0f, 1.0f);
	const float IKWeight = FMath::Pow(NormalizedLock, FMath::Max(0.1f, FootLockSettings.TwoBoneIKAlphaPower));
	if (IKWeight <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FTransform ThighCS = Output.Pose.GetComponentSpaceTransform(ThighIdx);
	FTransform CalfCS = Output.Pose.GetComponentSpaceTransform(CalfIdx);
	FTransform FootCS = Output.Pose.GetComponentSpaceTransform(FootIdx);

	const FVector HipPos = ThighCS.GetLocation();
	const FVector KneePos = CalfCS.GetLocation();
	const FVector FootPos = FootCS.GetLocation();

	FVector DesiredFootPos = FMath::Lerp(FootPos, FootSocket.PlantLockTargetCS, IKWeight);
	// V31: bounded correction is critical. If this is too high, a generated contact curve can
	// pull the ankle beyond the authored stride and make the foot/chest appear stretched/jittery.
	DesiredFootPos = FootPos + ClampVectorMagnitude(DesiredFootPos - FootPos, FootLockSettings.MaxTwoBoneIKCorrectionCm);

	FVector NewKnee = KneePos;
	FVector NewFoot = DesiredFootPos;
	if (!SolveTwoBonePositionsCS(HipPos, KneePos, FootPos, DesiredFootPos, NewKnee, NewFoot))
	{
		return;
	}

	const FVector OldUpper = (KneePos - HipPos).GetSafeNormal();
	const FVector NewUpper = (NewKnee - HipPos).GetSafeNormal();
	const FVector OldLower = (FootPos - KneePos).GetSafeNormal();
	const FVector NewLower = (NewFoot - NewKnee).GetSafeNormal();

	if (OldUpper.IsNearlyZero() || NewUpper.IsNearlyZero() || OldLower.IsNearlyZero() || NewLower.IsNearlyZero())
	{
		return;
	}

	const FQuat UpperDelta = FQuat::FindBetweenNormals(OldUpper, NewUpper);
	const FQuat LowerDelta = FQuat::FindBetweenNormals(OldLower, NewLower);

	ThighCS.SetRotation((UpperDelta * ThighCS.GetRotation()).GetNormalized());
	CalfCS.SetLocation(NewKnee);
	CalfCS.SetRotation((LowerDelta * CalfCS.GetRotation()).GetNormalized());

	const FVector FootDelta = NewFoot - FootCS.GetLocation();
	FootCS.SetLocation(NewFoot);
	// V30: keep the authored/generated foot orientation. Rotating the foot by LowerDelta caused
	// visible ankle/ball stretching on generated clips because the JSON already contains the
	// intended foot snap/toe orientation. The IK pass should only solve ankle position.

	Output.Pose.SetComponentSpaceTransform(ThighIdx, ThighCS);
	Output.Pose.SetComponentSpaceTransform(CalfIdx, CalfCS);
	Output.Pose.SetComponentSpaceTransform(FootIdx, FootCS);
	OutBoneTransforms.Add(FBoneTransform(ThighIdx, ThighCS));
	OutBoneTransforms.Add(FBoneTransform(CalfIdx, CalfCS));
	OutBoneTransforms.Add(FBoneTransform(FootIdx, FootCS));

	// V31: do not translate descendants by default. In UE skeletal controls, a solved foot transform
	// already propagates to children through the hierarchy. Moving ball/toe sockets again created
	// the visible foot/toe stretch reported in V29/V30. Keep the old behavior behind an advanced flag.
	if (FootLockSettings.bTranslateFootDescendantsWithIK)
	{
		for (FAegisSocketBoneRuntimeCache& Candidate : Cache.SocketBones)
		{
			if (Candidate.BoneIndex == INDEX_NONE || Candidate.BoneIndex == FootIdx || Candidate.BoneIndex == CalfIdx || Candidate.BoneIndex == ThighIdx)
			{
				continue;
			}

			if (IsDescendantOrSelfCompact(BC, Candidate.BoneIndex, FootIdx))
			{
				FTransform DescendantCS = Output.Pose.GetComponentSpaceTransform(Candidate.BoneIndex);
				DescendantCS.AddToTranslation(FootDelta);
				Output.Pose.SetComponentSpaceTransform(Candidate.BoneIndex, DescendantCS);
				OutBoneTransforms.Add(FBoneTransform(Candidate.BoneIndex, DescendantCS));
			}
		}
	}

	if (DebugProxy && FootLockSettings.bDrawDebugTwoBoneIK)
	{
		AnimDrawDebugLineSafe(DebugProxy, HipPos, NewKnee, FColor::Purple, 2.0f);
		AnimDrawDebugLineSafe(DebugProxy, NewKnee, NewFoot, FColor::Magenta, 2.0f);
		AnimDrawDebugLineSafe(DebugProxy, FootSocket.PlantLockTargetCS, NewFoot, FColor::Green, 1.5f);
	}
}

FAnimNode_AegisProceduralMotionDriver::FAnimNode_AegisProceduralMotionDriver()
{
}

void FAnimNode_AegisProceduralMotionDriver::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);

	ActionChainCaches.Reset();
	CachedAsset.Reset();
	CachedAssetSignature = 0u;
	LastSeenActionInstanceId = 0u;
	bHasCapturedStartPose = false;
}

void FAnimNode_AegisProceduralMotionDriver::UpdateInternal(const FAnimationUpdateContext& Context)
{
	Super::UpdateInternal(Context);
}

bool FAnimNode_AegisProceduralMotionDriver::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	const UAegisProceduralActionAsset* Asset = nullptr;
	float Time01 = 0.f;
	float ActionAlpha = 0.f;
	ResolveActionState(Asset, Time01, ActionAlpha);
	return Asset != nullptr;
}

void FAnimNode_AegisProceduralMotionDriver::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	const UAegisProceduralActionAsset* Asset = nullptr;
	float Time01 = 0.f;
	float ActionAlpha = 0.f;
	ResolveActionState(Asset, Time01, ActionAlpha);

	if (Asset)
	{
		EnsureCachesBuilt(Asset, RequiredBones);
	}
}

void FAnimNode_AegisProceduralMotionDriver::ResolveActionState(
	const UAegisProceduralActionAsset*& OutAsset,
	float& OutTime01,
	float& OutAlpha) const
{
	OutAsset = nullptr;
	OutTime01 = 0.f;
	OutAlpha = 0.f;

	if (ActionComponent)
	{
		const UAegisProceduralActionAsset* ComponentAsset = ActionComponent->GetCurrentActionAsset();
		const bool bRunning = ActionComponent->IsActionActive();
		const bool bEditorPreview =
#if WITH_EDITOR
		(ActionComponent->bDebugScrubEnabled && ComponentAsset != nullptr);
#else
			false;
#endif

		if ((bRunning || bEditorPreview) && ComponentAsset != nullptr)
		{
			OutAsset = ComponentAsset;
			OutTime01 = FMath::Clamp(ActionComponent->GetActionTime01(), 0.f, 1.f);

			if (bEditorPreview)
			{
				OutAlpha = 1.f;
			}
			else
			{
				OutAlpha = FMath::Clamp(ActionComponent->GetActionAlpha(), 0.f, 1.f);
			}

			return;
		}

		if (ComponentAsset != nullptr)
		{
			OutAsset = ComponentAsset;
			OutTime01 = 0.f;
			OutAlpha = 0.f;
			return;
		}
	}

	if (ActionAssetOverride)
	{
		OutAsset = ActionAssetOverride;
		OutTime01 = FMath::Clamp(ActionTime01Override, 0.f, 1.f);
		OutAlpha = FMath::Clamp(ActionAlphaOverride, 0.f, 1.f);
	}
}

void FAnimNode_AegisProceduralMotionDriver::ResetAllSmoothedStates()
{
	for (FAegisActionChainRuntimeCache& Cache : ActionChainCaches)
	{
		for (FAegisSocketBoneRuntimeCache& SocketBone : Cache.SocketBones)
		{
			SocketBone.SmoothedRotDeg = FRotator::ZeroRotator;
			SocketBone.SmoothedTransPS = FVector::ZeroVector;
			SocketBone.SmoothedIKLockAlpha = 0.0f;
			SocketBone.SmoothedPlantLockAlpha = 0.0f;
			SocketBone.bHasPlantLockTarget = false;
			SocketBone.PlantLockTargetCS = FVector::ZeroVector;
			SocketBone.RotVelocityDeg = FRotator::ZeroRotator;
			SocketBone.TransVelocityPS = FVector::ZeroVector;
			SocketBone.CapturedLocalTransform = FTransform::Identity;
		}
	}

	bHasCapturedStartPose = false;
}

void FAnimNode_AegisProceduralMotionDriver::CaptureStartPose(FComponentSpacePoseContext& Output)
{
	for (FAegisActionChainRuntimeCache& Cache : ActionChainCaches)
	{
		for (FAegisSocketBoneRuntimeCache& SocketBone : Cache.SocketBones)
		{
			if (SocketBone.BoneIndex == INDEX_NONE)
			{
				continue;
			}

			SocketBone.CapturedLocalTransform = Output.Pose.GetLocalSpaceTransform(SocketBone.BoneIndex);
			SocketBone.SmoothedRotDeg = FRotator::ZeroRotator;
			SocketBone.SmoothedTransPS = FVector::ZeroVector;
			SocketBone.SmoothedIKLockAlpha = 0.0f;
			SocketBone.SmoothedPlantLockAlpha = 0.0f;
			SocketBone.bHasPlantLockTarget = false;
			SocketBone.PlantLockTargetCS = FVector::ZeroVector;
			SocketBone.RotVelocityDeg = FRotator::ZeroRotator;
			SocketBone.TransVelocityPS = FVector::ZeroVector;
		}
	}

	bHasCapturedStartPose = true;
}

void FAnimNode_AegisProceduralMotionDriver::EnsureCachesBuilt(const UAegisProceduralActionAsset* Asset, const FBoneContainer& RequiredBones)
{
	if (!Asset)
	{
		return;
	}

	const uint32 NewSignature = CalculateActionAssetSignature(Asset);
	const bool bNeedFullRebuild =
		(CachedAsset.Get() != Asset) ||
		(CachedAssetSignature != NewSignature) ||
		(ActionChainCaches.Num() != Asset->Chains.Num());

	if (bNeedFullRebuild)
	{
		ActionChainCaches.SetNum(Asset->Chains.Num());

		for (FAegisActionChainRuntimeCache& Cache : ActionChainCaches)
		{
			Cache.Reset();
		}

		CachedAsset = Asset;
		CachedAssetSignature = NewSignature;
		bHasCapturedStartPose = false;
	}

	for (int32 Index = 0; Index < Asset->Chains.Num(); ++Index)
	{
		if (ActionChainCaches[Index].ChainBones.Num() == 0 && ActionChainCaches[Index].SocketBones.Num() == 0)
		{
			BuildCacheForChain(ActionChainCaches[Index], Asset->Chains[Index], RequiredBones);
		}
	}
}

void FAnimNode_AegisProceduralMotionDriver::BuildCacheForChain(
	FAegisActionChainRuntimeCache& Cache,
	const FAegisChainDef_Inline& Chain,
	const FBoneContainer& RequiredBones)
{
	Cache.Reset();

	if (Chain.StartBone.IsNone() || Chain.EndBone.IsNone())
	{
		return;
	}

	const int32 StartPoseIndex = RequiredBones.GetPoseBoneIndexForBoneName(Chain.StartBone);
	const int32 EndPoseIndex = RequiredBones.GetPoseBoneIndexForBoneName(Chain.EndBone);

	FCompactPoseBoneIndex StartIdx(INDEX_NONE);
	FCompactPoseBoneIndex EndIdx(INDEX_NONE);

	if (!PoseIndexToCompact(RequiredBones, StartPoseIndex, StartIdx) ||
		!PoseIndexToCompact(RequiredBones, EndPoseIndex, EndIdx))
	{
		return;
	}

	TArray<FCompactPoseBoneIndex> Temp;
	FCompactPoseBoneIndex Cur = EndIdx;
	int32 Safety = 0;

	while (Cur != INDEX_NONE && Safety++ < 256)
	{
		Temp.Add(Cur);

		if (Cur == StartIdx)
		{
			break;
		}

		Cur = RequiredBones.GetParentBoneIndex(Cur);
	}

	if (Temp.Num() == 0 || Temp.Last() != StartIdx)
	{
		UE_LOG(
			LogAegisMotion,
			Warning,
			TEXT("[AegisProceduralMotionDriver] Invalid chain path for '%s' (%s -> %s)"),
			*Chain.ChainName.ToString(),
			*Chain.StartBone.ToString(),
			*Chain.EndBone.ToString());
		return;
	}

	Algo::Reverse(Temp);
	Cache.ChainBones = Temp;

	TMap<int32, FAegisSocketBoneRuntimeCache> SocketMap;

	for (const FAegisSocketBoneDef& Def : Chain.SocketBones)
	{
		if (Def.BoneName.IsNone())
		{
			continue;
		}

		const int32 PoseIndex = RequiredBones.GetPoseBoneIndexForBoneName(Def.BoneName);
		FCompactPoseBoneIndex CompactIndex(INDEX_NONE);

		if (!PoseIndexToCompact(RequiredBones, PoseIndex, CompactIndex))
		{
			continue;
		}

		FAegisSocketBoneRuntimeCache RuntimeDef;
		RuntimeDef.BoneName = Def.BoneName;
		RuntimeDef.BoneIndex = CompactIndex;
		RuntimeDef.BoneWeight = Def.BoneWeight;
		RuntimeDef.MaxRotationDegrees = Def.Limits.MaxRotationDegrees;
		RuntimeDef.MaxTranslationCm = Def.Limits.MaxTranslationCm;
		RuntimeDef.MotionProfile = Def.MotionProfile;

		SocketMap.Add(CompactIndex.GetInt(), RuntimeDef);
	}

	for (const FCompactPoseBoneIndex& BoneIdx : Cache.ChainBones)
	{
		if (FAegisSocketBoneRuntimeCache* Found = SocketMap.Find(BoneIdx.GetInt()))
		{
			Cache.SocketBones.Add(*Found);
			SocketMap.Remove(BoneIdx.GetInt());
		}
	}

	if (SocketMap.Num() > 0)
	{
		TArray<FAegisSocketBoneRuntimeCache> Remaining;
		SocketMap.GenerateValueArray(Remaining);

		Remaining.Sort([&RequiredBones](const FAegisSocketBoneRuntimeCache& A, const FAegisSocketBoneRuntimeCache& B)
			{
				auto Depth = [&RequiredBones](FCompactPoseBoneIndex Bone)
					{
						int32 D = 0;
						FCompactPoseBoneIndex Cursor = Bone;

						while (Cursor != INDEX_NONE && D < 512)
						{
							Cursor = RequiredBones.GetParentBoneIndex(Cursor);
							++D;
						}

						return D;
					};

				return Depth(A.BoneIndex) < Depth(B.BoneIndex);
			});

		Cache.SocketBones.Append(Remaining);
	}
}

float FAnimNode_AegisProceduralMotionDriver::HalfLifeAlpha(float DeltaSeconds, float HalfLifeSeconds)
{
	if (HalfLifeSeconds <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}

	return 1.f - FMath::Pow(0.5f, DeltaSeconds / HalfLifeSeconds);
}

float FAnimNode_AegisProceduralMotionDriver::EvalAutomaticPhaseWeight(const FAegisActionPhaseBlendDef& Phase, float Time01)
{
	if (!Phase.bUseAutomaticPhaseWeight)
	{
		return 1.f;
	}

	const float T = FMath::Clamp(Time01, 0.f, 1.f);
	const float Start = FMath::Clamp(Phase.StartTime01, 0.f, 1.f);
	const float Peak = FMath::Clamp(Phase.PeakTime01, Start, 1.f);
	const float End = FMath::Clamp(Phase.EndTime01, Peak, 1.f);

	if (T < Start || T > End)
	{
		return 0.f;
	}

	if (End <= Start)
	{
		return 1.f;
	}

	if (T <= Peak)
	{
		const float Den = FMath::Max(KINDA_SMALL_NUMBER, Peak - Start);
		return FMath::Pow(FMath::Clamp((T - Start) / Den, 0.f, 1.f), Phase.EaseInExponent);
	}

	const float Den = FMath::Max(KINDA_SMALL_NUMBER, End - Peak);
	return FMath::Pow(FMath::Clamp((End - T) / Den, 0.f, 1.f), Phase.EaseOutExponent);
}

double FAnimNode_AegisProceduralMotionDriver::StepSpringDamperFloat(
	double Current,
	double Target,
	double& Velocity,
	double DeltaSeconds,
	const FAegisPerBoneMotionProfile& Profile,
	double MaxSpeed)
{
	if (DeltaSeconds <= static_cast<double>(KINDA_SMALL_NUMBER))
	{
		return Current;
	}

	const double Inertia = static_cast<double>(FMath::Clamp(Profile.Inertia, 0.f, 1.f));
	const double InertialTarget = FMath::Lerp(Current, Target, 1.0 - Inertia);

	const double HalfLifeBlend = static_cast<double>(HalfLifeAlpha(static_cast<float>(DeltaSeconds), Profile.DampingHalfLife));
	const double DampedTarget = FMath::Lerp(Current, InertialTarget, HalfLifeBlend);

	const double Spring = static_cast<double>(FMath::Max(0.f, Profile.SpringStrength));
	if (Spring <= static_cast<double>(KINDA_SMALL_NUMBER))
	{
		Velocity = 0.0;
		return DampedTarget;
	}

	const double CritDamping = 2.0 * FMath::Sqrt(Spring);
	const double Accel = (DampedTarget - Current) * Spring - Velocity * CritDamping;

	Velocity += Accel * DeltaSeconds;

	if (MaxSpeed > 0.0)
	{
		Velocity = FMath::Clamp(Velocity, -MaxSpeed, MaxSpeed);
	}

	double Result = Current + Velocity * DeltaSeconds;

	const double OvershootPad = FMath::Max(1.0, FMath::Abs(Target - Current));
	const double MinBound = FMath::Min(Current, Target) - OvershootPad;
	const double MaxBound = FMath::Max(Current, Target) + OvershootPad;
	Result = FMath::Clamp(Result, MinBound, MaxBound);

	return Result;
}


static void ApplyAegisOrderedEuler(FTransform& BoneLS, const FVector& RotDegXYZ, EAegisBvhRotationOrder RotationOrder)
{
	auto ApplyAxisRot = [&BoneLS](const FVector& AxisLS, double Deg)
	{
		if (FMath::IsNearlyZero(Deg, 0.0001))
		{
			return;
		}

		const FVector SafeAxisLS = AxisLS.GetSafeNormal();
		if (SafeAxisLS.IsNearlyZero())
		{
			return;
		}

		const FQuat DeltaQuat(SafeAxisLS, FMath::DegreesToRadians(Deg));
		BoneLS.SetRotation((DeltaQuat * BoneLS.GetRotation()).GetNormalized());
	};

	auto ApplyX = [&]() { ApplyAxisRot(FVector(1.0, 0.0, 0.0), RotDegXYZ.X); };
	auto ApplyY = [&]() { ApplyAxisRot(FVector(0.0, 1.0, 0.0), RotDegXYZ.Y); };
	auto ApplyZ = [&]() { ApplyAxisRot(FVector(0.0, 0.0, 1.0), RotDegXYZ.Z); };

	switch (RotationOrder)
	{
	case EAegisBvhRotationOrder::XYZ:
		ApplyX(); ApplyY(); ApplyZ();
		break;
	case EAegisBvhRotationOrder::XZY:
		ApplyX(); ApplyZ(); ApplyY();
		break;
	case EAegisBvhRotationOrder::YXZ:
		ApplyY(); ApplyX(); ApplyZ();
		break;
	case EAegisBvhRotationOrder::YZX:
		ApplyY(); ApplyZ(); ApplyX();
		break;
	case EAegisBvhRotationOrder::ZXY:
		ApplyZ(); ApplyX(); ApplyY();
		break;
	case EAegisBvhRotationOrder::ZYX:
		ApplyZ(); ApplyY(); ApplyX();
		break;
	default:
		ApplyZ(); ApplyX(); ApplyY();
		break;
	}
}

void FAnimNode_AegisProceduralMotionDriver::EvalSocketBoneTargetParentSpace(
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
	float& OutPlantLockAlpha) const
{
	const float T = FMath::Clamp(Time01, 0.f, 1.f);
	const float A = FMath::Clamp(ActionAlpha, 0.f, 1.f);

	float ChainAlpha = 1.f;
	if (Chain.ChainAlpha01)
	{
		ChainAlpha = CurveToUnsigned01(Chain.ChainAlpha01->GetFloatValue(T));
	}
	ChainAlpha = FMath::Clamp(ChainAlpha * Chain.ChainAlphaMultiplier, 0.f, 1.f);

	TArray<float> RawWeights;
	RawWeights.Reserve(Chain.Phases.Num());

	float TotalWeight = 0.f;
	for (const FAegisActionPhaseBlendDef& Phase : Chain.Phases)
	{
		float W = Phase.bUseAutomaticPhaseWeight ? EvalAutomaticPhaseWeight(Phase, T) : 1.0f;

		if (Phase.PhaseAlpha01)
		{
			W *= CurveToUnsigned01(Phase.PhaseAlpha01->GetFloatValue(T));
		}

		RawWeights.Add(W);
		TotalWeight += W;
	}

	FVector RotDegXYZ = FVector::ZeroVector;
	FVector Trans = FVector::ZeroVector;
	FVector4 RawQuatAccum(0.f, 0.f, 0.f, 0.f);
	float RawQuatWeight = 0.f;

	OutRotationOrder = EAegisBvhRotationOrder::ZXY;
	bOutBypassSmoothing = false;
	bOutReplaceLocalRotation = false;
	bOutUseCapturedStartPoseBase = false;
	bOutUseLiveBasePose = false;
	OutIKLockAlpha = 0.0f;
	OutPlantLockAlpha = 0.0f;
	bOutUseRawQuaternion = false;
	OutRawQuat = FQuat::Identity;

	for (int32 PhaseIndex = 0; PhaseIndex < Chain.Phases.Num(); ++PhaseIndex)
	{
		const float PhaseWeight = RawWeights.IsValidIndex(PhaseIndex) ? RawWeights[PhaseIndex] : 0.f;
		if (PhaseWeight <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float NormalizedWeight = (TotalWeight > KINDA_SMALL_NUMBER) ? (PhaseWeight / TotalWeight) : 0.f;
		const FAegisActionPhaseBlendDef& Phase = Chain.Phases[PhaseIndex];

		const FAegisSocketBonePhaseCurves* FoundSlot = nullptr;
		for (const FAegisSocketBonePhaseCurves& Slot : Phase.BoneCurves)
		{
			if (Slot.BoneName == SocketBone.BoneName)
			{
				FoundSlot = &Slot;
				break;
			}
		}

		if (!FoundSlot)
		{
			continue;
		}

		float SlotAlpha = 1.f;
		if (FoundSlot->Alpha01)
		{
			SlotAlpha = CurveToUnsigned01(FoundSlot->Alpha01->GetFloatValue(T));
		}

		const float W = NormalizedWeight * ChainAlpha * A * SocketBone.BoneWeight * SlotAlpha;
		if (W <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float RotMul = FoundSlot->RotationMultiplier;
		const float PosMul = FoundSlot->TranslationMultiplier;

		const bool bGeneratedNativeQuaternion = FoundSlot->RotationValueSpace == EAegisCurveValueSpace::GeneratedNativeQuaternionLocal;
		const bool bRawQuaternion = FoundSlot->RotationValueSpace == EAegisCurveValueSpace::RawMocapQuaternionLocal || bGeneratedNativeQuaternion;
		const bool bRawRotation = FoundSlot->RotationValueSpace == EAegisCurveValueSpace::RawBvhLocal || bRawQuaternion;
		const bool bRawTranslation = FoundSlot->TranslationValueSpace == EAegisCurveValueSpace::RawBvhLocal;

		if (bRawRotation)
		{
			OutRotationOrder = FoundSlot->RotationOrder;
			bOutBypassSmoothing |= FoundSlot->bBypassSmoothingForRawBvh;
			bOutReplaceLocalRotation = true;

			// V47.5: LiveBaseGeneratedOverlay can now be authored either as quaternion
			// reference curves or as scalar additive rot_x/rot_y/rot_z degree curves.
			// The V47.4 importer correctly stores scalar generated overlays as raw degree
			// curves so their values are not multiplied by MaxRotationDegrees, but the
			// runtime only enabled the live-base pose path for quaternion slots. Scalar
			// overlays therefore started from the reference pose instead of the current
			// AnimGraph source pose, making strong kick curves look weak/incorrect in PIE.
			const bool bLiveBaseGeneratedOverlay = (PlaybackMode == EAegisActionPlaybackMode::LiveBaseGeneratedOverlay);
			if (bLiveBaseGeneratedOverlay)
			{
				bOutUseLiveBasePose = true;
				bOutUseCapturedStartPoseBase = false;
			}
			else if (bGeneratedNativeQuaternion)
			{
				bOutUseLiveBasePose = false;
				bOutUseCapturedStartPoseBase = true;
			}
		}
		if (bRawTranslation)
		{
			bOutBypassSmoothing |= FoundSlot->bBypassSmoothingForRawBvh;
		}

		auto EvalRotationAxisDeg = [&](const TObjectPtr<UCurveFloat>& Curve, float MaxDegrees) -> float
		{
			if (!Curve)
			{
				return 0.f;
			}

			const float V = Curve->GetFloatValue(T);
			return bRawRotation ? V : CurveToSignedSmart(V) * MaxDegrees;
		};

		auto EvalTranslationAxisCm = [&](const TObjectPtr<UCurveFloat>& Curve, float MaxCm) -> float
		{
			if (!Curve)
			{
				return 0.f;
			}

			const float V = Curve->GetFloatValue(T);
			return bRawTranslation ? V : CurveToSignedSmart(V) * MaxCm;
		};

		auto EvalContactAlpha = [&](const TObjectPtr<UCurveFloat>& Curve) -> float
		{
			return Curve ? CurveToUnsigned01(Curve->GetFloatValue(T)) : 0.0f;
		};

		if (bRawQuaternion)
		{
			const float Qx = FoundSlot->RotX01 ? FoundSlot->RotX01->GetFloatValue(T) : 0.f;
			const float Qy = FoundSlot->RotY01 ? FoundSlot->RotY01->GetFloatValue(T) : 0.f;
			const float Qz = FoundSlot->RotZ01 ? FoundSlot->RotZ01->GetFloatValue(T) : 0.f;
			const float Qw = FoundSlot->RotW01 ? FoundSlot->RotW01->GetFloatValue(T) : 1.f;
			RawQuatAccum += FVector4(Qx, Qy, Qz, Qw) * W;
			RawQuatWeight += W;
			bOutUseRawQuaternion = true;
		}
		else
		{
			RotDegXYZ.X += EvalRotationAxisDeg(FoundSlot->RotX01, SocketBone.MaxRotationDegrees.X) * RotMul * W;
			RotDegXYZ.Y += EvalRotationAxisDeg(FoundSlot->RotY01, SocketBone.MaxRotationDegrees.Y) * RotMul * W;
			RotDegXYZ.Z += EvalRotationAxisDeg(FoundSlot->RotZ01, SocketBone.MaxRotationDegrees.Z) * RotMul * W;
		}

		Trans.X += EvalTranslationAxisCm(FoundSlot->PosX01, SocketBone.MaxTranslationCm.X) * PosMul * W;
		Trans.Y += EvalTranslationAxisCm(FoundSlot->PosY01, SocketBone.MaxTranslationCm.Y) * PosMul * W;
		Trans.Z += EvalTranslationAxisCm(FoundSlot->PosZ01, SocketBone.MaxTranslationCm.Z) * PosMul * W;

		OutIKLockAlpha += EvalContactAlpha(FoundSlot->IkLockAlpha01) * W;
		OutPlantLockAlpha += EvalContactAlpha(FoundSlot->PlantLockAlpha01) * W;
	}

	if (bOutUseRawQuaternion && RawQuatWeight > KINDA_SMALL_NUMBER)
	{
		const FVector4 Q = RawQuatAccum / RawQuatWeight;
		OutRawQuat = FQuat(Q.X, Q.Y, Q.Z, Q.W);
		OutRawQuat.Normalize();
		if (!OutRawQuat.IsNormalized())
		{
			OutRawQuat = FQuat::Identity;
		}
	}

	OutRotDegXYZ = RotDegXYZ;
	OutTransPS = Trans;
}

void FAnimNode_AegisProceduralMotionDriver::ApplySocketBoneChain(
	FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms,
	const FAegisChainDef_Inline& Chain,
	FAegisActionChainRuntimeCache& Cache,
	float Time01,
	float ActionAlpha,
	float DT,
	const FAegisGeneratedFootLockSettings& FootLockSettings,
	EAegisActionPlaybackMode PlaybackMode)
{
	if (Cache.SocketBones.Num() == 0)
	{
		return;
	}

	const FBoneContainer& BC = Output.Pose.GetPose().GetBoneContainer();
	const FColor ConeColor(0, 255, 255, 255);

	for (FAegisSocketBoneRuntimeCache& SocketBone : Cache.SocketBones)
	{
		if (SocketBone.BoneIndex == INDEX_NONE)
		{
			continue;
		}

		const FCompactPoseBoneIndex ParentIdx = BC.GetParentBoneIndex(SocketBone.BoneIndex);
		if (ParentIdx == INDEX_NONE)
		{
			continue;
		}

		FVector TargetRotDegXYZ = FVector::ZeroVector;
		FQuat TargetRawQuat = FQuat::Identity;
		bool bUseRawQuaternion = false;
		FVector TargetTransPS = FVector::ZeroVector;
		EAegisBvhRotationOrder TargetRotationOrder = EAegisBvhRotationOrder::ZXY;
		bool bBypassSmoothing = false;
		bool bReplaceLocalRotation = false;
		bool bUseCapturedStartPoseBase = false;
		bool bUseLiveBasePose = false;
		float TargetIKLockAlpha = 0.0f;
		float TargetPlantLockAlpha = 0.0f;

		EvalSocketBoneTargetParentSpace(
			Chain,
			SocketBone,
			Time01,
			ActionAlpha,
			PlaybackMode,
			TargetRotDegXYZ,
			TargetRawQuat,
			bUseRawQuaternion,
			TargetTransPS,
			TargetRotationOrder,
			bBypassSmoothing,
			bReplaceLocalRotation,
			bUseCapturedStartPoseBase,
			bUseLiveBasePose,
			TargetIKLockAlpha,
			TargetPlantLockAlpha);

		if (bBypassSmoothing)
		{
			SocketBone.SmoothedRotDeg.Roll = TargetRotDegXYZ.X;
			SocketBone.SmoothedRotDeg.Pitch = TargetRotDegXYZ.Y;
			SocketBone.SmoothedRotDeg.Yaw = TargetRotDegXYZ.Z;
			SocketBone.SmoothedQuat = TargetRawQuat;
			SocketBone.SmoothedTransPS = TargetTransPS;

			SocketBone.RotVelocityDeg = FRotator::ZeroRotator;
			SocketBone.TransVelocityPS = FVector::ZeroVector;
		}
		else
		{
			SocketBone.SmoothedRotDeg.Roll =
				StepSpringDamperFloat(
					static_cast<double>(SocketBone.SmoothedRotDeg.Roll),
					static_cast<double>(TargetRotDegXYZ.X),
					SocketBone.RotVelocityDeg.Roll,
					static_cast<double>(DT),
					SocketBone.MotionProfile,
					static_cast<double>(SocketBone.MotionProfile.MaxRotationSpeedDegPerSec));

			SocketBone.SmoothedRotDeg.Pitch =
				StepSpringDamperFloat(
					static_cast<double>(SocketBone.SmoothedRotDeg.Pitch),
					static_cast<double>(TargetRotDegXYZ.Y),
					SocketBone.RotVelocityDeg.Pitch,
					static_cast<double>(DT),
					SocketBone.MotionProfile,
					static_cast<double>(SocketBone.MotionProfile.MaxRotationSpeedDegPerSec));

			SocketBone.SmoothedRotDeg.Yaw =
				StepSpringDamperFloat(
					static_cast<double>(SocketBone.SmoothedRotDeg.Yaw),
					static_cast<double>(TargetRotDegXYZ.Z),
					SocketBone.RotVelocityDeg.Yaw,
					static_cast<double>(DT),
					SocketBone.MotionProfile,
					static_cast<double>(SocketBone.MotionProfile.MaxRotationSpeedDegPerSec));

			SocketBone.SmoothedTransPS.X =
				StepSpringDamperFloat(
					static_cast<double>(SocketBone.SmoothedTransPS.X),
					static_cast<double>(TargetTransPS.X),
					SocketBone.TransVelocityPS.X,
					static_cast<double>(DT),
					SocketBone.MotionProfile,
					static_cast<double>(SocketBone.MotionProfile.MaxTranslationSpeedCmPerSec));

			SocketBone.SmoothedTransPS.Y =
				StepSpringDamperFloat(
					static_cast<double>(SocketBone.SmoothedTransPS.Y),
					static_cast<double>(TargetTransPS.Y),
					SocketBone.TransVelocityPS.Y,
					static_cast<double>(DT),
					SocketBone.MotionProfile,
					static_cast<double>(SocketBone.MotionProfile.MaxTranslationSpeedCmPerSec));

			SocketBone.SmoothedTransPS.Z =
				StepSpringDamperFloat(
					static_cast<double>(SocketBone.SmoothedTransPS.Z),
					static_cast<double>(TargetTransPS.Z),
					SocketBone.TransVelocityPS.Z,
					static_cast<double>(DT),
					SocketBone.MotionProfile,
					static_cast<double>(SocketBone.MotionProfile.MaxTranslationSpeedCmPerSec));
		}

		const float TargetIKAlphaClamped = FMath::Clamp(TargetIKLockAlpha, 0.0f, 1.0f);
		const float TargetPlantAlphaClamped = FMath::Clamp(TargetPlantLockAlpha, 0.0f, 1.0f);
		const float ContactAlphaInterpSpeed = FMath::Max(0.0f, FootLockSettings.ContactAlphaInterpSpeed);
		if (ContactAlphaInterpSpeed > KINDA_SMALL_NUMBER && DT > KINDA_SMALL_NUMBER)
		{
			SocketBone.SmoothedIKLockAlpha = FMath::FInterpTo(SocketBone.SmoothedIKLockAlpha, TargetIKAlphaClamped, DT, ContactAlphaInterpSpeed);
			SocketBone.SmoothedPlantLockAlpha = FMath::FInterpTo(SocketBone.SmoothedPlantLockAlpha, TargetPlantAlphaClamped, DT, ContactAlphaInterpSpeed);
		}
		else
		{
			SocketBone.SmoothedIKLockAlpha = TargetIKAlphaClamped;
			SocketBone.SmoothedPlantLockAlpha = TargetPlantAlphaClamped;
		}

		// Exact mocap playback is not an additive offset from the currently captured animation pose.
		// AMC/BVH/FBX channels describe animation deltas from a skeleton rest pose.  In V7 the runtime
		// incorrectly built raw mocap bones from identity rotation, which erased the UE mannequin bind
		// orientation and caused limbs to explode/twist away from the body.  For raw local-replace clips,
		// always start from the compact-pose reference/local bind transform, then apply the imported
		// local delta rotation on top of that reference transform.
		FTransform BoneLS = FTransform::Identity;
		if (bReplaceLocalRotation)
		{
			if (bUseLiveBasePose)
			{
				// V36 live-base overlay: use the current source-pose local transform every frame.
				// This allows the normal AnimBP locomotion/run cycle to continue underneath.
				BoneLS = Output.Pose.GetLocalSpaceTransform(SocketBone.BoneIndex);
			}
			else if (bUseCapturedStartPoseBase && bHasCapturedStartPose)
			{
				BoneLS = SocketBone.CapturedLocalTransform;
			}
			else
			{
				BoneLS = BC.GetRefPoseTransform(SocketBone.BoneIndex);
			}
		}
		else
		{
			BoneLS = (ActionAlpha > KINDA_SMALL_NUMBER && bHasCapturedStartPose)
				? SocketBone.CapturedLocalTransform
				: Output.Pose.GetLocalSpaceTransform(SocketBone.BoneIndex);
		}

		const FVector SmoothedRotDegXYZ(
			SocketBone.SmoothedRotDeg.Roll,
			SocketBone.SmoothedRotDeg.Pitch,
			SocketBone.SmoothedRotDeg.Yaw);

		if (bUseRawQuaternion)
		{
			// V11 exact path: curves already store the ASF/AMC rotation as a ref-pose-relative local quaternion.
			// Do not decompose/recompose into Euler; that was the V9/V10 source of the wonky rotations.
			BoneLS.SetRotation(BoneLS.GetRotation() * SocketBone.SmoothedQuat);
		}
		else
		{
			ApplyAegisOrderedEuler(BoneLS, SmoothedRotDegXYZ, TargetRotationOrder);
		}

		if (bReplaceLocalRotation && SocketBone.BoneName == FName(TEXT("pelvis")))
		{
			// Root motion curves are delta centimeters.  For generated-native clips the base location is
			// the captured idle/locomotion pose; for legacy exact mocap it remains the UE reference pose.
			const FVector BaseLocation = bUseLiveBasePose
				? Output.Pose.GetLocalSpaceTransform(SocketBone.BoneIndex).GetLocation()
				: (bUseCapturedStartPoseBase && bHasCapturedStartPose
					? SocketBone.CapturedLocalTransform.GetLocation()
					: BC.GetRefPoseTransform(SocketBone.BoneIndex).GetLocation());
			BoneLS.SetLocation(BaseLocation + SocketBone.SmoothedTransPS);
		}
		else
		{
			BoneLS.AddToTranslation(SocketBone.SmoothedTransPS);
		}

		const FTransform ParentCS = Output.Pose.GetComponentSpaceTransform(ParentIdx);
		const FTransform BoneCS = BoneLS * ParentCS;

		Output.Pose.SetComponentSpaceTransform(SocketBone.BoneIndex, BoneCS);
		OutBoneTransforms.Add(FBoneTransform(SocketBone.BoneIndex, BoneCS));
	}


	if (FootLockSettings.bEnableGeneratedFootLock && (!FootLockSettings.bEnableGeneratedTwoBoneIK || FootLockSettings.bAllowGeneratedRootCorrection))
	{
		const float ReleaseThreshold = FMath::Clamp(FootLockSettings.LockReleaseThreshold, 0.0f, 0.95f);
		const float ActivationThreshold = FMath::Clamp(FootLockSettings.LockActivationThreshold, ReleaseThreshold + 0.01f, 1.0f);
		const float MaxCorrection = FMath::Max(0.0f, FootLockSettings.MaxRootCorrectionCm);
		const float BlendPower = FMath::Max(0.1f, FootLockSettings.LockBlendPower);

		FVector AccumulatedCorrectionCS = FVector::ZeroVector;
		float AccumulatedWeight = 0.0f;

		for (FAegisSocketBoneRuntimeCache& SocketBone : Cache.SocketBones)
		{
			if (SocketBone.BoneIndex == INDEX_NONE || !IsGeneratedFootLockBoneName(SocketBone.BoneName))
			{
				continue;
			}

			const float RawLockAlpha = FMath::Max(SocketBone.SmoothedPlantLockAlpha, SocketBone.SmoothedIKLockAlpha);

			if (RawLockAlpha <= ReleaseThreshold)
			{
				SocketBone.bHasPlantLockTarget = false;
				SocketBone.PlantLockTargetCS = FVector::ZeroVector;
				continue;
			}

			const FTransform CurrentFootCS = Output.Pose.GetComponentSpaceTransform(SocketBone.BoneIndex);
			const FVector CurrentFootLocationCS = CurrentFootCS.GetLocation();

			// While the contact is ramping in, keep refreshing the target to avoid a foot-pop.
			// Once the authored contact alpha is confident, freeze the target and move the generated
			// full-body pose around that planted foot.
			if (!SocketBone.bHasPlantLockTarget || RawLockAlpha < ActivationThreshold)
			{
				SocketBone.PlantLockTargetCS = CurrentFootLocationCS;
				SocketBone.bHasPlantLockTarget = true;
			}

			const float NormalizedLock = FMath::Clamp((RawLockAlpha - ReleaseThreshold) / FMath::Max(KINDA_SMALL_NUMBER, 1.0f - ReleaseThreshold), 0.0f, 1.0f);
			const float LockWeight = FMath::Pow(NormalizedLock, BlendPower);
			if (LockWeight <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			AccumulatedCorrectionCS += (SocketBone.PlantLockTargetCS - CurrentFootLocationCS) * LockWeight;
			AccumulatedWeight += LockWeight;
		}

		if (AccumulatedWeight > KINDA_SMALL_NUMBER)
		{
			FVector CorrectionCS = AccumulatedCorrectionCS / AccumulatedWeight;
			if (MaxCorrection > 0.0f && CorrectionCS.Size() > MaxCorrection)
			{
				CorrectionCS = CorrectionCS.GetSafeNormal() * MaxCorrection;
			}

			if (FootLockSettings.bEnableGeneratedTwoBoneIK)
			{
				// V28: leave most of the remaining foot error for the leg IK solver.
				// This prevents the V27 behavior where the whole body was translated around a planted foot
				// but the knee/ankle chain still did not solve to the contact target.
				CorrectionCS *= FMath::Clamp(FootLockSettings.TwoBoneIKRootCorrectionWeight, 0.0f, 1.0f);
			}

			if (!CorrectionCS.IsNearlyZero(0.001f))
			{
				for (const FAegisSocketBoneRuntimeCache& SocketBone : Cache.SocketBones)
				{
					if (SocketBone.BoneIndex == INDEX_NONE)
					{
						continue;
					}

					FTransform CorrectedCS = Output.Pose.GetComponentSpaceTransform(SocketBone.BoneIndex);
					CorrectedCS.AddToTranslation(CorrectionCS);
					Output.Pose.SetComponentSpaceTransform(SocketBone.BoneIndex, CorrectedCS);
					OutBoneTransforms.Add(FBoneTransform(SocketBone.BoneIndex, CorrectedCS));
				}
			}
		}
	}

	if (FootLockSettings.bEnableGeneratedTwoBoneIK)
	{
		const float ReleaseThreshold = FMath::Clamp(FootLockSettings.LockReleaseThreshold, 0.0f, 0.95f);
		for (FAegisSocketBoneRuntimeCache& SocketBone : Cache.SocketBones)
		{
			if (SocketBone.BoneIndex == INDEX_NONE || !IsGeneratedFootLockBoneName(SocketBone.BoneName))
			{
				continue;
			}

			const float RawLockAlpha = FMath::Max(SocketBone.SmoothedPlantLockAlpha, SocketBone.SmoothedIKLockAlpha);
			if (RawLockAlpha <= ReleaseThreshold)
			{
				continue;
			}

			ApplyGeneratedLegTwoBoneIKCS(
				Output,
				OutBoneTransforms,
				Cache,
				SocketBone,
				RawLockAlpha,
				FootLockSettings,
				Output.AnimInstanceProxy);
		}
	}

	if (ShouldDrawDebug(*this) && Output.AnimInstanceProxy)
	{
		USkeletalMeshComponent* SkelComp = Output.AnimInstanceProxy->GetSkelMeshComponent();
		if (SkelComp)
		{
			const FTransform ComponentTM = Output.AnimInstanceProxy->GetComponentTransform();

			for (const FAegisSocketBoneRuntimeCache& SocketBone : Cache.SocketBones)
			{
				if (SocketBone.BoneIndex == INDEX_NONE)
				{
					continue;
				}

				const FTransform BoneCS = Output.Pose.GetComponentSpaceTransform(SocketBone.BoneIndex);
				const FVector WorldPos = ComponentTM.TransformPosition(BoneCS.GetLocation());
				const FVector X = ComponentTM.TransformVector(BoneCS.GetUnitAxis(EAxis::X));
				const FVector Y = ComponentTM.TransformVector(BoneCS.GetUnitAxis(EAxis::Y));
				const FVector Z = ComponentTM.TransformVector(BoneCS.GetUnitAxis(EAxis::Z));

				AnimDrawDebugLineSafe(Output.AnimInstanceProxy, WorldPos, WorldPos + X * 10.f, FColor::Red, 1.2f);
				AnimDrawDebugLineSafe(Output.AnimInstanceProxy, WorldPos, WorldPos + Y * 10.f, FColor::Green, 1.2f);
				AnimDrawDebugLineSafe(Output.AnimInstanceProxy, WorldPos, WorldPos + Z * 10.f, FColor::Blue, 1.2f);

				if (Chain.bDrawDebugCones)
				{
					const float ConeAngle =
						FMath::Max3(
							SocketBone.MaxRotationDegrees.X,
							SocketBone.MaxRotationDegrees.Y,
							SocketBone.MaxRotationDegrees.Z);

					DrawDebugConeApprox(Output.AnimInstanceProxy, WorldPos, X, 14.f, ConeAngle, ConeColor);
				}

				if (bDebugDrawJointNumbers)
				{
					if (UWorld* World = SkelComp->GetWorld())
					{
						const FString Text = FString::Printf(
							TEXT("%s\nRot %.1f %.1f %.1f\nPos %.1f %.1f %.1f"),
							*SocketBone.BoneName.ToString(),
							static_cast<double>(SocketBone.SmoothedRotDeg.Roll),
							static_cast<double>(SocketBone.SmoothedRotDeg.Pitch),
							static_cast<double>(SocketBone.SmoothedRotDeg.Yaw),
							static_cast<double>(SocketBone.SmoothedTransPS.X),
							static_cast<double>(SocketBone.SmoothedTransPS.Y),
							static_cast<double>(SocketBone.SmoothedTransPS.Z));

						EnqueueDebugString(World, WorldPos + FVector(0.f, 0.f, 12.f), Text, FColor::Yellow);
					}
				}
			}
		}
	}
}

void FAnimNode_AegisProceduralMotionDriver::EvaluateSkeletalControl_AnyThread(
	FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms)
{
	OutBoneTransforms.Reset();

	const UAegisProceduralActionAsset* Asset = nullptr;
	float Time01 = 0.f;
	float ActionAlpha = 0.f;
	ResolveActionState(Asset, Time01, ActionAlpha);

	if (!Asset || Asset->Chains.Num() == 0)
	{
		return;
	}

	const FBoneContainer& BC = Output.Pose.GetPose().GetBoneContainer();
	EnsureCachesBuilt(Asset, BC);

	if (ActionComponent)
	{
		const uint32 CurrentActionInstanceId = static_cast<uint32>(ActionComponent->GetActionInstanceId());
		if (CurrentActionInstanceId != LastSeenActionInstanceId)
		{
			ResetAllSmoothedStates();
			LastSeenActionInstanceId = CurrentActionInstanceId;
		}
	}

	if (ActionAlpha > KINDA_SMALL_NUMBER && !bHasCapturedStartPose)
	{
		CaptureStartPose(Output);
	}
	else if (ActionAlpha <= KINDA_SMALL_NUMBER)
	{
		bHasCapturedStartPose = false;
	}

	const float DT = Output.AnimInstanceProxy ? Output.AnimInstanceProxy->GetDeltaSeconds() : (1.f / 60.f);

	for (int32 ChainIndex = 0; ChainIndex < Asset->Chains.Num(); ++ChainIndex)
	{
		if (!ActionChainCaches.IsValidIndex(ChainIndex))
		{
			continue;
		}

		const FAegisChainDef_Inline& Chain = Asset->Chains[ChainIndex];
		if (!Chain.bApplyToChain)
		{
			continue;
		}

		FAegisActionChainRuntimeCache& Cache = ActionChainCaches[ChainIndex];
		if (Cache.ChainBones.Num() == 0 && Cache.SocketBones.Num() == 0)
		{
			BuildCacheForChain(Cache, Chain, BC);
		}

		ApplySocketBoneChain(Output, OutBoneTransforms, Chain, Cache, Time01, ActionAlpha, DT, Asset->FootLockSettings, Asset->PlaybackMode);
	}

	OutBoneTransforms.Sort([](const FBoneTransform& A, const FBoneTransform& B)
		{
			return A.BoneIndex < B.BoneIndex;
		});

	if (OutBoneTransforms.Num() > 1)
	{
		TArray<FBoneTransform> Unique;
		Unique.Reserve(OutBoneTransforms.Num());

		for (const FBoneTransform& Transform : OutBoneTransforms)
		{
			if (Unique.Num() > 0 && Unique.Last().BoneIndex == Transform.BoneIndex)
			{
				Unique.Last() = Transform;
			}
			else
			{
				Unique.Add(Transform);
			}
		}

		OutBoneTransforms = MoveTemp(Unique);
	}
}
