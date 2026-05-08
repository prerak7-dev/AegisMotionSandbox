#include "AegisProceduralActionComponentVisualizer.h"

#include "AegisAction/AegisProceduralActionAsset.h"
#include "AegisAction/AegisProceduralActionComponent.h"

#include "Algo/Reverse.h"
#include "CanvasItem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "Widgets/Text/STextBlock.h"

IMPLEMENT_HIT_PROXY(HAegisActionBoneVisProxy, HComponentVisProxy);

static const FLinearColor AegisDebugConeColor(0.0f, 0.8f, 1.0f, 1.0f);
static const FLinearColor AegisGhostPoseColor(1.0f, 0.35f, 1.0f, 0.9f);

static FORCEINLINE bool IsEditorScrubActive(const UAegisProceduralActionComponent* Comp)
{
	return Comp && Comp->bDebugScrubEnabled && Comp->GetCurrentActionAsset() != nullptr;
}

static FORCEINLINE float CurveToSignedSmartEditor(float V)
{
	if (V < 0.f || V > 1.f)
	{
		return V;
	}
	return (V - 0.5f) * 2.f;
}

static FORCEINLINE float CurveToUnsigned01Editor(float V)
{
	return FMath::Clamp(V, 0.f, 1.f);
}

USkeletalMeshComponent* FAegisProceduralActionComponentVisualizer::FindSkeletalMeshComponent(const UAegisProceduralActionComponent* Comp) const
{
	if (!Comp || !Comp->GetOwner())
	{
		return nullptr;
	}
	return Comp->GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
}

float FAegisProceduralActionComponentVisualizer::EvalAutomaticPhaseWeight(const FAegisActionPhaseBlendDef& Phase, float Time01)
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

void FAegisProceduralActionComponentVisualizer::EvalGhostOffsetForBone(const FAegisChainDef_Inline& Chain, const FAegisSocketBoneDef& SocketBone, float Time01, FRotator& OutRotDeg, FVector& OutTransLS)
{
	const float T = FMath::Clamp(Time01, 0.f, 1.f);
	float ChainAlpha = 1.f;
	if (Chain.ChainAlpha01)
	{
		ChainAlpha = CurveToUnsigned01Editor(Chain.ChainAlpha01->GetFloatValue(T));
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
			W *= CurveToUnsigned01Editor(Phase.PhaseAlpha01->GetFloatValue(T));
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
		const float NormalizedWeight = TotalWeight > KINDA_SMALL_NUMBER ? PhaseWeight / TotalWeight : 0.f;
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
			SlotAlpha = CurveToUnsigned01Editor(FoundSlot->Alpha01->GetFloatValue(T));
		}
		const float W = NormalizedWeight * ChainAlpha * SocketBone.BoneWeight * SlotAlpha;
		if (W <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float RotMul = FoundSlot->RotationMultiplier;
		const float PosMul = FoundSlot->TranslationMultiplier;

		const bool bRawRotation = FoundSlot->RotationValueSpace == EAegisCurveValueSpace::RawBvhLocal;
		const bool bRawTranslation = FoundSlot->TranslationValueSpace == EAegisCurveValueSpace::RawBvhLocal;

		auto EvalRotationAxisDeg = [&](const TObjectPtr<UCurveFloat>& Curve, float MaxDegrees) -> float
		{
			if (!Curve)
			{
				return 0.f;
			}

			const float V = Curve->GetFloatValue(T);
			return bRawRotation ? V : CurveToSignedSmartEditor(V) * MaxDegrees;
		};

		auto EvalTranslationAxisCm = [&](const TObjectPtr<UCurveFloat>& Curve, float MaxCm) -> float
		{
			if (!Curve)
			{
				return 0.f;
			}

			const float V = Curve->GetFloatValue(T);
			return bRawTranslation ? V : CurveToSignedSmartEditor(V) * MaxCm;
		};

		RotX += EvalRotationAxisDeg(FoundSlot->RotX01, SocketBone.Limits.MaxRotationDegrees.X) * RotMul * W;
		RotY += EvalRotationAxisDeg(FoundSlot->RotY01, SocketBone.Limits.MaxRotationDegrees.Y) * RotMul * W;
		RotZ += EvalRotationAxisDeg(FoundSlot->RotZ01, SocketBone.Limits.MaxRotationDegrees.Z) * RotMul * W;

		Trans.X += EvalTranslationAxisCm(FoundSlot->PosX01, SocketBone.Limits.MaxTranslationCm.X) * PosMul * W;
		Trans.Y += EvalTranslationAxisCm(FoundSlot->PosY01, SocketBone.Limits.MaxTranslationCm.Y) * PosMul * W;
		Trans.Z += EvalTranslationAxisCm(FoundSlot->PosZ01, SocketBone.Limits.MaxTranslationCm.Z) * PosMul * W;
	}

	OutRotDeg = FRotator(RotY, RotZ, RotX);
	OutTransLS = Trans;
}

