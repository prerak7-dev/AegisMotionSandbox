// AnimNode_AegisProceduralMotionDriver.cpp
#include "ProceduralMotion/AnimNodes/AnimNode_AegisProceduralMotionDriver.h"

#include "Algo/Reverse.h"
#include "Animation/AnimInstanceProxy.h"
#include "Async/Async.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarAegisDebugProceduralDriver(
	TEXT("aegis.Motion.DebugProceduralDriver"),
	0,
	TEXT("0=off, 1=log, 2=draw debug"),
	ECVF_Default);

// Production debug quality controls
static TAutoConsoleVariable<int32> CVarAegisDebugXRay(
	TEXT("aegis.Motion.DebugXRay"),
	1,
	TEXT("0=depth-tested (occluded), 1=foreground (x-ray)"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAegisDebugHalo(
	TEXT("aegis.Motion.DebugHalo"),
	1,
	TEXT("0=single line, 1=halo outline (black thick + colored thin)"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAegisDebugOffsetCm(
	TEXT("aegis.Motion.DebugOffsetCm"),
	3.f,
	TEXT("World-space offset (cm) applied to debug points to avoid z-fighting / skin occlusion."),
	ECVF_Default);

// Pivot curves: accept either signed (-1..1) or 0..1 centered at 0.5
static FORCEINLINE float CurveToSignedSmart(float V)
{
	if (V < 0.f || V > 1.f) return V;
	return (V - 0.5f) * 2.f;
}

// Hinge curves (default unsigned 0..1)
static FORCEINLINE float CurveToUnsigned01(float V)
{
	return FMath::Clamp(V, 0.f, 1.f);
}

// NEW: per-slot hinge curve evaluation (unsigned or signed)
static FORCEINLINE float EvalHingeCurveSlot(const FAegisHingeCurveSlot& Slot, float T)
{
	if (!Slot.Angle01) return 0.f;

	const float V = Slot.Angle01->GetFloatValue(T);

	if (Slot.bSignedAngle)
	{
		// If author provided true signed values already, keep them.
		if (V < 0.f || V > 1.f) return V;

		// Otherwise treat 0..1 as centered at 0.5 -> [-1..1]
		return (V - 0.5f) * 2.f;
	}

	// Unsigned
	return FMath::Clamp(V, 0.f, 1.f);
}

static FORCEINLINE bool ShouldDrawDebug(const FAnimNode_AegisProceduralMotionDriver& Node)
{
	const int32 DebugLevel = CVarAegisDebugProceduralDriver.GetValueOnAnyThread();
	return (Node.bDebugDraw || DebugLevel >= 2);
}

static FORCEINLINE ESceneDepthPriorityGroup GetDebugDepthGroup()
{
	return (CVarAegisDebugXRay.GetValueOnAnyThread() != 0) ? SDPG_Foreground : SDPG_World;
}

static FORCEINLINE FVector DebugOffsetVector(const FTransform& ComponentTM)
{
	const float OffCm = CVarAegisDebugOffsetCm.GetValueOnAnyThread();
	return ComponentTM.TransformVectorNoScale(FVector(0, 0, OffCm));
}

static void AnimDrawDebugLinePro(
	FAnimInstanceProxy* Proxy,
	const FVector& A,
	const FVector& B,
	const FColor& Color,
	float Thickness)
{
	if (!Proxy) return;

	const bool bPersistent = false;
	const float LifeTime = 0.f;
	const ESceneDepthPriorityGroup DepthGroup = GetDebugDepthGroup();
	const bool bHalo = (CVarAegisDebugHalo.GetValueOnAnyThread() != 0);

	if (bHalo)
	{
		Proxy->AnimDrawDebugLine(A, B, FColor::Black, bPersistent, LifeTime, Thickness + 2.0f, DepthGroup);
		Proxy->AnimDrawDebugLine(A, B, Color, bPersistent, LifeTime, Thickness, DepthGroup);
	}
	else
	{
		Proxy->AnimDrawDebugLine(A, B, Color, bPersistent, LifeTime, Thickness, DepthGroup);
	}
}

static void AnimDrawJointCross(
	FAnimInstanceProxy* Proxy,
	const FVector& P,
	float Size,
	const FColor& C,
	float Thickness)
{
	if (!Proxy) return;

	AnimDrawDebugLinePro(Proxy, P - FVector(Size, 0, 0), P + FVector(Size, 0, 0), C, Thickness);
	AnimDrawDebugLinePro(Proxy, P - FVector(0, Size, 0), P + FVector(0, Size, 0), C, Thickness);
	AnimDrawDebugLinePro(Proxy, P - FVector(0, 0, Size), P + FVector(0, 0, Size), C, Thickness);
}

static void AnimDrawDebugAxesPro(
	FAnimInstanceProxy* Proxy,
	const FVector& Origin,
	const FVector& X,
	const FVector& Y,
	const FVector& Z)
{
	if (!Proxy) return;
	AnimDrawDebugLinePro(Proxy, Origin, Origin + X, FColor::Red, 1.8f);
	AnimDrawDebugLinePro(Proxy, Origin, Origin + Y, FColor::Green, 1.8f);
	AnimDrawDebugLinePro(Proxy, Origin, Origin + Z, FColor::Blue, 1.8f);
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
	return (OutCompact != INDEX_NONE);
}

static void EnqueueDebugString(
	UWorld* World,
	const FVector& WorldPos,
	const FString& Text,
	const FColor& Color)
{
	if (!World) return;

	TWeakObjectPtr<UWorld> WeakWorld(World);
	AsyncTask(ENamedThreads::GameThread, [WeakWorld, WorldPos, Text, Color]()
		{
			if (UWorld* W = WeakWorld.Get())
			{
				DrawDebugString(W, WorldPos, Text, nullptr, Color, 0.f, false);
			}
		});
}

// NOTE: CSPose is non-const for UE variants where GetComponentSpaceTransform isn't const.
static void DebugDrawChainOutlineAndAxes(
	FAnimInstanceProxy* Proxy,
	const FTransform& ComponentTM,
	FCSPose<FCompactPose>& CSPose,
	const FBoneContainer& BoneContainer,
	const TArray<FCompactPoseBoneIndex>& ChainBones,
	FCompactPoseBoneIndex LabelBone,
	const FRotator& AppliedDeltaPRY_Deg,
	const FString* OptionalExtraText,
	float SegmentThickness)
{
	if (!Proxy || ChainBones.Num() == 0) return;

	const FVector Off = DebugOffsetVector(ComponentTM);

	for (const FCompactPoseBoneIndex& B : ChainBones)
	{
		const FTransform& BCS = CSPose.GetComponentSpaceTransform(B);
		const FVector Pw = ComponentTM.TransformPosition(BCS.GetLocation()) + Off;

		AnimDrawJointCross(Proxy, Pw, 3.5f, FColor::Cyan, 1.0f);

		const FCompactPoseBoneIndex P = BoneContainer.GetParentBoneIndex(B);
		if (P != INDEX_NONE && ChainBones.Contains(P))
		{
			const FTransform& PCS = CSPose.GetComponentSpaceTransform(P);
			const FVector Ppw = ComponentTM.TransformPosition(PCS.GetLocation()) + Off;

			AnimDrawDebugLinePro(Proxy, Ppw, Pw, FColor::Cyan, SegmentThickness);
		}
	}

	if (LabelBone != INDEX_NONE)
	{
		const FTransform& LCS = CSPose.GetComponentSpaceTransform(LabelBone);
		const FVector Lw = ComponentTM.TransformPosition(LCS.GetLocation()) + Off;

		const FVector X = ComponentTM.TransformVector(LCS.GetUnitAxis(EAxis::X)) * 14.f;
		const FVector Y = ComponentTM.TransformVector(LCS.GetUnitAxis(EAxis::Y)) * 14.f;
		const FVector Z = ComponentTM.TransformVector(LCS.GetUnitAxis(EAxis::Z)) * 14.f;

		AnimDrawDebugAxesPro(Proxy, Lw, X, Y, Z);

		if (USkeletalMeshComponent* SkelComp = Proxy->GetSkelMeshComponent())
		{
			if (UWorld* World = SkelComp->GetWorld())
			{
				FString S = FString::Printf(
					TEXT("PRY (deg)\nP: %.2f\nY: %.2f\nR: %.2f"),
					AppliedDeltaPRY_Deg.Pitch,
					AppliedDeltaPRY_Deg.Yaw,
					AppliedDeltaPRY_Deg.Roll);

				if (OptionalExtraText && OptionalExtraText->Len() > 0)
				{
					S += TEXT("\n");
					S += *OptionalExtraText;
				}

				EnqueueDebugString(World, Lw + FVector(0, 0, 20.f), S, FColor::Yellow);
			}
		}
	}
}

static void DebugDrawHingeAngles(
	FAnimInstanceProxy* Proxy,
	const FTransform& ComponentTM,
	FCSPose<FCompactPose>& CSPose,
	const TArray<FCompactPoseBoneIndex>& HingeBones,
	const TArray<float>& SmoothedAnglesDeg)
{
	if (!Proxy || HingeBones.Num() == 0) return;

	USkeletalMeshComponent* SkelComp = Proxy->GetSkelMeshComponent();
	if (!SkelComp) return;

	UWorld* World = SkelComp->GetWorld();
	if (!World) return;

	const FVector Off = DebugOffsetVector(ComponentTM);

	const int32 N = FMath::Min(HingeBones.Num(), SmoothedAnglesDeg.Num());
	for (int32 i = 0; i < N; ++i)
	{
		const FTransform& BCS = CSPose.GetComponentSpaceTransform(HingeBones[i]);
		const FVector Pw = ComponentTM.TransformPosition(BCS.GetLocation()) + Off;

		const FString S = FString::Printf(TEXT("[HINGE] %.1f deg"), SmoothedAnglesDeg[i]);
		EnqueueDebugString(World, Pw + FVector(0, 0, 18.f), S, FColor::White);
	}
}

// ---------------- Node ----------------

FAnimNode_AegisProceduralMotionDriver::FAnimNode_AegisProceduralMotionDriver()
{
}

void FAnimNode_AegisProceduralMotionDriver::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);
	ActionChainCaches.Reset();
}

void FAnimNode_AegisProceduralMotionDriver::UpdateInternal(const FAnimationUpdateContext& Context)
{
	Super::UpdateInternal(Context);
}

bool FAnimNode_AegisProceduralMotionDriver::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	const UAegisProceduralActionAsset* Asset = nullptr;
	float Time01 = 0.f, ActionAlpha = 0.f;
	ResolveActionState(Asset, Time01, ActionAlpha);
	return (Asset && Asset->Chains.Num() > 0);
}

void FAnimNode_AegisProceduralMotionDriver::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	const UAegisProceduralActionAsset* Asset = nullptr;
	float Time01 = 0.f, ActionAlpha = 0.f;
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

	if (ActionComponent && ActionComponent->IsActionActive())
	{
		OutAsset = ActionComponent->GetCurrentActionAsset();
		OutTime01 = ActionComponent->GetActionTime01();
		OutAlpha = ActionComponent->GetActionAlpha();
		return;
	}

	if (ActionAssetOverride)
	{
		OutAsset = ActionAssetOverride;
		OutTime01 = ActionTime01Override;
		OutAlpha = ActionAlphaOverride;
	}
}

