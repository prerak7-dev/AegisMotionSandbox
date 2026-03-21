#include "AegisAction/AegisProceduralActionAsset.h"

#include "Algo/Reverse.h"
#include "Engine/SkeletalMesh.h"
#include "AegisMotionModule.h"

#if WITH_EDITOR
void UAegisProceduralActionAsset::PostLoad()
{
	Super::PostLoad();
	AutoFixupPhaseNamesAndDefaults_Internal(false);
	AutoPopulateSocketBones_Internal(false);
	AutoFixupPhaseBoneSlots_Internal(false);
}

void UAegisProceduralActionAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	AutoFixupPhaseNamesAndDefaults_Internal(false);
	AutoPopulateSocketBones_Internal(false);
	AutoFixupPhaseBoneSlots_Internal(false);
}
#endif

void UAegisProceduralActionAsset::AutoFixup_PhaseNamesAndDefaults()
{
	AutoFixupPhaseNamesAndDefaults_Internal(true);
	MarkPackageDirty();
}

void UAegisProceduralActionAsset::AutoFixup_PopulateSocketBones()
{
	AutoPopulateSocketBones_Internal(true);
	MarkPackageDirty();
}

void UAegisProceduralActionAsset::AutoFixup_PhaseBoneSlots()
{
	AutoFixupPhaseBoneSlots_Internal(true);
	MarkPackageDirty();
}

bool UAegisProceduralActionAsset::BuildRefSkeletonChainInclusive(
	const FReferenceSkeleton& RefSkel,
	FName StartBone,
	FName EndBone,
	TArray<int32>& OutSkelPath)
{
	OutSkelPath.Reset();

	const int32 StartIdx = RefSkel.FindBoneIndex(StartBone);
	const int32 EndIdx = RefSkel.FindBoneIndex(EndBone);
	if (StartIdx == INDEX_NONE || EndIdx == INDEX_NONE)
	{
		return false;
	}

	int32 Cur = EndIdx;
	int32 Safety = 0;
	while (Cur != INDEX_NONE && Safety++ < 512)
	{
		OutSkelPath.Add(Cur);
		if (Cur == StartIdx)
		{
			break;
		}
		Cur = RefSkel.GetParentIndex(Cur);
	}

	if (OutSkelPath.Num() == 0 || OutSkelPath.Last() != StartIdx)
	{
		OutSkelPath.Reset();
		return false;
	}

	Algo::Reverse(OutSkelPath);
	return true;
}

void UAegisProceduralActionAsset::NormalizePhaseTimings(FAegisActionPhaseBlendDef& Phase)
{
	Phase.StartTime01 = FMath::Clamp(Phase.StartTime01, 0.f, 1.f);
	Phase.PeakTime01 = FMath::Clamp(Phase.PeakTime01, 0.f, 1.f);
	Phase.EndTime01 = FMath::Clamp(Phase.EndTime01, 0.f, 1.f);

	if (Phase.PeakTime01 < Phase.StartTime01)
	{
		Phase.PeakTime01 = Phase.StartTime01;
	}
	if (Phase.EndTime01 < Phase.PeakTime01)
	{
		Phase.EndTime01 = Phase.PeakTime01;
	}

	Phase.EaseInExponent = FMath::Max(0.1f, Phase.EaseInExponent);
	Phase.EaseOutExponent = FMath::Max(0.1f, Phase.EaseOutExponent);
}

void UAegisProceduralActionAsset::EnsurePhaseSlotsMatchSocketBones(
	FAegisActionPhaseBlendDef& Phase,
	const TArray<FAegisSocketBoneDef>& SocketBonesOrdered)
{
	struct FExisting
	{
		TObjectPtr<UCurveFloat> Alpha = nullptr;
		TObjectPtr<UCurveFloat> RotX = nullptr;
		TObjectPtr<UCurveFloat> RotY = nullptr;
		TObjectPtr<UCurveFloat> RotZ = nullptr;
		TObjectPtr<UCurveFloat> PosX = nullptr;
		TObjectPtr<UCurveFloat> PosY = nullptr;
		TObjectPtr<UCurveFloat> PosZ = nullptr;
		float RotMul = 1.0f;
		float PosMul = 1.0f;
	};

	TMap<FName, FExisting> Existing;
	for (const FAegisSocketBonePhaseCurves& Slot : Phase.BoneCurves)
	{
		if (!Slot.BoneName.IsNone())
		{
			FExisting E;
			E.Alpha = Slot.Alpha01;
			E.RotX = Slot.RotX01;
			E.RotY = Slot.RotY01;
			E.RotZ = Slot.RotZ01;
			E.PosX = Slot.PosX01;
			E.PosY = Slot.PosY01;
			E.PosZ = Slot.PosZ01;
			E.RotMul = Slot.RotationMultiplier;
			E.PosMul = Slot.TranslationMultiplier;
			Existing.Add(Slot.BoneName, E);
		}
	}

	Phase.BoneCurves.Reset();
	Phase.BoneCurves.Reserve(SocketBonesOrdered.Num());
	for (const FAegisSocketBoneDef& SocketBone : SocketBonesOrdered)
	{
		FAegisSocketBonePhaseCurves Slot;
		Slot.BoneName = SocketBone.BoneName;
		if (const FExisting* E = Existing.Find(SocketBone.BoneName))
		{
			Slot.Alpha01 = E->Alpha;
			Slot.RotX01 = E->RotX;
			Slot.RotY01 = E->RotY;
			Slot.RotZ01 = E->RotZ;
			Slot.PosX01 = E->PosX;
			Slot.PosY01 = E->PosY;
			Slot.PosZ01 = E->PosZ;
			Slot.RotationMultiplier = E->RotMul;
			Slot.TranslationMultiplier = E->PosMul;
		}
		Phase.BoneCurves.Add(MoveTemp(Slot));
	}
}

