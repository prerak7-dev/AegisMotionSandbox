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

void FAnimNode_AegisProceduralMotionDriver::EvalSocketBoneTargetParentSpace(
	const FAegisChainDef_Inline& Chain,
	const FAegisSocketBoneRuntimeCache& SocketBone,
	float Time01,
	float ActionAlpha,
	FRotator& OutRotDeg,
	FVector& OutTransPS) const
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
		float W = EvalAutomaticPhaseWeight(Phase, T);

		if (Phase.PhaseAlpha01)
		{
			W *= CurveToUnsigned01(Phase.PhaseAlpha01->GetFloatValue(T));
		}

		RawWeights.Add(W);
		TotalWeight += W;
	}

	float RotX = 0.f;
	float RotY = 0.f;
	float RotZ = 0.f;
	FVector Trans = FVector::ZeroVector;

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

		RotX += (FoundSlot->RotX01 ? CurveToSignedSmart(FoundSlot->RotX01->GetFloatValue(T)) : 0.f) * SocketBone.MaxRotationDegrees.X * RotMul * W;
		RotY += (FoundSlot->RotY01 ? CurveToSignedSmart(FoundSlot->RotY01->GetFloatValue(T)) : 0.f) * SocketBone.MaxRotationDegrees.Y * RotMul * W;
		RotZ += (FoundSlot->RotZ01 ? CurveToSignedSmart(FoundSlot->RotZ01->GetFloatValue(T)) : 0.f) * SocketBone.MaxRotationDegrees.Z * RotMul * W;

		Trans.X += (FoundSlot->PosX01 ? CurveToSignedSmart(FoundSlot->PosX01->GetFloatValue(T)) : 0.f) * SocketBone.MaxTranslationCm.X * PosMul * W;
		Trans.Y += (FoundSlot->PosY01 ? CurveToSignedSmart(FoundSlot->PosY01->GetFloatValue(T)) : 0.f) * SocketBone.MaxTranslationCm.Y * PosMul * W;
		Trans.Z += (FoundSlot->PosZ01 ? CurveToSignedSmart(FoundSlot->PosZ01->GetFloatValue(T)) : 0.f) * SocketBone.MaxTranslationCm.Z * PosMul * W;
	}

	OutRotDeg = FRotator(RotY, RotZ, RotX);
	OutTransPS = Trans;
}

void FAnimNode_AegisProceduralMotionDriver::ApplySocketBoneChain(
	FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms,
	const FAegisChainDef_Inline& Chain,
	FAegisActionChainRuntimeCache& Cache,
	float Time01,
	float ActionAlpha,
	float DT)
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

		FRotator TargetRotDeg = FRotator::ZeroRotator;
		FVector TargetTransPS = FVector::ZeroVector;
		EvalSocketBoneTargetParentSpace(Chain, SocketBone, Time01, ActionAlpha, TargetRotDeg, TargetTransPS);

		SocketBone.SmoothedRotDeg.Roll =
			StepSpringDamperFloat(
				static_cast<double>(SocketBone.SmoothedRotDeg.Roll),
				static_cast<double>(TargetRotDeg.Roll),
				SocketBone.RotVelocityDeg.Roll,
				static_cast<double>(DT),
				SocketBone.MotionProfile,
				static_cast<double>(SocketBone.MotionProfile.MaxRotationSpeedDegPerSec));

		SocketBone.SmoothedRotDeg.Pitch =
			StepSpringDamperFloat(
				static_cast<double>(SocketBone.SmoothedRotDeg.Pitch),
				static_cast<double>(TargetRotDeg.Pitch),
				SocketBone.RotVelocityDeg.Pitch,
				static_cast<double>(DT),
				SocketBone.MotionProfile,
				static_cast<double>(SocketBone.MotionProfile.MaxRotationSpeedDegPerSec));

		SocketBone.SmoothedRotDeg.Yaw =
			StepSpringDamperFloat(
				static_cast<double>(SocketBone.SmoothedRotDeg.Yaw),
				static_cast<double>(TargetRotDeg.Yaw),
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

		FTransform BoneLS =
			(ActionAlpha > KINDA_SMALL_NUMBER && bHasCapturedStartPose)
			? SocketBone.CapturedLocalTransform
			: Output.Pose.GetLocalSpaceTransform(SocketBone.BoneIndex);

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

		ApplyAxisRot(FVector(1.0, 0.0, 0.0), SocketBone.SmoothedRotDeg.Roll);
		ApplyAxisRot(FVector(0.0, 1.0, 0.0), SocketBone.SmoothedRotDeg.Pitch);
		ApplyAxisRot(FVector(0.0, 0.0, 1.0), SocketBone.SmoothedRotDeg.Yaw);

		BoneLS.AddToTranslation(SocketBone.SmoothedTransPS);

		const FTransform ParentCS = Output.Pose.GetComponentSpaceTransform(ParentIdx);
		const FTransform BoneCS = BoneLS * ParentCS;

		Output.Pose.SetComponentSpaceTransform(SocketBone.BoneIndex, BoneCS);
		OutBoneTransforms.Add(FBoneTransform(SocketBone.BoneIndex, BoneCS));
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

		ApplySocketBoneChain(Output, OutBoneTransforms, Chain, Cache, Time01, ActionAlpha, DT);
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
