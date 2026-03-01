#include "AegisAction/AegisProceduralActionAsset.h"

#include "Algo/Reverse.h"
#include "Engine/SkeletalMesh.h"

#if WITH_EDITOR
void UAegisProceduralActionAsset::PostLoad()
{
	Super::PostLoad();

	AutoFixupPhaseNamesAndDefaults_Internal(false);
	AutoFixupHingePhaseSlots_Internal(false);
}

void UAegisProceduralActionAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	AutoFixupPhaseNamesAndDefaults_Internal(false);
	AutoFixupHingePhaseSlots_Internal(false);
}
#endif

void UAegisProceduralActionAsset::AutoFixup_PhaseNamesAndDefaults()
{
	AutoFixupPhaseNamesAndDefaults_Internal(true);
	MarkPackageDirty();
}

void UAegisProceduralActionAsset::AutoFixup_HingePhaseSlots()
{
	AutoFixupHingePhaseSlots_Internal(true);
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
	while (Cur != INDEX_NONE)
	{
		OutSkelPath.Add(Cur);
		if (Cur == StartIdx) break;
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

void UAegisProceduralActionAsset::EnsurePhaseSlotsMatchHinges(
	FAegisAutoHingePhase& Phase,
	const TArray<FName>& HingeBonesOrdered)
{
	struct FExisting
	{
		TObjectPtr<UCurveFloat> Curve = nullptr;
		bool bSigned = false;
	};

	TMap<FName, FExisting> Existing;
	for (const FAegisHingeCurveSlot& Slot : Phase.Hinges)
	{
		if (!Slot.HingeBone.IsNone())
		{
			Existing.Add(Slot.HingeBone, { Slot.Angle01, Slot.bSignedAngle });
		}
	}

	Phase.Hinges.Reset();
	Phase.Hinges.Reserve(HingeBonesOrdered.Num());

	for (const FName Bone : HingeBonesOrdered)
	{
		FAegisHingeCurveSlot NewSlot;
		NewSlot.HingeBone = Bone;

		if (const FExisting* E = Existing.Find(Bone))
		{
			NewSlot.Angle01 = E->Curve;
			NewSlot.bSignedAngle = E->bSigned;
		}

		Phase.Hinges.Add(MoveTemp(NewSlot));
	}
}

void UAegisProceduralActionAsset::EnsurePhaseNamesPivot(FAegisChainDef_Inline& Chain, bool bLog)
{
	if (Chain.SolverType != EAegisChainSolverType::PivotChain)
	{
		return;
	}

	if (Chain.PivotPhases.Num() == 0)
	{
		FAegisPhaseCurvesPRY P;
		P.PhaseName = FName(TEXT("Pivot_01"));
		P.ActiveThreshold = 0.01f;
		Chain.PivotPhases.Add(P);

		if (bLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AegisActionAsset] Added default PivotPhases on chain '%s'"), *Chain.ChainName.ToString());
		}
	}

	TSet<FName> Used;
	for (int32 i = 0; i < Chain.PivotPhases.Num(); ++i)
	{
		FAegisPhaseCurvesPRY& Phase = Chain.PivotPhases[i];

		if (Phase.PhaseName.IsNone())
		{
			Phase.PhaseName = FName(*FString::Printf(TEXT("Pivot_%02d"), i + 1));
		}

		if (Used.Contains(Phase.PhaseName))
		{
			Phase.PhaseName = FName(*FString::Printf(TEXT("%s_%02d"), *Phase.PhaseName.ToString(), i + 1));
		}

		Used.Add(Phase.PhaseName);
	}
}

void UAegisProceduralActionAsset::EnsurePhaseNamesHinge(FAegisChainDef_Inline& Chain, bool bLog)
{
	if (Chain.SolverType != EAegisChainSolverType::HingeChainAuto)
	{
		return;
	}

	if (Chain.HingePhases.Num() == 0)
	{
		FAegisAutoHingePhase H;
		H.PhaseName = FName(TEXT("Hinge_01"));
		H.ActiveThreshold = 0.01f;
		Chain.HingePhases.Add(H);

		if (bLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AegisActionAsset] Added default HingePhases on chain '%s'"), *Chain.ChainName.ToString());
		}
	}

	TSet<FName> Used;
	for (int32 i = 0; i < Chain.HingePhases.Num(); ++i)
	{
		FAegisAutoHingePhase& Phase = Chain.HingePhases[i];

		if (Phase.PhaseName.IsNone())
		{
			Phase.PhaseName = FName(*FString::Printf(TEXT("Hinge_%02d"), i + 1));
		}

		if (Used.Contains(Phase.PhaseName))
		{
			Phase.PhaseName = FName(*FString::Printf(TEXT("%s_%02d"), *Phase.PhaseName.ToString(), i + 1));
		}

		Used.Add(Phase.PhaseName);
	}
}

void UAegisProceduralActionAsset::AutoFixupPhaseNamesAndDefaults_Internal(bool bLog)
{
	for (FAegisChainDef_Inline& C : Chains)
	{
		EnsurePhaseNamesPivot(C, bLog);
		EnsurePhaseNamesHinge(C, bLog);
	}
}

void UAegisProceduralActionAsset::AutoFixupHingePhaseSlots_Internal(bool bLog)
{
	if (!SkeletalMesh)
	{
		if (bLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AegisActionAsset] AutoFixupHingePhaseSlots: SkeletalMesh is null"));
		}
		return;
	}

	const FReferenceSkeleton& RefSkel = SkeletalMesh->GetRefSkeleton();

	for (FAegisChainDef_Inline& C : Chains)
	{
		if (C.SolverType != EAegisChainSolverType::HingeChainAuto)
		{
			continue;
		}

		if (C.StartBone.IsNone() || C.EndBone.IsNone())
		{
			continue;
		}

		TArray<int32> SkelPath;
		if (!BuildRefSkeletonChainInclusive(RefSkel, C.StartBone, C.EndBone, SkelPath))
		{
			if (bLog)
			{
				UE_LOG(LogTemp, Warning, TEXT("[AegisActionAsset] Chain '%s' invalid path %s->%s"),
					*C.ChainName.ToString(),
					*C.StartBone.ToString(),
					*C.EndBone.ToString());
			}
			continue;
		}

		TArray<FName> HingeBonesOrdered;

		const int32 StartSkelIdx = (SkelPath.Num() > 0) ? SkelPath[0] : INDEX_NONE;
		const int32 StartParent = (StartSkelIdx != INDEX_NONE) ? RefSkel.GetParentIndex(StartSkelIdx) : INDEX_NONE;

		const int32 FirstHingeIndex = (StartParent == INDEX_NONE) ? 1 : 0;

		for (int32 i = FirstHingeIndex; i < SkelPath.Num(); ++i)
		{
			HingeBonesOrdered.Add(RefSkel.GetBoneName(SkelPath[i]));
		}

		for (FAegisAutoHingePhase& Phase : C.HingePhases)
		{
			EnsurePhaseSlotsMatchHinges(Phase, HingeBonesOrdered);
		}

		if (bLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AegisActionAsset] Chain '%s' AutoHingeSlots=%d"), *C.ChainName.ToString(), HingeBonesOrdered.Num());
		}
	}
}