void UAegisProceduralActionAsset::EnsurePhaseNames(FAegisChainDef_Inline& Chain, bool bLog)
{
	if (Chain.Phases.Num() == 0)
	{
		FAegisActionPhaseBlendDef Phase;
		Phase.PhaseName = FName(TEXT("Phase_01"));
		Phase.StartTime01 = 0.0f;
		Phase.PeakTime01 = 0.5f;
		Phase.EndTime01 = 1.0f;
		Chain.Phases.Add(Phase);
		if (bLog)
		{
			UE_LOG(LogAegisMotion, Warning, TEXT("[AegisActionAsset] Added default phase on chain '%s'"), *Chain.ChainName.ToString());
		}
	}

	TSet<FName> Used;
	for (int32 i = 0; i < Chain.Phases.Num(); ++i)
	{
		FAegisActionPhaseBlendDef& Phase = Chain.Phases[i];
		if (Phase.PhaseName.IsNone())
		{
			Phase.PhaseName = FName(*FString::Printf(TEXT("Phase_%02d"), i + 1));
		}
		if (Used.Contains(Phase.PhaseName))
		{
			Phase.PhaseName = FName(*FString::Printf(TEXT("%s_%02d"), *Phase.PhaseName.ToString(), i + 1));
		}
		Used.Add(Phase.PhaseName);
		NormalizePhaseTimings(Phase);
	}
}

void UAegisProceduralActionAsset::AutoFixupPhaseNamesAndDefaults_Internal(bool bLog)
{
	for (FAegisChainDef_Inline& Chain : Chains)
	{
		EnsurePhaseNames(Chain, bLog);
	}
}

void UAegisProceduralActionAsset::AutoPopulateSocketBones_Internal(bool bLog)
{
	if (!SkeletalMesh)
	{
		if (bLog)
		{
			UE_LOG(LogAegisMotion, Warning, TEXT("[AegisActionAsset] AutoPopulateSocketBones: SkeletalMesh is null"));
		}
		return;
	}

	const FReferenceSkeleton& RefSkel = SkeletalMesh->GetRefSkeleton();

	for (FAegisChainDef_Inline& Chain : Chains)
	{
		if (!Chain.bAutoPopulateSocketBonesFromChain || Chain.StartBone.IsNone() || Chain.EndBone.IsNone())
		{
			continue;
		}

		TArray<int32> SkelPath;
		if (!BuildRefSkeletonChainInclusive(RefSkel, Chain.StartBone, Chain.EndBone, SkelPath))
		{
			if (bLog)
			{
				UE_LOG(LogAegisMotion, Warning, TEXT("[AegisActionAsset] Chain '%s' invalid path %s->%s"),
					*Chain.ChainName.ToString(), *Chain.StartBone.ToString(), *Chain.EndBone.ToString());
			}
			continue;
		}

		TMap<FName, FAegisSocketBoneDef> Existing;
		for (const FAegisSocketBoneDef& Def : Chain.SocketBones)
		{
			if (!Def.BoneName.IsNone())
			{
				Existing.Add(Def.BoneName, Def);
			}
		}

		TArray<FAegisSocketBoneDef> NewSocketBones;
		NewSocketBones.Reserve(SkelPath.Num());
		for (int32 SkelIndex : SkelPath)
		{
			const int32 ParentIndex = RefSkel.GetParentIndex(SkelIndex);
			if (ParentIndex == INDEX_NONE)
			{
				continue;
			}

			const FName BoneName = RefSkel.GetBoneName(SkelIndex);
			if (const FAegisSocketBoneDef* ExistingDef = Existing.Find(BoneName))
			{
				NewSocketBones.Add(*ExistingDef);
			}
			else
			{
				FAegisSocketBoneDef Def;
				Def.BoneName = BoneName;
				NewSocketBones.Add(Def);
			}
		}

		Chain.SocketBones = MoveTemp(NewSocketBones);
		if (bLog)
		{
			UE_LOG(LogAegisMotion, Warning, TEXT("[AegisActionAsset] Chain '%s' AutoPopulateSocketBones=%d"), *Chain.ChainName.ToString(), Chain.SocketBones.Num());
		}
	}
}

void UAegisProceduralActionAsset::AutoFixupPhaseBoneSlots_Internal(bool bLog)
{
	for (FAegisChainDef_Inline& Chain : Chains)
	{
		EnsurePhaseNames(Chain, false);
		for (FAegisActionPhaseBlendDef& Phase : Chain.Phases)
		{
			EnsurePhaseSlotsMatchSocketBones(Phase, Chain.SocketBones);
		}
		if (bLog)
		{
			UE_LOG(LogAegisMotion, Warning, TEXT("[AegisActionAsset] Chain '%s' PhaseBoneSlots updated"), *Chain.ChainName.ToString());
		}
	}
}