void FAegisProceduralActionComponentVisualizer::BuildChainBones(const USkeletalMeshComponent* SkelComp, FName StartBone, FName EndBone, TArray<int32>& OutBoneIndices)
{
	OutBoneIndices.Reset();
	if (!SkelComp || !SkelComp->GetSkeletalMeshAsset())
	{
		return;
	}

	const FReferenceSkeleton& RefSkel = SkelComp->GetSkeletalMeshAsset()->GetRefSkeleton();
	const int32 StartIdx = RefSkel.FindBoneIndex(StartBone);
	const int32 EndIdx = RefSkel.FindBoneIndex(EndBone);
	if (StartIdx == INDEX_NONE || EndIdx == INDEX_NONE)
	{
		return;
	}

	TArray<int32> Temp;
	int32 Cur = EndIdx;
	int32 Safety = 0;
	while (Cur != INDEX_NONE && Safety++ < 512)
	{
		Temp.Add(Cur);
		if (Cur == StartIdx)
		{
			break;
		}
		Cur = RefSkel.GetParentIndex(Cur);
	}
	Algo::Reverse(Temp);
	OutBoneIndices = MoveTemp(Temp);
}

void FAegisProceduralActionComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UAegisProceduralActionComponent* Comp = Cast<UAegisProceduralActionComponent>(Component);
	if (!IsEditorScrubActive(Comp))
	{
		return;
	}

	USkeletalMeshComponent* SkelComp = FindSkeletalMeshComponent(Comp);
	if (!SkelComp || !SkelComp->GetSkeletalMeshAsset())
	{
		return;
	}

	const UAegisProceduralActionAsset* Asset = Comp->GetCurrentActionAsset();
	if (!Asset)
	{
		return;
	}

	const FReferenceSkeleton& RefSkel = SkelComp->GetSkeletalMeshAsset()->GetRefSkeleton();
	const float Time01 = Comp->GetEffectiveTime01();
	BoneToChainIndices.Reset();
	PendingLabels.Reset();

	for (int32 ChainIdx = 0; ChainIdx < Asset->Chains.Num(); ++ChainIdx)
	{
		const FAegisChainDef_Inline& Chain = Asset->Chains[ChainIdx];
		if (!Chain.bApplyToChain)
		{
			continue;
		}

		for (const FAegisSocketBoneDef& SocketBone : Chain.SocketBones)
		{
			if (SocketBone.BoneName.IsNone())
			{
				continue;
			}

			const int32 BoneIdx = RefSkel.FindBoneIndex(SocketBone.BoneName);
			if (BoneIdx == INDEX_NONE)
			{
				continue;
			}

			BoneToChainIndices.FindOrAdd(SocketBone.BoneName).AddUnique(ChainIdx);
			const FVector WorldPos = SkelComp->GetBoneLocation(SocketBone.BoneName);
			PDI->SetHitProxy(new HAegisActionBoneVisProxy(Comp, SocketBone.BoneName));
			PDI->DrawPoint(WorldPos, FLinearColor::Yellow, 11.f, SDPG_Foreground);
			PDI->SetHitProxy(nullptr);

			const FQuat BoneQuat = SkelComp->GetBoneQuaternion(SocketBone.BoneName);
			const FVector Forward = BoneQuat.GetForwardVector();
			FVector Right = BoneQuat.GetRightVector();
			FVector Up = BoneQuat.GetUpVector();
			const float ConeAngleDeg = FMath::Max3(SocketBone.Limits.MaxRotationDegrees.X, SocketBone.Limits.MaxRotationDegrees.Y, SocketBone.Limits.MaxRotationDegrees.Z);
			const float ConeLength = 10.f;
			const float ConeRadius = ConeLength * FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(ConeAngleDeg, 1.f, 89.f)));
			const FVector Tip = WorldPos + Forward * ConeLength;
			FVector FirstRim = FVector::ZeroVector;
			FVector PrevRim = FVector::ZeroVector;
			for (int32 Segment = 0; Segment < 12; ++Segment)
			{
				const float Angle = (2.f * PI * static_cast<float>(Segment)) / 12.f;
				const FVector Rim = Tip + (Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * ConeRadius;
				PDI->DrawLine(WorldPos, Rim, AegisDebugConeColor, SDPG_Foreground, 0.8f);
				if (Segment > 0)
				{
					PDI->DrawLine(PrevRim, Rim, AegisDebugConeColor, SDPG_Foreground, 0.6f);
				}
				else
				{
					FirstRim = Rim;
				}
				PrevRim = Rim;
			}
			PDI->DrawLine(PrevRim, FirstRim, AegisDebugConeColor, SDPG_Foreground, 0.6f);

			if (Chain.bDrawGhostPose)
			{
				FRotator GhostRotDeg = FRotator::ZeroRotator;
				FVector GhostTransLS = FVector::ZeroVector;
				EvalGhostOffsetForBone(Chain, SocketBone, Time01, GhostRotDeg, GhostTransLS);

				const FVector GhostWorldPos = WorldPos + SkelComp->GetComponentTransform().TransformVectorNoScale(BoneQuat.RotateVector(GhostTransLS));
				PDI->DrawPoint(GhostWorldPos, AegisGhostPoseColor, 8.f, SDPG_Foreground);
				PDI->DrawLine(WorldPos, GhostWorldPos, AegisGhostPoseColor, SDPG_Foreground, 1.0f);
				const FVector BoneX = BoneQuat.RotateVector(FVector::ForwardVector);
				const FVector BoneY = BoneQuat.RotateVector(FVector::RightVector);
				const FVector BoneZ = BoneQuat.RotateVector(FVector::UpVector);
				const FQuat GhostDelta = FQuat(BoneX, FMath::DegreesToRadians(GhostRotDeg.Roll)) * FQuat(BoneY, FMath::DegreesToRadians(GhostRotDeg.Pitch)) * FQuat(BoneZ, FMath::DegreesToRadians(GhostRotDeg.Yaw));
				const FQuat GhostQuat = GhostDelta * BoneQuat;
				PDI->DrawLine(GhostWorldPos, GhostWorldPos + GhostQuat.GetForwardVector() * 8.f, AegisGhostPoseColor, SDPG_Foreground, 1.0f);
			}

			FAegisViewportLabel Label;
			Label.WorldPos = WorldPos + FVector(0.f, 0.f, 8.f);
			Label.Text = FString::Printf(TEXT("%s | t=%.3f"), *SocketBone.BoneName.ToString(), Time01);
			PendingLabels.Add(MoveTemp(Label));
		}
	}
}

void FAegisProceduralActionComponentVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	const UAegisProceduralActionComponent* Comp = Cast<UAegisProceduralActionComponent>(Component);
	if (!IsEditorScrubActive(Comp) || !Canvas || !View || !GEngine)
	{
		return;
	}

	UFont* Font = GEngine->GetSmallFont();
	if (!Font)
	{
		return;
	}

	for (const FAegisViewportLabel& Label : PendingLabels)
	{
		FVector2D ScreenPos;
		if (View->WorldToPixel(Label.WorldPos, ScreenPos))
		{
			FCanvasTextItem TextItem(ScreenPos, FText::FromString(Label.Text), Font, FLinearColor::White);
			TextItem.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(TextItem);
		}
	}
}

bool FAegisProceduralActionComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	LastComponent.Reset();
	SelectedBone = NAME_None;
	SelectedCurves.Reset();

	if (VisProxy && VisProxy->IsA(HAegisActionBoneVisProxy::StaticGetType()))
	{
		const HAegisActionBoneVisProxy* Proxy = static_cast<const HAegisActionBoneVisProxy*>(VisProxy);
		LastComponent = Cast<UAegisProceduralActionComponent>(const_cast<UActorComponent*>(Proxy->Component.Get()));
		SelectedBone = Proxy->BoneName;
		SelectedChainIndices = BoneToChainIndices.FindRef(SelectedBone);
		if (UAegisProceduralActionComponent* Comp = LastComponent.Get())
		{
			if (const UAegisProceduralActionAsset* Asset = Comp->GetCurrentActionAsset())
			{
				BuildSelectedCurveList(Asset, Comp->GetEffectiveTime01(), SelectedBone);
			}
		}
		return true;
	}

	return false;
}

