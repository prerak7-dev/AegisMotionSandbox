#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "AegisProceduralActionAsset.generated.h"

class USkeletalMesh;

UENUM(BlueprintType)
enum class EAegisMocapCurveTarget : uint8
{
	None,
	Rotation,
	Translation,
	FootLock UMETA(DisplayName = "Foot IK / Plant Lock")
};

UENUM(BlueprintType)
enum class EAegisMocapCurveAxis : uint8
{
	X UMETA(DisplayName = "X"),
	Y UMETA(DisplayName = "Y"),
	Z UMETA(DisplayName = "Z"),
	W UMETA(DisplayName = "W")
};

UENUM(BlueprintType)
enum class EAegisCurveValueSpace : uint8
{
	NormalizedProcedural UMETA(DisplayName = "Normalized Procedural (0..1 * Limits)"),
	RawBvhLocal UMETA(DisplayName = "Raw Local Degrees / Centimeters"),
	RawMocapQuaternionLocal UMETA(DisplayName = "Raw Local Quaternion Delta"),
	GeneratedNativeQuaternionLocal UMETA(DisplayName = "Generated UE-Native Quaternion Delta")
};

UENUM(BlueprintType)
enum class EAegisActionPlaybackMode : uint8
{
	ProceduralAdditive UMETA(DisplayName = "Procedural Additive"),
	MocapExactQuaternion UMETA(DisplayName = "Imported Mocap Exact Quaternion"),
	GeneratedNativeQuaternion UMETA(DisplayName = "Generated Native Quaternion"),
	LiveBaseGeneratedOverlay UMETA(DisplayName = "Live Base Generated Overlay")
};

UENUM(BlueprintType)
enum class EAegisBvhRotationOrder : uint8
{
	XYZ UMETA(DisplayName = "XYZ"),
	XZY UMETA(DisplayName = "XZY"),
	YXZ UMETA(DisplayName = "YXZ"),
	YZX UMETA(DisplayName = "YZX"),
	ZXY UMETA(DisplayName = "ZXY"),
	ZYX UMETA(DisplayName = "ZYX")
};


USTRUCT(BlueprintType)
struct FAegisImportedMocapCurveBinding
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Import Debug", meta = (AdvancedDisplay))
	FName SourceCurveName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Import Debug", meta = (AdvancedDisplay))
	FName SourceJointName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Import Debug", meta = (AdvancedDisplay))
	FName SourceChannelName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Import Debug", meta = (AdvancedDisplay))
	FName MatchedChainName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Import Debug", meta = (AdvancedDisplay))
	FName MatchedBoneName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Import Debug", meta = (AdvancedDisplay))
	EAegisMocapCurveTarget Target = EAegisMocapCurveTarget::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Import Debug", meta = (AdvancedDisplay))
	EAegisMocapCurveAxis Axis = EAegisMocapCurveAxis::X;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Import Debug", meta = (AdvancedDisplay))
	bool bMatchedToPhaseBoneSlot = false;
};


USTRUCT(BlueprintType)
struct FAegisMocapJointRemap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Import Debug", meta = (AdvancedDisplay))
	FName SourceJointName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Import Debug", meta = (AdvancedDisplay))
	FName TargetBoneName = NAME_None;

	FAegisMocapJointRemap()
	{
	}

	FAegisMocapJointRemap(const FName InSourceJointName, const FName InTargetBoneName)
		: SourceJointName(InSourceJointName)
		, TargetBoneName(InTargetBoneName)
	{
	}
};


USTRUCT(BlueprintType)
struct FAegisSocketBoneLimit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Procedural Limits", meta = (AdvancedDisplay, ClampMin = "0.0"))
	FVector MaxRotationDegrees = FVector(25.f, 25.f, 25.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Procedural Limits", meta = (AdvancedDisplay, ClampMin = "0.0"))
	FVector MaxTranslationCm = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FAegisPerBoneMotionProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Procedural Motion", meta = (AdvancedDisplay, ClampMin = "0.0"))
	float DampingHalfLife = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Procedural Motion", meta = (AdvancedDisplay, ClampMin = "0.0"))
	float SpringStrength = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Procedural Motion", meta = (AdvancedDisplay, ClampMin = "0.0", ClampMax = "1.0"))
	float Inertia = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Procedural Motion", meta = (AdvancedDisplay, ClampMin = "0.0"))
	float MaxRotationSpeedDegPerSec = 1080.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Procedural Motion", meta = (AdvancedDisplay, ClampMin = "0.0"))
	float MaxTranslationSpeedCmPerSec = 200.0f;
};

