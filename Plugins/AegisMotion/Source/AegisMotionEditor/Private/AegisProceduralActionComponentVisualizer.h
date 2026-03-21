#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"
#include "UObject/WeakObjectPtr.h"

class UAegisProceduralActionComponent;
class UAegisProceduralActionAsset;
class USkeletalMeshComponent;
class UCurveFloat;
struct FAegisSocketBonePhaseCurves;
struct FAegisSocketBoneDef;
struct FAegisChainDef_Inline;
struct FAegisActionPhaseBlendDef;

struct HAegisActionBoneVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HAegisActionBoneVisProxy(const UActorComponent* InComponent, FName InBoneName)
		: HComponentVisProxy(InComponent, HPP_Wireframe)
		, BoneName(InBoneName)
	{
	}

	FName BoneName = NAME_None;
};

struct FCandidateCurve
{
	TWeakObjectPtr<UCurveFloat> Curve;
	FString Label;
	float Time01 = 0.f;
	float RawValue = 0.f;
};

struct FAegisViewportLabel
{
	FVector WorldPos = FVector::ZeroVector;
	FString Text;
};

class FAegisProceduralActionComponentVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual TSharedPtr<SWidget> GenerateContextMenu() const override;

private:
	mutable TWeakObjectPtr<UAegisProceduralActionComponent> LastComponent;
	mutable FName SelectedBone = NAME_None;
	mutable TArray<FCandidateCurve> SelectedCurves;
	mutable TArray<int32> SelectedChainIndices;
	mutable TMap<FName, TArray<int32>> BoneToChainIndices;
	mutable TArray<FAegisViewportLabel> PendingLabels;

private:
	USkeletalMeshComponent* FindSkeletalMeshComponent(const UAegisProceduralActionComponent* Comp) const;
	static void BuildChainBones(const USkeletalMeshComponent* SkelComp, FName StartBone, FName EndBone, TArray<int32>& OutBoneIndices);
	void BuildSelectedCurveList(const UAegisProceduralActionAsset* Asset, float Time01, FName BoneName) const;
	static void AddOrUpdateKey(UCurveFloat* Curve, float Time01, float NewValue);
	static float EvalAutomaticPhaseWeight(const FAegisActionPhaseBlendDef& Phase, float Time01);
	static void EvalGhostOffsetForBone(const FAegisChainDef_Inline& Chain, const FAegisSocketBoneDef& SocketBone, float Time01, FRotator& OutRotDeg, FVector& OutTransLS);
};
