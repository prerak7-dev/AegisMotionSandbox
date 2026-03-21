#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "AegisProceduralActionAsset.generated.h"

class USkeletalMesh;

USTRUCT(BlueprintType)
struct FAegisSocketBoneLimit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Limits", meta = (ClampMin = "0.0"))
	FVector MaxRotationDegrees = FVector(25.f, 25.f, 25.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Limits", meta = (ClampMin = "0.0"))
	FVector MaxTranslationCm = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FAegisPerBoneMotionProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Motion", meta = (ClampMin = "0.0"))
	float DampingHalfLife = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Motion", meta = (ClampMin = "0.0"))
	float SpringStrength = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Inertia = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Motion", meta = (ClampMin = "0.0"))
	float MaxRotationSpeedDegPerSec = 1080.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Motion", meta = (ClampMin = "0.0"))
	float MaxTranslationSpeedCmPerSec = 200.0f;
};

USTRUCT(BlueprintType)
struct FAegisSocketBoneDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket")
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket", meta = (ClampMin = "0.0"))
	float BoneWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket")
	FAegisSocketBoneLimit Limits;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket")
	FAegisPerBoneMotionProfile MotionProfile;
};

USTRUCT(BlueprintType)
struct FAegisSocketBonePhaseCurves
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis Socket")
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Alpha")
	TObjectPtr<UCurveFloat> Alpha01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Rotation")
	TObjectPtr<UCurveFloat> RotX01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Rotation")
	TObjectPtr<UCurveFloat> RotY01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Rotation")
	TObjectPtr<UCurveFloat> RotZ01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Translation")
	TObjectPtr<UCurveFloat> PosX01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Translation")
	TObjectPtr<UCurveFloat> PosY01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Translation")
	TObjectPtr<UCurveFloat> PosZ01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Multipliers")
	float RotationMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Socket|Multipliers")
	float TranslationMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct FAegisActionPhaseBlendDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase")
	FName PhaseName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StartTime01 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PeakTime01 = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EndTime01 = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase", meta = (ClampMin = "0.1"))
	float EaseInExponent = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase", meta = (ClampMin = "0.1"))
	float EaseOutExponent = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase|Alpha")
	TObjectPtr<UCurveFloat> PhaseAlpha01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase")
	TArray<FAegisSocketBonePhaseCurves> BoneCurves;
};

USTRUCT(BlueprintType)
struct FAegisChainDef_Inline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain")
	FName ChainName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain")
	bool bApplyToChain = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain")
	FName StartBone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain")
	FName EndBone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain")
	bool bAutoPopulateSocketBonesFromChain = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain")
	TArray<FAegisSocketBoneDef> SocketBones;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain|Alpha")
	TObjectPtr<UCurveFloat> ChainAlpha01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain|Alpha")
	float ChainAlphaMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain|Smoothing", meta = (ClampMin = "0.0"))
	float SmoothingHalfLife = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain|Debug")
	bool bDrawDebugCones = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain|Debug")
	bool bDrawGhostPose = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain")
	TArray<FAegisActionPhaseBlendDef> Phases;
};

UCLASS(BlueprintType)
class AEGISMOTION_API UAegisProceduralActionAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Action")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Action", meta = (ClampMin = "0.01"))
	float DurationSeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Action")
	TArray<FAegisChainDef_Inline> Chains;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
#endif

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Aegis Action", meta = (DisplayName = "Auto Fixup: Phase Names + Defaults"))
	void AutoFixup_PhaseNamesAndDefaults();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Aegis Action", meta = (DisplayName = "Auto Fixup: Populate Socket Bones"))
	void AutoFixup_PopulateSocketBones();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Aegis Action", meta = (DisplayName = "Auto Fixup: Phase Bone Slots"))
	void AutoFixup_PhaseBoneSlots();

private:
	void AutoFixupPhaseNamesAndDefaults_Internal(bool bLog);
	void AutoPopulateSocketBones_Internal(bool bLog);
	void AutoFixupPhaseBoneSlots_Internal(bool bLog);

	static bool BuildRefSkeletonChainInclusive(
		const FReferenceSkeleton& RefSkel,
		FName StartBone,
		FName EndBone,
		TArray<int32>& OutSkelPath);

	static void EnsurePhaseSlotsMatchSocketBones(
		FAegisActionPhaseBlendDef& Phase,
		const TArray<FAegisSocketBoneDef>& SocketBonesOrdered);

	static void EnsurePhaseNames(FAegisChainDef_Inline& Chain, bool bLog);
	static void NormalizePhaseTimings(FAegisActionPhaseBlendDef& Phase);
};
