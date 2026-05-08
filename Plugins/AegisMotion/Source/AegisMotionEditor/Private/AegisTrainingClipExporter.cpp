#include "AegisTrainingClipExporter.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimationBlueprintLibrary.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "IContentBrowserSingleton.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace AegisTrainingClipExporterInternal
{
	static const TArray<FName>& GetAegisTrainingBones()
	{
		static const TArray<FName> Bones = {
			TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("neck_01"), TEXT("head"),
			TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
			TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
			TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("ball_l"),
			TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("ball_r")
		};
		return Bones;
	}

	static FString SanitizeFileStem(const FString& In)
	{
		FString Out;
		for (TCHAR C : In)
		{
			Out.AppendChar((FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-')) ? C : TEXT('_'));
		}
		return Out.IsEmpty() ? TEXT("AegisTrainingClip") : Out;
	}

	static TSharedPtr<FJsonObject> MakeKey(const float TimeSeconds, const float Value)
	{
		TSharedPtr<FJsonObject> Key = MakeShared<FJsonObject>();
		Key->SetNumberField(TEXT("time"), TimeSeconds);
		Key->SetNumberField(TEXT("value"), Value);
		return Key;
	}

	static TSharedPtr<FJsonObject> MakeCurve(
		const FString& CurveName,
		const FName JointName,
		const FString& ChannelName,
		const TArray<float>& Values,
		const float DurationSeconds)
	{
		TSharedPtr<FJsonObject> Curve = MakeShared<FJsonObject>();
		Curve->SetStringField(TEXT("curveName"), CurveName);
		Curve->SetStringField(TEXT("jointName"), JointName.ToString());
		Curve->SetStringField(TEXT("channelName"), ChannelName);

		TArray<TSharedPtr<FJsonValue>> Keys;
		Keys.Reserve(Values.Num());
		const int32 LastIndex = FMath::Max(Values.Num() - 1, 1);
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			const float Time = (static_cast<float>(Index) / static_cast<float>(LastIndex)) * DurationSeconds;
			Keys.Add(MakeShared<FJsonValueObject>(MakeKey(Time, Values[Index])));
		}

		Curve->SetArrayField(TEXT("keys"), Keys);
		Curve->SetNumberField(TEXT("originalKeyCount"), Values.Num());
		Curve->SetNumberField(TEXT("compressedKeyCount"), Values.Num());
		Curve->SetStringField(TEXT("interpolation"), TEXT("linear"));
		Curve->SetBoolField(TEXT("preserveKeys"), true);
		return Curve;
	}

	static void AddParentChainBones(
		const FReferenceSkeleton& RefSkeleton,
		const FName BoneName,
		TArray<FName>& InOutBoneNames)
	{
		int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
		while (BoneIndex != INDEX_NONE)
		{
			const FName ParentName = RefSkeleton.GetBoneName(BoneIndex);
			InOutBoneNames.AddUnique(ParentName);
			BoneIndex = RefSkeleton.GetParentIndex(BoneIndex);
		}
	}

	static void BuildSampleBoneList(
		const FReferenceSkeleton& RefSkeleton,
		TArray<FName>& OutSampleBones,
		TArray<FName>& OutExportBones)
	{
		OutSampleBones.Reset();
		OutExportBones.Reset();

		for (const FName BoneName : GetAegisTrainingBones())
		{
			if (RefSkeleton.FindBoneIndex(BoneName) != INDEX_NONE)
			{
				OutExportBones.Add(BoneName);
				AddParentChainBones(RefSkeleton, BoneName, OutSampleBones);
			}
		}

		// Keep sample order stable and parent-before-child according to the ref skeleton.
		OutSampleBones.Sort([&RefSkeleton](const FName& A, const FName& B)
		{
			return RefSkeleton.FindBoneIndex(A) < RefSkeleton.FindBoneIndex(B);
		});
	}

	static bool GetPosesForFrame(
		const UAnimSequence* AnimSequence,
		const TArray<FName>& BoneNames,
		const int32 FrameIndex,
		const bool bExtractRootMotion,
		TArray<FTransform>& OutLocalPoses)
	{
		OutLocalPoses.Reset();
		if (!AnimSequence || BoneNames.Num() == 0)
		{
			return false;
		}

		// UE 5.x editor-safe sampling API. The returned transforms are suitable for exporting
		// local animation tracks for training. If this API signature changes, this function is
		// the single compatibility point to update.
		UAnimationBlueprintLibrary::GetBonePosesForFrame(
			AnimSequence,
			BoneNames,
			FrameIndex,
			bExtractRootMotion,
			OutLocalPoses
		);

		return OutLocalPoses.Num() == BoneNames.Num();
	}

	static FTransform GetLocalPose(
		const FName BoneName,
		const TMap<FName, FTransform>& LocalByBone,
		const FReferenceSkeleton& RefSkeleton)
	{
		if (const FTransform* Existing = LocalByBone.Find(BoneName))
		{
			return *Existing;
		}

		const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
		return BoneIndex != INDEX_NONE ? RefSkeleton.GetRefBonePose()[BoneIndex] : FTransform::Identity;
	}

	static FTransform ComputeComponentTransform(
		const FName BoneName,
		const TMap<FName, FTransform>& LocalByBone,
		const FReferenceSkeleton& RefSkeleton,
		TMap<FName, FTransform>& InOutComponentCache)
	{
		if (const FTransform* Cached = InOutComponentCache.Find(BoneName))
		{
			return *Cached;
		}

		const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			return FTransform::Identity;
		}

		const FTransform Local = GetLocalPose(BoneName, LocalByBone, RefSkeleton);
		const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		FTransform Component = Local;

		if (ParentIndex != INDEX_NONE)
		{
			const FName ParentName = RefSkeleton.GetBoneName(ParentIndex);
			Component = Local * ComputeComponentTransform(ParentName, LocalByBone, RefSkeleton, InOutComponentCache);
		}

		InOutComponentCache.Add(BoneName, Component);
		return Component;
	}

	static void NormalizeQuaternionContinuity(TArray<FQuat>& Quats)
	{
		for (int32 Index = 0; Index < Quats.Num(); ++Index)
		{
			Quats[Index].Normalize();
			if (Index > 0 && (Quats[Index - 1] | Quats[Index]) < 0.0f)
			{
				Quats[Index].X *= -1.0;
				Quats[Index].Y *= -1.0;
				Quats[Index].Z *= -1.0;
				Quats[Index].W *= -1.0;
			}
		}
	}

	static TArray<float> ToFloatArray(const TArray<FVector>& Vectors, const int32 ComponentIndex)
	{
		TArray<float> Out;
		Out.Reserve(Vectors.Num());
		for (const FVector& V : Vectors)
		{
			Out.Add(ComponentIndex == 0 ? V.X : (ComponentIndex == 1 ? V.Y : V.Z));
		}
		return Out;
	}

	static TArray<float> ToQuatFloatArray(const TArray<FQuat>& Quats, const int32 ComponentIndex)
	{
		TArray<float> Out;
		Out.Reserve(Quats.Num());
		for (const FQuat& Q : Quats)
		{
			Out.Add(ComponentIndex == 0 ? Q.X : (ComponentIndex == 1 ? Q.Y : (ComponentIndex == 2 ? Q.Z : Q.W)));
		}
		return Out;
	}

	static TArray<float> BuildContactCurve(
		const TArray<FVector>& Positions,
		const float SampleDeltaSeconds,
		const float HeightSlackCm,
		const float VelocityThresholdCmPerSec)
	{
		TArray<float> Out;
		Out.SetNumZeroed(Positions.Num());
		if (Positions.Num() == 0)
		{
			return Out;
		}

		float MinZ = Positions[0].Z;
		for (const FVector& P : Positions)
		{
			MinZ = FMath::Min(MinZ, static_cast<float>(P.Z));
		}

		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			const FVector Prev = Positions[FMath::Max(0, Index - 1)];
			const FVector Next = Positions[FMath::Min(Positions.Num() - 1, Index + 1)];
			const float Speed = static_cast<float>((Next - Prev).Size()) / FMath::Max(SampleDeltaSeconds * 2.0f, KINDA_SMALL_NUMBER);
			const float HeightAlpha = 1.0f - FMath::Clamp((static_cast<float>(Positions[Index].Z) - MinZ) / FMath::Max(HeightSlackCm, 1.0f), 0.0f, 1.0f);
			const float SpeedAlpha = 1.0f - FMath::Clamp(Speed / FMath::Max(VelocityThresholdCmPerSec, 1.0f), 0.0f, 1.0f);
			Out[Index] = FMath::Clamp(HeightAlpha * SpeedAlpha, 0.0f, 1.0f);
		}

		// Smooth enough for training/contact labels without hiding plants.
		TArray<float> Smoothed = Out;
		for (int32 Index = 1; Index < Out.Num() - 1; ++Index)
		{
			Smoothed[Index] = (Out[Index - 1] + Out[Index] * 2.0f + Out[Index + 1]) * 0.25f;
		}
		return Smoothed;
	}

	static TArray<float> BuildPlantCurve(const TArray<float>& Contact)
	{
		TArray<float> Out;
		Out.Reserve(Contact.Num());
		for (const float Value : Contact)
		{
			Out.Add(FMath::Clamp((Value - 0.25f) / 0.75f, 0.0f, 1.0f));
		}
		return Out;
	}
}

