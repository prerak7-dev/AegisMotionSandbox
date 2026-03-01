// AegisProceduralActionAsset.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "AegisProceduralActionAsset.generated.h"

UENUM(BlueprintType)
enum class EAegisChainSolverType : uint8
{
	PivotChain UMETA(DisplayName = "Pivot Chain"),
	HingeChainAuto UMETA(DisplayName = "Hinge Chain (Auto)")
};

UENUM(BlueprintType)
enum class EAegisChainDistributionMode : uint8
{
	Uniform UMETA(DisplayName = "Uniform"),
	RampToEnd UMETA(DisplayName = "Ramp To End"),
	PivotToEnd UMETA(DisplayName = "Pivot To End")
};

UENUM(BlueprintType)
enum class EAegisRotChannels : uint8
{
	None = 0 UMETA(Hidden),
	Pitch = 1 << 0,
	Yaw = 1 << 1,
	Roll = 1 << 2,
	All = Pitch | Yaw | Roll
};
ENUM_CLASS_FLAGS(EAegisRotChannels)

USTRUCT(BlueprintType)
struct FAegisMaxDegreesPRY
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Pivot", meta = (ClampMin = "0.0"))
	float Pitch = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Pivot", meta = (ClampMin = "0.0"))
	float Yaw = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Pivot", meta = (ClampMin = "0.0"))
	float Roll = 0.f;
};

USTRUCT(BlueprintType)
struct FAegisPhaseCurvesPRY
{
	GENERATED_BODY()

	// NEW: name for cleaner authoring
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase")
	FName PhaseName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase")
	TObjectPtr<UCurveFloat> Window01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ActiveThreshold = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase")
	TObjectPtr<UCurveFloat> Pitch01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase")
	TObjectPtr<UCurveFloat> Yaw01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase")
	TObjectPtr<UCurveFloat> Roll01 = nullptr;
};

USTRUCT(BlueprintType)
struct FAegisHingeCurveSlot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis Hinge")
	FName HingeBone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Hinge")
	TObjectPtr<UCurveFloat> Angle01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Hinge")
	bool bSignedAngle = false;
};

USTRUCT(BlueprintType)
struct FAegisAutoHingePhase
{
	GENERATED_BODY()

	// NEW: name for cleaner authoring
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase")
	FName PhaseName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase")
	TObjectPtr<UCurveFloat> Window01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ActiveThreshold = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Phase")
	TArray<FAegisHingeCurveSlot> Hinges;
};

USTRUCT(BlueprintType)
struct FAegisChainDef_Inline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain")
	FName ChainName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain")
	EAegisChainSolverType SolverType = EAegisChainSolverType::PivotChain;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain")
	FName StartBone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain")
	FName EndBone;

	// Pivot
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Pivot")
	EAegisChainDistributionMode DistributionMode = EAegisChainDistributionMode::RampToEnd;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Pivot")
	FName PivotBone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Pivot")
	FAegisMaxDegreesPRY MaxDegreesPRY;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Pivot", meta = (Bitmask, BitmaskEnum = "/Script/AegisMotion.EAegisRotChannels"))
	int32 DrivenChannels = int32(EAegisRotChannels::All);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Pivot")
	TArray<FAegisPhaseCurvesPRY> PivotPhases;

	// Hinge Auto
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Hinge Auto", meta = (EditCondition = "SolverType==EAegisChainSolverType::HingeChainAuto"))
	FVector ParentSpaceAxis = FVector(1, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Hinge Auto", meta = (EditCondition = "SolverType==EAegisChainSolverType::HingeChainAuto", ClampMin = "0.0"))
	float MaxDegreesScale = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Hinge Auto", meta = (EditCondition = "SolverType==EAegisChainSolverType::HingeChainAuto"))
	bool bClampDegrees = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Hinge Auto", meta = (EditCondition = "bClampDegrees && SolverType==EAegisChainSolverType::HingeChainAuto"))
	float MinDegrees = -60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Hinge Auto", meta = (EditCondition = "bClampDegrees && SolverType==EAegisChainSolverType::HingeChainAuto"))
	float MaxDegreesClamp = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Hinge Auto", meta = (EditCondition = "SolverType==EAegisChainSolverType::HingeChainAuto"))
	TArray<FAegisAutoHingePhase> HingePhases;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis Chain", meta = (ClampMin = "0.0"))
	float SmoothingHalfLife = 0.12f;
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

	// Buttons
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Aegis Action", meta = (DisplayName = "Auto Fixup: Phase Names + Defaults"))
	void AutoFixup_PhaseNamesAndDefaults();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Aegis Action", meta = (DisplayName = "Auto Fixup: Hinge Phase Slots"))
	void AutoFixup_HingePhaseSlots();

private:
	void AutoFixupPhaseNamesAndDefaults_Internal(bool bLog);
	void AutoFixupHingePhaseSlots_Internal(bool bLog);

	static bool BuildRefSkeletonChainInclusive(
		const FReferenceSkeleton& RefSkel,
		FName StartBone,
		FName EndBone,
		TArray<int32>& OutSkelPath);

	static void EnsurePhaseSlotsMatchHinges(
		FAegisAutoHingePhase& Phase,
		const TArray<FName>& HingeBonesOrdered);

	static void EnsurePhaseNamesPivot(FAegisChainDef_Inline& Chain, bool bLog);
	static void EnsurePhaseNamesHinge(FAegisChainDef_Inline& Chain, bool bLog);
};