void FAegisProceduralActionComponentVisualizer::BuildSelectedCurveList(const UAegisProceduralActionAsset* Asset, float Time01, FName BoneName) const
{
	SelectedCurves.Reset();
	if (!Asset || BoneName.IsNone())
	{
		return;
	}

	const float T = FMath::Clamp(Time01, 0.f, 1.f);
	const bool bHasMembership = SelectedChainIndices.Num() > 0;

	for (int32 ChainIdx = 0; ChainIdx < Asset->Chains.Num(); ++ChainIdx)
	{
		const FAegisChainDef_Inline& Chain = Asset->Chains[ChainIdx];
		if (!Chain.bApplyToChain)
		{
			continue;
		}

		const bool bBoneInChain = bHasMembership ? SelectedChainIndices.Contains(ChainIdx) : (BoneName == Chain.StartBone || BoneName == Chain.EndBone);
		if (!bBoneInChain)
		{
			continue;
		}

		if (Chain.ChainAlpha01)
		{
			SelectedCurves.Add({ Chain.ChainAlpha01, FString::Printf(TEXT("%s / ChainAlpha01"), *Chain.ChainName.ToString()), T, Chain.ChainAlpha01->GetFloatValue(T) });
		}

		for (const FAegisActionPhaseBlendDef& Phase : Chain.Phases)
		{
			if (Phase.PhaseAlpha01)
			{
				SelectedCurves.Add({ Phase.PhaseAlpha01, FString::Printf(TEXT("%s / %s / PhaseAlpha01"), *Chain.ChainName.ToString(), *Phase.PhaseName.ToString()), T, Phase.PhaseAlpha01->GetFloatValue(T) });
			}

			for (const FAegisSocketBonePhaseCurves& Slot : Phase.BoneCurves)
			{
				if (Slot.BoneName != BoneName)
				{
					continue;
				}

				auto AddCurve = [&](UCurveFloat* Curve, const TCHAR* Suffix)
				{
					if (Curve)
					{
						SelectedCurves.Add({ Curve, FString::Printf(TEXT("%s / %s / %s / %s"), *Chain.ChainName.ToString(), *Phase.PhaseName.ToString(), *BoneName.ToString(), Suffix), T, Curve->GetFloatValue(T) });
					}
				};

				AddCurve(Slot.Alpha01, TEXT("Alpha01"));
				AddCurve(Slot.RotX01, TEXT("RotX01"));
				AddCurve(Slot.RotY01, TEXT("RotY01"));
				AddCurve(Slot.RotZ01, TEXT("RotZ01"));
				AddCurve(Slot.PosX01, TEXT("PosX01"));
				AddCurve(Slot.PosY01, TEXT("PosY01"));
				AddCurve(Slot.PosZ01, TEXT("PosZ01"));
			}
		}
	}
}

void FAegisProceduralActionComponentVisualizer::AddOrUpdateKey(UCurveFloat* Curve, float Time01, float NewValue)
{
	if (!Curve)
	{
		return;
	}

	const float T = FMath::Clamp(Time01, 0.f, 1.f);
	FScopedTransaction Tx(NSLOCTEXT("AegisMotion", "AegisCurveEdit", "Edit Aegis Curve Key"));
	Curve->Modify();
	FRichCurve& RC = Curve->FloatCurve;
	FKeyHandle Handle = RC.FindKey(T);
	if (Handle == FKeyHandle::Invalid())
	{
		Handle = RC.AddKey(T, NewValue);
	}
	else
	{
		RC.SetKeyValue(Handle, NewValue);
	}
	RC.AutoSetTangents();
	Curve->MarkPackageDirty();
	Curve->PostEditChange();
}

TSharedPtr<SWidget> FAegisProceduralActionComponentVisualizer::GenerateContextMenu() const
{
	UAegisProceduralActionComponent* Comp = LastComponent.Get();
	if (!Comp || SelectedBone.IsNone() || SelectedCurves.Num() == 0)
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.AddWidget(SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Bone: %s"), *SelectedBone.ToString()))), FText::GetEmpty(), true);

	const float T = Comp->GetEffectiveTime01();
	for (const FCandidateCurve& Candidate : SelectedCurves)
	{
		UCurveFloat* Curve = Candidate.Curve.Get();
		if (!Curve)
		{
			continue;
		}

		MenuBuilder.AddMenuEntry(
			FText::FromString(Candidate.Label),
			FText::FromString(TEXT("Add or update a key at the current scrub time using the sampled value.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([Curve, T, Raw = Candidate.RawValue]()
			{
				FAegisProceduralActionComponentVisualizer::AddOrUpdateKey(Curve, T, Raw);
			})));
	}

	return MenuBuilder.MakeWidget();
}