void FAnimNode_AegisProceduralMotionDriver::EnsureCachesBuilt(
	const UAegisProceduralActionAsset* Asset,
	const FBoneContainer& RequiredBones)
{
	if (!Asset) return;

	if (ActionChainCaches.Num() != Asset->Chains.Num())
	{
		ActionChainCaches.SetNum(Asset->Chains.Num());
		for (FAegisActionChainRuntimeCache& C : ActionChainCaches)
		{
			C.Reset();
		}
	}

	for (int32 i = 0; i < Asset->Chains.Num(); ++i)
	{
		if (ActionChainCaches[i].ChainBones.Num() == 0)
		{
			BuildCacheForChain(ActionChainCaches[i], Asset->Chains[i], RequiredBones);
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
		if (Cur == StartIdx) break;
		Cur = RequiredBones.GetParentBoneIndex(Cur);
	}

	Algo::Reverse(Temp);
	Cache.ChainBones = MoveTemp(Temp);

	FCompactPoseBoneIndex PivotIdx = StartIdx;

	if (!Chain.PivotBone.IsNone())
	{
		const int32 PivotPoseIndex = RequiredBones.GetPoseBoneIndexForBoneName(Chain.PivotBone);
		FCompactPoseBoneIndex P(INDEX_NONE);

		if (PoseIndexToCompact(RequiredBones, PivotPoseIndex, P) && Cache.ChainBones.Contains(P))
		{
			PivotIdx = P;
		}
	}

	Cache.PivotBone = PivotIdx;

	TSet<int32> Unique;
	for (const FAegisAutoHingePhase& Phase : Chain.HingePhases)
	{
		for (const FAegisHingeCurveSlot& Slot : Phase.Hinges)
		{
			if (Slot.HingeBone.IsNone()) continue;

			const int32 PoseIdx = RequiredBones.GetPoseBoneIndexForBoneName(Slot.HingeBone);
			FCompactPoseBoneIndex CIdx(INDEX_NONE);
			if (PoseIndexToCompact(RequiredBones, PoseIdx, CIdx))
			{
				Unique.Add(CIdx.GetInt());
			}
		}
	}

	for (int32 Id : Unique)
	{
		Cache.HingeBones.Add(FCompactPoseBoneIndex(Id));
	}

	Cache.HingeBones.Sort([](const FCompactPoseBoneIndex& A, const FCompactPoseBoneIndex& B)
		{
			return A.GetInt() < B.GetInt();
		});

	Cache.SmoothedHingeAnglesDeg.SetNum(Cache.HingeBones.Num());
	for (float& V : Cache.SmoothedHingeAnglesDeg) V = 0.f;
}

float FAnimNode_AegisProceduralMotionDriver::HalfLifeAlpha(float DeltaSeconds, float HalfLifeSeconds)
{
	if (HalfLifeSeconds <= KINDA_SMALL_NUMBER) return 1.f;
	return 1.f - FMath::Pow(0.5f, DeltaSeconds / HalfLifeSeconds);
}

// ---------------- Pivot ----------------

FRotator FAnimNode_AegisProceduralMotionDriver::EvalPivotTargetDeg(
	const FAegisChainDef_Inline& Chain,
	float Time01,
	float ActionAlpha) const
{
	const float T = FMath::Clamp(Time01, 0.f, 1.f);
	const float A = FMath::Clamp(ActionAlpha, 0.f, 1.f);

	float Pitch = 0.f, Yaw = 0.f, Roll = 0.f;

	for (const FAegisPhaseCurvesPRY& Phase : Chain.PivotPhases)
	{
		float Window = 1.f;
		if (Phase.Window01) Window = Phase.Window01->GetFloatValue(T);
		if (Window < Phase.ActiveThreshold) continue;

		const float PhaseAlpha = Window;

		const float P = Phase.Pitch01 ? CurveToSignedSmart(Phase.Pitch01->GetFloatValue(T)) : 0.f;
		const float Y = Phase.Yaw01 ? CurveToSignedSmart(Phase.Yaw01->GetFloatValue(T)) : 0.f;
		const float R = Phase.Roll01 ? CurveToSignedSmart(Phase.Roll01->GetFloatValue(T)) : 0.f;

		// Robust channel driving: if a curve is assigned, it contributes (prevents mask mismatch failures)
		const bool bDrivePitch = (Phase.Pitch01 != nullptr) || ((Chain.DrivenChannels & int32(EAegisRotChannels::Pitch)) != 0);
		const bool bDriveYaw = (Phase.Yaw01 != nullptr) || ((Chain.DrivenChannels & int32(EAegisRotChannels::Yaw)) != 0);
		const bool bDriveRoll = (Phase.Roll01 != nullptr) || ((Chain.DrivenChannels & int32(EAegisRotChannels::Roll)) != 0);

		if (bDrivePitch) Pitch += P * Chain.MaxDegreesPRY.Pitch * PhaseAlpha;
		if (bDriveYaw)   Yaw += Y * Chain.MaxDegreesPRY.Yaw * PhaseAlpha;
		if (bDriveRoll)  Roll += R * Chain.MaxDegreesPRY.Roll * PhaseAlpha;
	}

	return FRotator(Pitch * A, Yaw * A, Roll * A);
}

float FAnimNode_AegisProceduralMotionDriver::DistributionWeight(
	EAegisChainDistributionMode Mode,
	int32 Index,
	int32 Num,
	int32 PivotIndex) const
{
	if (Num <= 0) return 0.f;
	if (Num == 1) return 1.f;

	switch (Mode)
	{
	case EAegisChainDistributionMode::Uniform:
		return float(Index + 1) / float(Num);

	case EAegisChainDistributionMode::RampToEnd:
		return float(Index) / float(Num - 1);

	case EAegisChainDistributionMode::PivotToEnd:
	default:
	{
		if (PivotIndex < 0)
		{
			return float(Index) / float(Num - 1);
		}
		if (Index <= PivotIndex)
		{
			return 0.f;
		}
		const float Den = float((Num - 1) - PivotIndex);
		return (Den > 0.f) ? float(Index - PivotIndex) / Den : 1.f;
	}
	}
}

void FAnimNode_AegisProceduralMotionDriver::ApplyPivotChain(
	FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms,
	const FAegisChainDef_Inline& Chain,
	FAegisActionChainRuntimeCache& Cache,
	const FRotator& TargetDeg,
	float DT)
{
	if (Cache.ChainBones.Num() == 0) return;

	const float S = HalfLifeAlpha(DT, Chain.SmoothingHalfLife);
	Cache.SmoothedPivotDelta = FMath::Lerp(Cache.SmoothedPivotDelta, TargetDeg, S);

	const int32 Num = Cache.ChainBones.Num();
	const int32 PivotIndex = Cache.ChainBones.IndexOfByKey(Cache.PivotBone);

	float PrevCumW = 0.f;
	for (int32 i = 0; i < Num; ++i)
	{
		float CumW = DistributionWeight(Chain.DistributionMode, i, Num, PivotIndex);
		CumW = FMath::Clamp(CumW, 0.f, 1.f);

		// Smoothstep to avoid harsh kinks at the start/end of the chain.
		CumW = CumW * CumW * (3.f - 2.f * CumW);

		const float DeltaW = CumW - PrevCumW;
		PrevCumW = CumW;

		if (DeltaW <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FCompactPoseBoneIndex BoneIdx = Cache.ChainBones[i];
		FTransform BoneCS = Output.Pose.GetComponentSpaceTransform(BoneIdx);

		const FRotator DeltaRot = Cache.SmoothedPivotDelta * DeltaW;
		const float YawRad = FMath::DegreesToRadians(DeltaRot.Yaw);
		const float PitchRad = FMath::DegreesToRadians(DeltaRot.Pitch);
		const float RollRad = FMath::DegreesToRadians(DeltaRot.Roll);

		// Apply around stable component-space axes (reduces violent twist from bone-axis quirks).
		const FQuat QYaw(FVector(0.f, 0.f, 1.f), YawRad);
		const FQuat QPitch(FVector(0.f, 1.f, 0.f), PitchRad);
		const FQuat QRoll(FVector(1.f, 0.f, 0.f), RollRad);
		const FQuat AddQ = (QYaw * QPitch * QRoll).GetNormalized();

		BoneCS.SetRotation((AddQ * BoneCS.GetRotation()).GetNormalized());

		Output.Pose.SetComponentSpaceTransform(BoneIdx, BoneCS);
		OutBoneTransforms.Add(FBoneTransform(BoneIdx, BoneCS));
	}

	if (ShouldDrawDebug(*this) && Output.AnimInstanceProxy)
	{
		const FTransform& CompTM = Output.AnimInstanceProxy->GetComponentTransform();
		const FBoneContainer& BC = Output.Pose.GetPose().GetBoneContainer();
		FCSPose<FCompactPose>& CSPose = Output.Pose;

		DebugDrawChainOutlineAndAxes(
			Output.AnimInstanceProxy,
			CompTM,
			CSPose,
			BC,
			Cache.ChainBones,
			Cache.PivotBone,
			Cache.SmoothedPivotDelta,
			nullptr,
			3.2f);
	}
}

// ---------------- Hinge (true hinge + debug + SIGNED per-slot) ----------------

void FAnimNode_AegisProceduralMotionDriver::ApplyHingeChain(
	FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms,
	const FAegisChainDef_Inline& Chain,
	FAegisActionChainRuntimeCache& Cache,
	float Time01,
	float ActionAlpha,
	float DT)
{
	if (Cache.HingeBones.Num() == 0) return;

	const float T = FMath::Clamp(Time01, 0.f, 1.f);
	const float A = FMath::Clamp(ActionAlpha, 0.f, 1.f);

	const FVector ParentAxisPS = Chain.ParentSpaceAxis.GetSafeNormal();
	if (ParentAxisPS.IsNearlyZero()) return;

	// 1) Build targets per hinge bone (unsigned or signed per-slot)
	TArray<float> TargetDeg;
	TargetDeg.Init(0.f, Cache.HingeBones.Num());

	auto HingeIndexFromName = [&](const FName BoneName) -> int32
		{
			if (BoneName.IsNone()) return INDEX_NONE;
			const FBoneContainer& BC = Output.Pose.GetPose().GetBoneContainer();
			const int32 PoseIndex = BC.GetPoseBoneIndexForBoneName(BoneName);
			FCompactPoseBoneIndex CIdx(INDEX_NONE);
			if (!PoseIndexToCompact(BC, PoseIndex, CIdx)) return INDEX_NONE;
			return Cache.HingeBones.IndexOfByKey(CIdx);
		};

	for (const FAegisAutoHingePhase& Phase : Chain.HingePhases)
	{
		float Window = 1.f;
		if (Phase.Window01) Window = Phase.Window01->GetFloatValue(T);
		if (Window < Phase.ActiveThreshold) continue;

		const float PhaseAlpha = Window;

		for (const FAegisHingeCurveSlot& Slot : Phase.Hinges)
		{
			const int32 Idx = HingeIndexFromName(Slot.HingeBone);
			if (Idx == INDEX_NONE) continue;

			const float V = EvalHingeCurveSlot(Slot, T); // <-- signed or unsigned
			TargetDeg[Idx] += V * Chain.MaxDegreesScale * PhaseAlpha;
		}
	}

	for (int32 i = 0; i < TargetDeg.Num(); ++i)
	{
		TargetDeg[i] *= A;

		if (Chain.bClampDegrees)
		{
			TargetDeg[i] = FMath::Clamp(TargetDeg[i], Chain.MinDegrees, Chain.MaxDegreesClamp);
		}
	}

	// 2) Smooth per hinge bone
	const float S = HalfLifeAlpha(DT, Chain.SmoothingHalfLife);
	for (int32 i = 0; i < Cache.SmoothedHingeAnglesDeg.Num(); ++i)
	{
		Cache.SmoothedHingeAnglesDeg[i] = FMath::Lerp(Cache.SmoothedHingeAnglesDeg[i], TargetDeg[i], S);
	}

	// 3) Apply hinges parent->child as true hinge:
	//    modify LOCAL rotation, rebuild CS = Local * ParentCS, update pose for child propagation.
	const FBoneContainer& BC = Output.Pose.GetPose().GetBoneContainer();

	// Apply in parent->child order by depth
	TArray<int32> Indices;
	Indices.Reserve(Cache.HingeBones.Num());
	for (int32 i = 0; i < Cache.HingeBones.Num(); ++i) Indices.Add(i);

	auto Depth = [&](FCompactPoseBoneIndex Bone) -> int32
		{
			int32 D = 0;
			FCompactPoseBoneIndex Cur = Bone;
			while (Cur != INDEX_NONE && D < 512)
			{
				Cur = BC.GetParentBoneIndex(Cur);
				++D;
			}
			return D;
		};

	Indices.Sort([&](int32 AIdx, int32 BIdx)
		{
			return Depth(Cache.HingeBones[AIdx]) < Depth(Cache.HingeBones[BIdx]);
		});

	for (int32 ListI : Indices)
	{
		const FCompactPoseBoneIndex BoneIdx = Cache.HingeBones[ListI];
		const FCompactPoseBoneIndex ParentIdx = BC.GetParentBoneIndex(BoneIdx);
		if (ParentIdx == INDEX_NONE) continue;

		const float Deg = Cache.SmoothedHingeAnglesDeg[ListI];
		if (FMath::IsNearlyZero(Deg, 0.0001f)) continue;

		const FTransform ParentCS = Output.Pose.GetComponentSpaceTransform(ParentIdx);
		FTransform BoneLS = Output.Pose.GetLocalSpaceTransform(BoneIdx);

		// Axis authored in parent space -> component space using parent rotation
		const FVector AxisCS = ParentCS.GetRotation().RotateVector(ParentAxisPS).GetSafeNormal();

		// Convert axis into bone local space (apply local hinge)
		const FVector AxisPS = ParentCS.GetRotation().Inverse().RotateVector(AxisCS);
		const FVector AxisLS = BoneLS.GetRotation().Inverse().RotateVector(AxisPS).GetSafeNormal();

		const FQuat DeltaLocal(AxisLS, FMath::DegreesToRadians(Deg));
		BoneLS.SetRotation((DeltaLocal * BoneLS.GetRotation()).GetNormalized());

		const FTransform BoneCS = BoneLS * ParentCS;

		Output.Pose.SetComponentSpaceTransform(BoneIdx, BoneCS);
		OutBoneTransforms.Add(FBoneTransform(BoneIdx, BoneCS));
	}

	// Debug hinge chain overlay + per-joint angles
	if (ShouldDrawDebug(*this) && Output.AnimInstanceProxy)
	{
		const FTransform& CompTM = Output.AnimInstanceProxy->GetComponentTransform();
		FCSPose<FCompactPose>& CSPose = Output.Pose;

		const TArray<FCompactPoseBoneIndex>& Outline = (Cache.ChainBones.Num() > 0) ? Cache.ChainBones : Cache.HingeBones;
		const FCompactPoseBoneIndex LabelBone = (Outline.Num() > 0) ? Outline[0] : FCompactPoseBoneIndex(INDEX_NONE);

		const FString Extra = TEXT("Hinge Chain (signed per-slot)\n(angles shown per joint)");
		DebugDrawChainOutlineAndAxes(
			Output.AnimInstanceProxy,
			CompTM,
			CSPose,
			BC,
			Outline,
			LabelBone,
			FRotator::ZeroRotator,
			&Extra,
			3.0f);

		DebugDrawHingeAngles(
			Output.AnimInstanceProxy,
			CompTM,
			CSPose,
			Cache.HingeBones,
			Cache.SmoothedHingeAnglesDeg);
	}
}

// ---------------- Evaluate ----------------

void FAnimNode_AegisProceduralMotionDriver::EvaluateSkeletalControl_AnyThread(
	FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms)
{
	OutBoneTransforms.Reset();

	const UAegisProceduralActionAsset* Asset = nullptr;
	float Time01 = 0.f, ActionAlpha = 0.f;
	ResolveActionState(Asset, Time01, ActionAlpha);

	if (!Asset || Asset->Chains.Num() == 0)
	{
		return;
	}

	const FBoneContainer& BC = Output.Pose.GetPose().GetBoneContainer();
	EnsureCachesBuilt(Asset, BC);

	const float DT = Output.AnimInstanceProxy ? Output.AnimInstanceProxy->GetDeltaSeconds() : (1.f / 60.f);

	for (int32 i = 0; i < Asset->Chains.Num(); ++i)
	{
		if (!ActionChainCaches.IsValidIndex(i)) continue;

		const FAegisChainDef_Inline& Chain = Asset->Chains[i];
		FAegisActionChainRuntimeCache& Cache = ActionChainCaches[i];

		if (Cache.ChainBones.Num() == 0)
		{
			BuildCacheForChain(Cache, Chain, BC);
			if (Cache.ChainBones.Num() == 0) continue;
		}

		if (Chain.SolverType == EAegisChainSolverType::PivotChain)
		{
			const FRotator Target = EvalPivotTargetDeg(Chain, Time01, ActionAlpha);
			ApplyPivotChain(Output, OutBoneTransforms, Chain, Cache, Target, DT);
		}
		else if (Chain.SolverType == EAegisChainSolverType::HingeChainAuto)
		{
			ApplyHingeChain(Output, OutBoneTransforms, Chain, Cache, Time01, ActionAlpha, DT);
		}
	}

	OutBoneTransforms.Sort([](const FBoneTransform& A, const FBoneTransform& B)
		{
			return A.BoneIndex < B.BoneIndex;
		});
}