TArray<UAnimSequence*> FAegisTrainingClipExporter::GetSelectedAnimSequences()
{
	TArray<UAnimSequence*> Out;

	if (GEditor)
	{
		TArray<UObject*> SelectedObjects;
		GEditor->GetSelectedObjects()->GetSelectedObjects(SelectedObjects);
		for (UObject* Object : SelectedObjects)
		{
			if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(Object))
			{
				Out.AddUnique(AnimSequence);
			}
		}
	}

	if (FModuleManager::Get().IsModuleLoaded(TEXT("ContentBrowser")))
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		TArray<FAssetData> SelectedAssets;
		ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
		for (const FAssetData& AssetData : SelectedAssets)
		{
			if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(AssetData.GetAsset()))
			{
				Out.AddUnique(AnimSequence);
			}
		}
	}

	return Out;
}

bool FAegisTrainingClipExporter::ExportAnimSequenceToTrainingJson(
	const UAnimSequence* AnimSequence,
	const FAegisTrainingClipExportOptions& Options,
	FString& OutFilePath,
	FText& OutError)
{
	using namespace AegisTrainingClipExporterInternal;

	if (!AnimSequence)
	{
		OutError = FText::FromString(TEXT("No AnimSequence supplied."));
		return false;
	}

	const USkeleton* Skeleton = AnimSequence->GetSkeleton();
	if (!Skeleton)
	{
		OutError = FText::FromString(TEXT("AnimSequence has no Skeleton."));
		return false;
	}

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	TArray<FName> SampleBones;
	TArray<FName> ExportBones;
	BuildSampleBoneList(RefSkeleton, SampleBones, ExportBones);

	if (ExportBones.Num() == 0)
	{
		OutError = FText::FromString(TEXT("None of the expected Manny/Quinn/Aegis bones were found on this skeleton."));
		return false;
	}

	const float DurationSeconds = FMath::Max(AnimSequence->GetPlayLength(), KINDA_SMALL_NUMBER);
	const int32 SampleRate = FMath::Clamp(Options.SampleRate, 1, 240);
	const int32 FrameCount = FMath::Max(2, FMath::RoundToInt(DurationSeconds * SampleRate) + 1);
	const float SampleDeltaSeconds = DurationSeconds / static_cast<float>(FrameCount - 1);

	TMap<FName, TArray<FQuat>> BoneRotations;
	TMap<FName, TArray<FVector>> BoneLocalTranslations;
	for (const FName BoneName : ExportBones)
	{
		BoneRotations.Add(BoneName, TArray<FQuat>());
		BoneLocalTranslations.Add(BoneName, TArray<FVector>());
		BoneRotations[BoneName].Reserve(FrameCount);
		BoneLocalTranslations[BoneName].Reserve(FrameCount);
	}

	TArray<FVector> PelvisComponentLocations;
	TArray<FVector> FootLComponentLocations;
	TArray<FVector> FootRComponentLocations;
	PelvisComponentLocations.Reserve(FrameCount);
	FootLComponentLocations.Reserve(FrameCount);
	FootRComponentLocations.Reserve(FrameCount);

	for (int32 FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
	{
		const int32 AnimationFrame = FMath::Clamp(
			FMath::RoundToInt((static_cast<float>(FrameIndex) / static_cast<float>(FrameCount - 1)) * FMath::Max(AnimSequence->GetNumberOfSampledKeys() - 1, 1)),
			0,
			FMath::Max(AnimSequence->GetNumberOfSampledKeys() - 1, 0)
		);

		TArray<FTransform> SampleLocalPoses;
		if (!GetPosesForFrame(AnimSequence, SampleBones, AnimationFrame, Options.bExtractRootMotion, SampleLocalPoses))
		{
			OutError = FText::FromString(FString::Printf(TEXT("Failed to sample frame %d from %s."), AnimationFrame, *AnimSequence->GetName()));
			return false;
		}

		TMap<FName, FTransform> LocalByBone;
		for (int32 Index = 0; Index < SampleBones.Num(); ++Index)
		{
			LocalByBone.Add(SampleBones[Index], SampleLocalPoses[Index]);
		}

		for (const FName BoneName : ExportBones)
		{
			const FTransform Local = GetLocalPose(BoneName, LocalByBone, RefSkeleton);
			BoneRotations[BoneName].Add(Local.GetRotation());
			BoneLocalTranslations[BoneName].Add(Local.GetLocation());
		}

		TMap<FName, FTransform> ComponentCache;
		const FTransform PelvisComponent = ComputeComponentTransform(FName(TEXT("pelvis")), LocalByBone, RefSkeleton, ComponentCache);
		PelvisComponentLocations.Add(PelvisComponent.GetLocation());

		const FTransform FootL = ComputeComponentTransform(FName(TEXT("foot_l")), LocalByBone, RefSkeleton, ComponentCache);
		const FTransform FootR = ComputeComponentTransform(FName(TEXT("foot_r")), LocalByBone, RefSkeleton, ComponentCache);
		FootLComponentLocations.Add(FootL.GetLocation());
		FootRComponentLocations.Add(FootR.GetLocation());
	}

	for (TPair<FName, TArray<FQuat>>& Pair : BoneRotations)
	{
		NormalizeQuaternionContinuity(Pair.Value);
	}

	TArray<TSharedPtr<FJsonValue>> Curves;

	// Root/pelvis component trajectory used by the motion-prior tensorizer.
	Curves.Add(MakeShared<FJsonValueObject>(MakeCurve(TEXT("pelvis.loc_x"), FName(TEXT("pelvis")), TEXT("loc_x"), ToFloatArray(PelvisComponentLocations, 0), DurationSeconds)));
	Curves.Add(MakeShared<FJsonValueObject>(MakeCurve(TEXT("pelvis.loc_y"), FName(TEXT("pelvis")), TEXT("loc_y"), ToFloatArray(PelvisComponentLocations, 1), DurationSeconds)));
	Curves.Add(MakeShared<FJsonValueObject>(MakeCurve(TEXT("pelvis.loc_z"), FName(TEXT("pelvis")), TEXT("loc_z"), ToFloatArray(PelvisComponentLocations, 2), DurationSeconds)));

	for (const FName BoneName : ExportBones)
	{
		const TArray<FQuat>& Quats = BoneRotations[BoneName];
		Curves.Add(MakeShared<FJsonValueObject>(MakeCurve(FString::Printf(TEXT("%s.rot_qx"), *BoneName.ToString()), BoneName, TEXT("rot_qx"), ToQuatFloatArray(Quats, 0), DurationSeconds)));
		Curves.Add(MakeShared<FJsonValueObject>(MakeCurve(FString::Printf(TEXT("%s.rot_qy"), *BoneName.ToString()), BoneName, TEXT("rot_qy"), ToQuatFloatArray(Quats, 1), DurationSeconds)));
		Curves.Add(MakeShared<FJsonValueObject>(MakeCurve(FString::Printf(TEXT("%s.rot_qz"), *BoneName.ToString()), BoneName, TEXT("rot_qz"), ToQuatFloatArray(Quats, 2), DurationSeconds)));
		Curves.Add(MakeShared<FJsonValueObject>(MakeCurve(FString::Printf(TEXT("%s.rot_qw"), *BoneName.ToString()), BoneName, TEXT("rot_qw"), ToQuatFloatArray(Quats, 3), DurationSeconds)));
	}

	if (Options.bGenerateFootContacts)
	{
		const TArray<float> FootLContact = BuildContactCurve(FootLComponentLocations, SampleDeltaSeconds, 6.0f, 22.0f);
		const TArray<float> FootRContact = BuildContactCurve(FootRComponentLocations, SampleDeltaSeconds, 6.0f, 22.0f);
		Curves.Add(MakeShared<FJsonValueObject>(MakeCurve(TEXT("foot_l.ik_lock_alpha"), FName(TEXT("foot_l")), TEXT("ik_lock_alpha"), FootLContact, DurationSeconds)));
		Curves.Add(MakeShared<FJsonValueObject>(MakeCurve(TEXT("foot_r.ik_lock_alpha"), FName(TEXT("foot_r")), TEXT("ik_lock_alpha"), FootRContact, DurationSeconds)));
		Curves.Add(MakeShared<FJsonValueObject>(MakeCurve(TEXT("foot_l.plant_lock_alpha"), FName(TEXT("foot_l")), TEXT("plant_lock_alpha"), BuildPlantCurve(FootLContact), DurationSeconds)));
		Curves.Add(MakeShared<FJsonValueObject>(MakeCurve(TEXT("foot_r.plant_lock_alpha"), FName(TEXT("foot_r")), TEXT("plant_lock_alpha"), BuildPlantCurve(FootRContact), DurationSeconds)));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	const FString ClipId = SanitizeFileStem(AnimSequence->GetName());
	Root->SetStringField(TEXT("id"), ClipId);
	Root->SetStringField(TEXT("name"), AnimSequence->GetName());
	Root->SetStringField(TEXT("sourceFormat"), TEXT("AEGIS_TRAINING_EXPORT_V44_ANIM_SEQUENCE_SAMPLED"));
	Root->SetNumberField(TEXT("durationSeconds"), DurationSeconds);
	Root->SetNumberField(TEXT("frameTime"), 1.0f / static_cast<float>(SampleRate));
	Root->SetNumberField(TEXT("frameCount"), FrameCount);
	Root->SetStringField(TEXT("playbackMode"), TEXT("LiveBaseGeneratedOverlay"));
	Root->SetStringField(TEXT("basePoseMode"), TEXT("UseLiveSourcePose"));
	Root->SetStringField(TEXT("skeletonProfile"), Options.SkeletonProfile);

	TSharedPtr<FJsonObject> Generation = MakeShared<FJsonObject>();
	Generation->SetStringField(TEXT("action"), Options.Action);
	Generation->SetStringField(TEXT("style"), Options.Style);
	Generation->SetStringField(TEXT("dominantLeg"), Options.DominantLeg);
	Generation->SetStringField(TEXT("quality"), TEXT("retargeted_animsequence_sampled"));
	Generation->SetStringField(TEXT("license"), Options.License);
	Generation->SetStringField(TEXT("exporter"), TEXT("AegisMotion V44 C++ AnimSequence Training Exporter"));
	Root->SetObjectField(TEXT("generationParameters"), Generation);

	TSharedPtr<FJsonObject> CoordinateSystem = MakeShared<FJsonObject>();
	CoordinateSystem->SetStringField(TEXT("pluginForwardAxis"), TEXT("pelvis.loc_y"));
	CoordinateSystem->SetStringField(TEXT("pluginLateralAxis"), TEXT("pelvis.loc_x"));
	CoordinateSystem->SetStringField(TEXT("pluginUpAxis"), TEXT("pelvis.loc_z"));
	CoordinateSystem->SetStringField(TEXT("rotationSpace"), TEXT("local_parent_space_quaternion"));
	Root->SetObjectField(TEXT("coordinateSystem"), CoordinateSystem);

	TArray<TSharedPtr<FJsonValue>> Phases;
	const TArray<TPair<FString, float>> PhasePairs = {
		TPair<FString, float>(TEXT("start"), 0.0f),
		TPair<FString, float>(TEXT("mid"), 0.5f),
		TPair<FString, float>(TEXT("end"), 1.0f)
	};
	for (const TPair<FString, float>& PhasePair : PhasePairs)
	{
		TSharedPtr<FJsonObject> Phase = MakeShared<FJsonObject>();
		Phase->SetStringField(TEXT("name"), PhasePair.Key);
		Phase->SetNumberField(TEXT("time01"), PhasePair.Value);
		Phases.Add(MakeShared<FJsonValueObject>(Phase));
	}
	Root->SetArrayField(TEXT("phaseMarkers"), Phases);
	Root->SetArrayField(TEXT("curves"), Curves);

	FString JsonOutput;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonOutput);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		OutError = FText::FromString(TEXT("Failed to serialize training JSON."));
		return false;
	}

	const FString Directory = Options.OutputDirectory.IsEmpty()
		? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AegisTrainingExports"))
		: Options.OutputDirectory;

	IFileManager::Get().MakeDirectory(*Directory, true);
	OutFilePath = FPaths::Combine(Directory, ClipId + TEXT(".json"));

	if (!FFileHelper::SaveStringToFile(JsonOutput, *OutFilePath))
	{
		OutError = FText::FromString(FString::Printf(TEXT("Failed to write JSON file: %s"), *OutFilePath));
		return false;
	}

	return true;
}