USTRUCT(BlueprintType)
struct FAegisSocketBoneDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks")
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks", meta = (ClampMin = "0.0"))
	float BoneWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks")
	FAegisSocketBoneLimit Limits;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks")
	FAegisPerBoneMotionProfile MotionProfile;
};

USTRUCT(BlueprintType)
struct FAegisSocketBonePhaseCurves
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks")
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks|Alpha")
	TObjectPtr<UCurveFloat> Alpha01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks|Rotation")
	TObjectPtr<UCurveFloat> RotX01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks|Rotation")
	TObjectPtr<UCurveFloat> RotY01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks|Rotation")
	TObjectPtr<UCurveFloat> RotZ01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks|Rotation")
	TObjectPtr<UCurveFloat> RotW01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks|Translation")
	TObjectPtr<UCurveFloat> PosX01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks|Translation")
	TObjectPtr<UCurveFloat> PosY01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks|Translation")
	TObjectPtr<UCurveFloat> PosZ01 = nullptr;

	// Optional generated-native contact curves consumed by the runtime plant-lock pass.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK")
	TObjectPtr<UCurveFloat> IkLockAlpha01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK")
	TObjectPtr<UCurveFloat> PlantLockAlpha01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks|Multipliers", meta = (AdvancedDisplay))
	float RotationMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Bone Tracks|Multipliers", meta = (AdvancedDisplay))
	float TranslationMultiplier = 1.0f;

	// Controls how rotation curves are interpreted.
	// NormalizedProcedural preserves the original Aegis behavior: curve value -> signed normalized control -> MaxRotationDegrees.
	// RawBvhLocal treats imported BVH values as local-space degrees before optional multipliers/alpha.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Advanced Import", meta = (AdvancedDisplay))
	EAegisCurveValueSpace RotationValueSpace = EAegisCurveValueSpace::NormalizedProcedural;

	// Controls how translation curves are interpreted.
	// NormalizedProcedural preserves the original Aegis behavior: curve value -> signed normalized control -> MaxTranslationCm.
	// RawBvhLocal treats imported BVH values as raw centimeters before optional multipliers/alpha.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Advanced Import", meta = (AdvancedDisplay))
	EAegisCurveValueSpace TranslationValueSpace = EAegisCurveValueSpace::NormalizedProcedural;

	// BVH rotations are ordered Euler channels. The Java exporter sample uses Zrotation, Xrotation, Yrotation, so ZXY is the default import order.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Advanced Import", meta = (AdvancedDisplay))
	EAegisBvhRotationOrder RotationOrder = EAegisBvhRotationOrder::XYZ;

	// Imported BVH playback should be exact by default. Disable this later when intentionally creating procedural lag/secondary motion.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Advanced Import", meta = (AdvancedDisplay))
	bool bBypassSmoothingForRawBvh = false;
};

USTRUCT(BlueprintType)
struct FAegisActionPhaseBlendDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Phase")
	FName PhaseName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StartTime01 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PeakTime01 = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EndTime01 = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Phase", meta = (ClampMin = "0.1"))
	float EaseInExponent = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Phase", meta = (ClampMin = "0.1"))
	float EaseOutExponent = 1.5f;

	// True preserves the original Aegis phase envelope behavior.
	// False is used for imported BVH playback so the mocap curves are not faded in/out by the phase envelope.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Phase")
	bool bUseAutomaticPhaseWeight = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Phase|Alpha")
	TObjectPtr<UCurveFloat> PhaseAlpha01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Phase")
	TArray<FAegisSocketBonePhaseCurves> BoneCurves;
};

USTRUCT(BlueprintType)
struct FAegisChainDef_Inline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Chain")
	FName ChainName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Chain")
	bool bApplyToChain = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Chain")
	FName StartBone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Chain")
	FName EndBone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Chain")
	bool bAutoPopulateSocketBonesFromChain = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Chain")
	TArray<FAegisSocketBoneDef> SocketBones;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Chain|Alpha")
	TObjectPtr<UCurveFloat> ChainAlpha01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Chain|Alpha")
	float ChainAlphaMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Chain|Smoothing", meta = (AdvancedDisplay, ClampMin = "0.0"))
	float SmoothingHalfLife = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Debug", meta = (AdvancedDisplay))
	bool bDrawDebugCones = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Debug", meta = (AdvancedDisplay))
	bool bDrawGhostPose = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Chain")
	TArray<FAegisActionPhaseBlendDef> Phases;
};

