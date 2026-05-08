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



void UAegisProceduralActionAsset::CleanGeneratedNativeAction()
{
	Modify();

	PlaybackMode = EAegisActionPlaybackMode::GeneratedNativeQuaternion;
	MocapJointRemapTable.Reset();
	ImportedMocapBindings.Reset();
	LastImportedMocapJsonPath.Empty();

	// V30 safe generated defaults: let the leg IK solve planted feet, but do not translate
	// the whole generated pose by default. The old whole-body correction path could create
	// visible upper-body/head stretching when contact curves were active.
	FootLockSettings.bEnableGeneratedFootLock = true;
	FootLockSettings.bEnableGeneratedTwoBoneIK = true;
	FootLockSettings.LockActivationThreshold = 0.35f;
	FootLockSettings.LockReleaseThreshold = 0.08f;
	FootLockSettings.MaxRootCorrectionCm = 0.0f;
	FootLockSettings.TwoBoneIKRootCorrectionWeight = 0.0f;
	FootLockSettings.TwoBoneIKAlphaPower = 1.10f;
	FootLockSettings.ContactAlphaInterpSpeed = 18.0f;
	FootLockSettings.bAllowGeneratedRootCorrection = false;
	FootLockSettings.MaxTwoBoneIKCorrectionCm = 35.0f;
	FootLockSettings.bTranslateFootDescendantsWithIK = false;

	// Keep only generated/imported native full-body chains. This removes stale authored
	// hinge/pivot/remap data after importing AI-native UE mannequin JSON.
	for (int32 ChainIndex = Chains.Num() - 1; ChainIndex >= 0; --ChainIndex)
	{
		const FName ChainName = Chains[ChainIndex].ChainName;
		if (ChainName != FName(TEXT("Aegis_GeneratedNative_FullBody"))
			&& ChainName != FName(TEXT("Imported_CMU_Mixamo_FullBody")))
		{
			Chains.RemoveAt(ChainIndex);
		}
	}

	MarkPackageDirty();
}

void UAegisProceduralActionAsset::ResetMocapJointRemapTableToDefaults()
{
	Modify();

	MocapJointRemapTable.Reset();

	// Mixamo / generic names.
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("Hips"), TEXT("pelvis")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("Spine"), TEXT("spine_01")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("Spine1"), TEXT("spine_02")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("Chest"), TEXT("spine_02")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("Spine2"), TEXT("spine_03")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("UpperChest"), TEXT("spine_03")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("Neck"), TEXT("neck_01")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("Head"), TEXT("head")));

	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("RightUpLeg"), TEXT("thigh_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("RightLeg"), TEXT("calf_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("RightFoot"), TEXT("foot_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("RightToeBase"), TEXT("ball_r")));

	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("LeftUpLeg"), TEXT("thigh_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("LeftLeg"), TEXT("calf_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("LeftFoot"), TEXT("foot_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("LeftToeBase"), TEXT("ball_l")));

	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("RightShoulder"), TEXT("clavicle_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("RightArm"), TEXT("upperarm_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("RightForeArm"), TEXT("lowerarm_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("RightHand"), TEXT("hand_r")));

	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("LeftShoulder"), TEXT("clavicle_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("LeftArm"), TEXT("upperarm_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("LeftForeArm"), TEXT("lowerarm_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("LeftHand"), TEXT("hand_l")));

	// CMU ASF/AMC subject skeleton names.
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("root"), TEXT("pelvis")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("lowerback"), TEXT("spine_01")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("upperback"), TEXT("spine_02")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("thorax"), TEXT("spine_03")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("lowerneck"), TEXT("neck_01")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("upperneck"), TEXT("neck_01")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("rclavicle"), TEXT("clavicle_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("rhumerus"), TEXT("upperarm_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("rradius"), TEXT("lowerarm_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("rwrist"), TEXT("hand_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("rhand"), TEXT("hand_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("rfingers"), TEXT("hand_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("rthumb"), TEXT("hand_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("lclavicle"), TEXT("clavicle_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("lhumerus"), TEXT("upperarm_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("lradius"), TEXT("lowerarm_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("lwrist"), TEXT("hand_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("lhand"), TEXT("hand_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("lfingers"), TEXT("hand_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("lthumb"), TEXT("hand_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("rfemur"), TEXT("thigh_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("rtibia"), TEXT("calf_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("rfoot"), TEXT("foot_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("rtoes"), TEXT("ball_r")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("lfemur"), TEXT("thigh_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("ltibia"), TEXT("calf_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("lfoot"), TEXT("foot_l")));
	MocapJointRemapTable.Add(FAegisMocapJointRemap(TEXT("ltoes"), TEXT("ball_l")));

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
		TObjectPtr<UCurveFloat> RotW = nullptr;
		TObjectPtr<UCurveFloat> PosX = nullptr;
		TObjectPtr<UCurveFloat> PosY = nullptr;
		TObjectPtr<UCurveFloat> PosZ = nullptr;
		TObjectPtr<UCurveFloat> IkLockAlpha = nullptr;
		TObjectPtr<UCurveFloat> PlantLockAlpha = nullptr;
		float RotMul = 1.0f;
		float PosMul = 1.0f;
		EAegisCurveValueSpace RotationValueSpace = EAegisCurveValueSpace::NormalizedProcedural;
		EAegisCurveValueSpace TranslationValueSpace = EAegisCurveValueSpace::NormalizedProcedural;
		EAegisBvhRotationOrder RotationOrder = EAegisBvhRotationOrder::ZXY;
		bool bBypassSmoothingForRawBvh = false;
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
			E.RotW = Slot.RotW01;
			E.PosX = Slot.PosX01;
			E.PosY = Slot.PosY01;
			E.PosZ = Slot.PosZ01;
			E.IkLockAlpha = Slot.IkLockAlpha01;
			E.PlantLockAlpha = Slot.PlantLockAlpha01;
			E.RotMul = Slot.RotationMultiplier;
			E.PosMul = Slot.TranslationMultiplier;
			E.RotationValueSpace = Slot.RotationValueSpace;
			E.TranslationValueSpace = Slot.TranslationValueSpace;
			E.RotationOrder = Slot.RotationOrder;
			E.bBypassSmoothingForRawBvh = Slot.bBypassSmoothingForRawBvh;
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
			Slot.RotW01 = E->RotW;
			Slot.PosX01 = E->PosX;
			Slot.PosY01 = E->PosY;
			Slot.PosZ01 = E->PosZ;
			Slot.IkLockAlpha01 = E->IkLockAlpha;
			Slot.PlantLockAlpha01 = E->PlantLockAlpha;
			Slot.RotationMultiplier = E->RotMul;
			Slot.TranslationMultiplier = E->PosMul;
			Slot.RotationValueSpace = E->RotationValueSpace;
			Slot.TranslationValueSpace = E->TranslationValueSpace;
			Slot.RotationOrder = E->RotationOrder;
			Slot.bBypassSmoothingForRawBvh = E->bBypassSmoothingForRawBvh;
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