USTRUCT(BlueprintType)
struct FAegisGeneratedFootLockSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK")
	bool bEnableGeneratedFootLock = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LockActivationThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LockReleaseThreshold = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK", meta = (ClampMin = "0.0"))
	float MaxRootCorrectionCm = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK", meta = (ClampMin = "0.1", ClampMax = "4.0"))
	float LockBlendPower = 1.5f;

	// V31: smooth contact curves before lock/IK decisions. Generated contact curves can be dense and
	// may cross thresholds on consecutive frames, which creates visible jitter.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK", meta = (ClampMin = "0.0"))
	float ContactAlphaInterpSpeed = 18.0f;

	// V31: the old root/full-body correction could move spine/head/chest while trying to lock a foot.
	// Keep it off by default and let the leg IK solve the contact. Enable only for special stylized clips.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK", meta = (AdvancedDisplay))
	bool bAllowGeneratedRootCorrection = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK")
	bool bDrawDebugFootLock = false;

	// V28: solve planted generated feet with a true upper/lower leg two-bone IK pass after generated pose evaluation.
	// The previous V27 pass only translated the full generated pose around the planted foot, which reduced slide
	// but did not correct knee/ankle placement.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK|Two Bone IK")
	bool bEnableGeneratedTwoBoneIK = true;

	// When two-bone IK is active, keep only part of the old full-body correction and let IK solve the remaining foot error.
	// 0 = solve entirely with leg IK, 1 = V27 style full-body correction before IK.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK|Two Bone IK", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TwoBoneIKRootCorrectionWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK|Two Bone IK", meta = (ClampMin = "0.1", ClampMax = "4.0"))
	float TwoBoneIKAlphaPower = 1.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK|Two Bone IK", meta = (ClampMin = "0.0"))
	float MaxTwoBoneIKCorrectionCm = 35.0f;

	// V31: do not manually translate foot descendants after solving the ankle. Unreal will propagate the
	// solved foot transform through the hierarchy. Manual descendant offsets caused toe/foot stretch.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK|Two Bone IK", meta = (AdvancedDisplay))
	bool bTranslateFootDescendantsWithIK = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK|Two Bone IK")
	bool bDrawDebugTwoBoneIK = false;

};

USTRUCT(BlueprintType)
struct FAegisGeneratedHeadLookAtSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Head LookAt")
	bool bEnableGeneratedHeadLookAt = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Head LookAt", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NeckWeight = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Head LookAt", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeadWeight = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Head LookAt", meta = (ClampMin = "0.0"))
	float SmoothingHalfLife = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Head LookAt", meta = (ClampMin = "0.0"))
	float MaxYawDegrees = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Head LookAt", meta = (ClampMin = "0.0"))
	float MaxPitchDegrees = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Head LookAt")
	FName TargetSocketOrBoneName = TEXT("KickTarget");
};

UCLASS(BlueprintType)
class AEGISMOTION_API UAegisProceduralActionAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Playback")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Playback", meta = (ClampMin = "0.01"))
	float DurationSeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Playback")
	EAegisActionPlaybackMode PlaybackMode = EAegisActionPlaybackMode::ProceduralAdditive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Foot IK")
	FAegisGeneratedFootLockSettings FootLockSettings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Generated Motion")
	FString SourceFormat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Generated Motion")
	FString SkeletonProfile;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Generated Motion")
	FString GenerationSummary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Advanced Import", meta = (AdvancedDisplay))
	bool bShowLegacyMocapImportSettings = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Chain")
	TArray<FAegisChainDef_Inline> Chains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Legacy Import", meta = (AdvancedDisplay))
	TArray<FAegisMocapJointRemap> MocapJointRemapTable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Legacy Import", meta = (AdvancedDisplay))
	TArray<FAegisImportedMocapCurveBinding> ImportedMocapBindings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aegis|Legacy Import", meta = (AdvancedDisplay))
	FString LastImportedMocapJsonPath;


#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
#endif

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Aegis|Utilities", meta = (DisplayName = "Auto Fixup: Phase Names + Defaults"))
	void AutoFixup_PhaseNamesAndDefaults();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Aegis|Utilities", meta = (DisplayName = "Auto Fixup: Populate Socket Bones"))
	void AutoFixup_PopulateSocketBones();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Aegis|Utilities", meta = (DisplayName = "Auto Fixup: Phase Bone Slots"))
	void AutoFixup_PhaseBoneSlots();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Aegis|Advanced Import", meta = (DisplayName = "Reset Legacy Mocap Joint Remap Table To Defaults", AdvancedDisplay))
	void ResetMocapJointRemapTableToDefaults();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Aegis|Generated Motion", meta = (DisplayName = "Clean Generated Native Action"))
	void CleanGeneratedNativeAction();

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
