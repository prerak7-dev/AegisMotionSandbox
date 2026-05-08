#include "AegisMotionEditorModule.h"

#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "PropertyEditorModule.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEd.h"
#include "ComponentVisualizer.h"
#endif

#include "AegisProceduralActionComponentDetails.h"
#include "AegisProceduralActionComponentVisualizer.h"
#include "AegisTrainingClipExporter.h"
#include "AegisAction/AegisProceduralActionComponent.h"

#include "ToolMenus.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Editor.h"
#include "ContentBrowserModule.h"
#include "AssetRegistry/AssetData.h"
#include "IContentBrowserSingleton.h"
#include "Curves/CurveFloat.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UObject/UObjectGlobals.h"

#include "AegisAction/AegisProceduralActionAsset.h"

IMPLEMENT_MODULE(FAegisMotionEditorModule, AegisMotionEditor)

void FAegisMotionEditorModule::StartupModule()
{
#if WITH_EDITOR
	RegisterCustomizations();
	RegisterVisualizers();

	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAegisMotionEditorModule::RegisterMenus)
		);
	}
#endif
}

void FAegisMotionEditorModule::ShutdownModule()
{
#if WITH_EDITOR
	UnregisterMenus();
	UnregisterVisualizers();
	UnregisterCustomizations();
#endif
}

void FAegisMotionEditorModule::RegisterCustomizations()
{
#if WITH_EDITOR
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FModuleManager::Get().LoadModule(TEXT("PropertyEditor"));
	}

	FPropertyEditorModule& PropertyModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	PropertyModule.RegisterCustomClassLayout(
		UAegisProceduralActionComponent::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(
			&FAegisProceduralActionComponentDetails::MakeInstance
		)
	);

	PropertyModule.NotifyCustomizationModuleChanged();
#endif
}

void FAegisMotionEditorModule::UnregisterCustomizations()
{
#if WITH_EDITOR
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyModule =
			FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

		PropertyModule.UnregisterCustomClassLayout(
			UAegisProceduralActionComponent::StaticClass()->GetFName()
		);

		PropertyModule.NotifyCustomizationModuleChanged();
	}
#endif
}

void FAegisMotionEditorModule::RegisterVisualizers()
{
#if WITH_EDITOR
	if (GUnrealEd == nullptr)
	{
		return;
	}

	ActionComponentVisualizer = MakeShared<FAegisProceduralActionComponentVisualizer>();

	GUnrealEd->RegisterComponentVisualizer(
		UAegisProceduralActionComponent::StaticClass()->GetFName(),
		ActionComponentVisualizer
	);

	if (ActionComponentVisualizer.IsValid())
	{
		ActionComponentVisualizer->OnRegister();
	}
#endif
}

void FAegisMotionEditorModule::UnregisterVisualizers()
{
#if WITH_EDITOR
	if (GUnrealEd && ActionComponentVisualizer.IsValid())
	{
		GUnrealEd->UnregisterComponentVisualizer(
			UAegisProceduralActionComponent::StaticClass()->GetFName()
		);
	}

	ActionComponentVisualizer.Reset();
#endif
}

namespace AegisMocapImportEditor
{
	static FString SanitizeObjectName(const FString& In)
	{
		FString Out;

		for (TCHAR C : In)
		{
			if (FChar::IsAlnum(C) || C == TEXT('_'))
			{
				Out.AppendChar(C);
			}
			else
			{
				Out.AppendChar(TEXT('_'));
			}
		}

		return Out.IsEmpty() ? TEXT("ImportedMocapCurve") : Out;
	}

	static FString NormalizeForMatch(const FString& In)
	{
		FString Out = In.ToLower();

		Out.ReplaceInline(TEXT("mixamorig:"), TEXT(""));
		Out.ReplaceInline(TEXT("mixamorig"), TEXT(""));
		Out.ReplaceInline(TEXT("_"), TEXT(""));
		Out.ReplaceInline(TEXT("-"), TEXT(""));
		Out.ReplaceInline(TEXT(" "), TEXT(""));
		Out.ReplaceInline(TEXT("."), TEXT(""));
		Out.ReplaceInline(TEXT("left"), TEXT("l"));
		Out.ReplaceInline(TEXT("right"), TEXT("r"));

		return Out;
	}


	static FName ResolveMocapJointAliasToAegisBone(
		const UAegisProceduralActionAsset* TargetAsset,
		const FName SourceJointName)
	{
		if (!TargetAsset || SourceJointName.IsNone())
		{
			return SourceJointName;
		}

		for (const FAegisMocapJointRemap& Remap : TargetAsset->MocapJointRemapTable)
		{
			if (Remap.SourceJointName == SourceJointName && !Remap.TargetBoneName.IsNone())
			{
				return Remap.TargetBoneName;
			}

			const FString SourceNormalized = NormalizeForMatch(SourceJointName.ToString());
			const FString RemapSourceNormalized = NormalizeForMatch(Remap.SourceJointName.ToString());

			if (!SourceNormalized.IsEmpty()
				&& !RemapSourceNormalized.IsEmpty()
				&& SourceNormalized == RemapSourceNormalized
				&& !Remap.TargetBoneName.IsNone())
			{
				return Remap.TargetBoneName;
			}
		}

		return SourceJointName;
	}

	static bool NamesLikelyMatch(const FName A, const FName B)
	{
		if (A == B)
		{
			return true;
		}

		const FString NA = NormalizeForMatch(A.ToString());
		const FString NB = NormalizeForMatch(B.ToString());

		return !NA.IsEmpty()
			&& !NB.IsEmpty()
			&& (NA == NB || NA.Contains(NB) || NB.Contains(NA));
	}

	static bool IsGeneratedNativeJson(const FString& SourceFormat, const FString& PlaybackMode)
	{
		const FString Combined = (SourceFormat + TEXT(" ") + PlaybackMode).ToLower();
		return Combined.Contains(TEXT("ai_native"))
			|| Combined.Contains(TEXT("generatednative"))
			|| Combined.Contains(TEXT("generated native"))
			|| Combined.Contains(TEXT("livebase"))
			|| Combined.Contains(TEXT("live base"));
	}

	static bool IsLiveBaseGeneratedOverlayJson(const FString& SourceFormat, const FString& PlaybackMode)
	{
		const FString Combined = (SourceFormat + TEXT(" ") + PlaybackMode).ToLower();
		return Combined.Contains(TEXT("livebase"))
			|| Combined.Contains(TEXT("live base"))
			|| Combined.Contains(TEXT("generated overlay"))
			|| Combined.Contains(TEXT("overlay"));
	}

	static bool IsAuthoringDebugCurve(const FString& CurveName, const FString& ChannelName)
	{
		const FString Combined = (CurveName + TEXT(" ") + ChannelName).ToLower();
		return Combined.Contains(TEXT("debug_"))
			|| Combined.Contains(TEXT("_debug"))
			|| Combined.Contains(TEXT("debugpitch"))
			|| Combined.Contains(TEXT("debugyaw"))
			|| Combined.Contains(TEXT("debugroll"));
	}

	static EAegisMocapCurveAxis DetectAxis(const FString& CurveName, const FString& ChannelName)
	{
		const FString Combined = (CurveName + TEXT(" ") + ChannelName).ToLower();

		if (Combined.Contains(TEXT("rot_x"))
			|| Combined.Contains(TEXT("loc_x"))
			|| Combined.Contains(TEXT("translation_x"))
			|| Combined.Contains(TEXT("rot_qx"))
			|| Combined.Contains(TEXT("quat_x"))
			|| Combined.Contains(TEXT("qx"))
			|| Combined.Contains(TEXT("pos_x"))
			|| Combined.Contains(TEXT("xrotation"))
			|| Combined.Contains(TEXT("xposition")))
		{
			return EAegisMocapCurveAxis::X;
		}

		if (Combined.Contains(TEXT("rot_y"))
			|| Combined.Contains(TEXT("loc_y"))
			|| Combined.Contains(TEXT("translation_y"))
			|| Combined.Contains(TEXT("rot_qy"))
			|| Combined.Contains(TEXT("quat_y"))
			|| Combined.Contains(TEXT("qy"))
			|| Combined.Contains(TEXT("pos_y"))
			|| Combined.Contains(TEXT("yrotation"))
			|| Combined.Contains(TEXT("yposition")))
		{
			return EAegisMocapCurveAxis::Y;
		}

		if (Combined.Contains(TEXT("rot_qw")) || Combined.Contains(TEXT("quat_w")) || Combined.Contains(TEXT("qw")))
		{
			return EAegisMocapCurveAxis::W;
		}

		return EAegisMocapCurveAxis::Z;
	}

	static EAegisMocapCurveTarget DetectTarget(const FString& CurveName, const FString& ChannelName)
	{
		const FString Combined = (CurveName + TEXT(" ") + ChannelName).ToLower();

		if (Combined.Contains(TEXT("ik_lock"))
			|| Combined.Contains(TEXT("plant_lock"))
			|| Combined.Contains(TEXT("foot_lock"))
			|| Combined.Contains(TEXT("contact_alpha")))
		{
			return EAegisMocapCurveTarget::FootLock;
		}

		if (Combined.Contains(TEXT("pos"))
			|| Combined.Contains(TEXT("loc_"))
			|| Combined.Contains(TEXT("location"))
			|| Combined.Contains(TEXT("translation"))
			|| Combined.Contains(TEXT("position")))
		{
			return EAegisMocapCurveTarget::Translation;
		}

		if (Combined.Contains(TEXT("rot")) || Combined.Contains(TEXT("rotation")))
		{
			return EAegisMocapCurveTarget::Rotation;
		}

		return EAegisMocapCurveTarget::None;
	}

	static void AssignToSlot(
		FAegisSocketBonePhaseCurves& Slot,
		EAegisMocapCurveTarget Target,
		EAegisMocapCurveAxis Axis,
		UCurveFloat* Curve,
		const bool bGeneratedNative)
	{
		if (!Curve)
		{
			return;
		}

		if (Target == EAegisMocapCurveTarget::Rotation)
		{
			const bool bQuaternionCurve = (Axis == EAegisMocapCurveAxis::W) || (Curve && Curve->GetName().Contains(TEXT("rot_q")));
			Slot.RotationValueSpace = bQuaternionCurve
				? (bGeneratedNative ? EAegisCurveValueSpace::GeneratedNativeQuaternionLocal : EAegisCurveValueSpace::RawMocapQuaternionLocal)
				: EAegisCurveValueSpace::RawBvhLocal;
			Slot.RotationOrder = EAegisBvhRotationOrder::XYZ;
			Slot.bBypassSmoothingForRawBvh = true;

			if (Axis == EAegisMocapCurveAxis::X)
			{
				Slot.RotX01 = Curve;
			}
			else if (Axis == EAegisMocapCurveAxis::Y)
			{
				Slot.RotY01 = Curve;
			}
			else if (Axis == EAegisMocapCurveAxis::Z)
			{
				Slot.RotZ01 = Curve;
			}
			else
			{
				Slot.RotW01 = Curve;
			}
		}
		else if (Target == EAegisMocapCurveTarget::Translation)
		{
			Slot.TranslationValueSpace = EAegisCurveValueSpace::RawBvhLocal;
			Slot.bBypassSmoothingForRawBvh = true;

			if (Axis == EAegisMocapCurveAxis::X)
			{
				Slot.PosX01 = Curve;
			}
			else if (Axis == EAegisMocapCurveAxis::Y)
			{
				Slot.PosY01 = Curve;
			}
			else
			{
				Slot.PosZ01 = Curve;
			}
		}
		else if (Target == EAegisMocapCurveTarget::FootLock)
		{
			const FString Name = Curve->GetName().ToLower();
			if (Name.Contains(TEXT("plant_lock")))
			{
				Slot.PlantLockAlpha01 = Curve;
			}
			else
			{
				Slot.IkLockAlpha01 = Curve;
			}
		}
	}


	static FAegisChainDef_Inline& EnsureImportedMocapFullBodyChain(UAegisProceduralActionAsset* TargetAsset, const bool bGeneratedNative)
	{
		check(TargetAsset);

		const FName ChainName = bGeneratedNative ? FName(TEXT("Aegis_GeneratedNative_FullBody")) : FName(TEXT("Imported_CMU_Mixamo_FullBody"));
		for (FAegisChainDef_Inline& Chain : TargetAsset->Chains)
		{
			if (Chain.ChainName == ChainName)
			{
				return Chain;
			}
		}

		FAegisChainDef_Inline& NewChain = TargetAsset->Chains.AddDefaulted_GetRef();
		NewChain.ChainName = ChainName;
		NewChain.bApplyToChain = true;
		NewChain.StartBone = TEXT("pelvis");
		NewChain.EndBone = TEXT("head");
		NewChain.bAutoPopulateSocketBonesFromChain = false;
		NewChain.SmoothingHalfLife = 0.0f;
		NewChain.ChainAlphaMultiplier = 1.0f;

		FAegisActionPhaseBlendDef& Phase = NewChain.Phases.AddDefaulted_GetRef();
		Phase.PhaseName = bGeneratedNative ? TEXT("GeneratedNativeFullBody") : TEXT("ImportedFullBody");
		Phase.StartTime01 = 0.0f;
		Phase.PeakTime01 = 0.5f;
		Phase.EndTime01 = 1.0f;
		Phase.bUseAutomaticPhaseWeight = false;

		return NewChain;
	}

	static FAegisSocketBonePhaseCurves& EnsureImportedMocapBoneSlot(
		UAegisProceduralActionAsset* TargetAsset,
		const FName BoneName,
		const bool bGeneratedNative)
	{
		FAegisChainDef_Inline& Chain = EnsureImportedMocapFullBodyChain(TargetAsset, bGeneratedNative);

		bool bHasSocket = false;
		for (FAegisSocketBoneDef& SocketBone : Chain.SocketBones)
		{
			if (SocketBone.BoneName == BoneName)
			{
				bHasSocket = true;
				SocketBone.BoneWeight = 1.0f;
				SocketBone.Limits.MaxRotationDegrees = FVector(180.0f, 180.0f, 180.0f);
				SocketBone.Limits.MaxTranslationCm = FVector(1000.0f, 1000.0f, 1000.0f);
				SocketBone.MotionProfile.DampingHalfLife = 0.0f;
				SocketBone.MotionProfile.SpringStrength = 0.0f;
				SocketBone.MotionProfile.Inertia = 0.0f;
				SocketBone.MotionProfile.MaxRotationSpeedDegPerSec = 0.0f;
				SocketBone.MotionProfile.MaxTranslationSpeedCmPerSec = 0.0f;
				break;
			}
		}

		if (!bHasSocket)
		{
			FAegisSocketBoneDef& SocketBone = Chain.SocketBones.AddDefaulted_GetRef();
			SocketBone.BoneName = BoneName;
			SocketBone.BoneWeight = 1.0f;
			SocketBone.Limits.MaxRotationDegrees = FVector(180.0f, 180.0f, 180.0f);
			SocketBone.Limits.MaxTranslationCm = FVector(1000.0f, 1000.0f, 1000.0f);
			SocketBone.MotionProfile.DampingHalfLife = 0.0f;
			SocketBone.MotionProfile.SpringStrength = 0.0f;
			SocketBone.MotionProfile.Inertia = 0.0f;
			SocketBone.MotionProfile.MaxRotationSpeedDegPerSec = 0.0f;
			SocketBone.MotionProfile.MaxTranslationSpeedCmPerSec = 0.0f;
		}

		if (Chain.Phases.Num() == 0)
		{
			FAegisActionPhaseBlendDef& Phase = Chain.Phases.AddDefaulted_GetRef();
			Phase.PhaseName = bGeneratedNative ? TEXT("GeneratedNativeFullBody") : TEXT("ImportedFullBody");
			Phase.bUseAutomaticPhaseWeight = false;
		}

		FAegisActionPhaseBlendDef& Phase = Chain.Phases[0];
		Phase.bUseAutomaticPhaseWeight = false;
		Phase.StartTime01 = 0.0f;
		Phase.PeakTime01 = 0.5f;
		Phase.EndTime01 = 1.0f;

		for (FAegisSocketBonePhaseCurves& Slot : Phase.BoneCurves)
		{
			if (Slot.BoneName == BoneName)
			{
				Slot.RotationMultiplier = 1.0f;
				Slot.TranslationMultiplier = 1.0f;
				if (Slot.RotationValueSpace != EAegisCurveValueSpace::RawMocapQuaternionLocal
					&& Slot.RotationValueSpace != EAegisCurveValueSpace::GeneratedNativeQuaternionLocal)
				{
					Slot.RotationValueSpace = EAegisCurveValueSpace::RawBvhLocal;
				}
				Slot.TranslationValueSpace = EAegisCurveValueSpace::RawBvhLocal;
				Slot.bBypassSmoothingForRawBvh = true;
				return Slot;
			}
		}

		FAegisSocketBonePhaseCurves& Slot = Phase.BoneCurves.AddDefaulted_GetRef();
		Slot.BoneName = BoneName;
		Slot.RotationMultiplier = 1.0f;
		Slot.TranslationMultiplier = 1.0f;
		Slot.RotationValueSpace = EAegisCurveValueSpace::RawBvhLocal;
		Slot.TranslationValueSpace = EAegisCurveValueSpace::RawBvhLocal;
		Slot.RotationOrder = EAegisBvhRotationOrder::XYZ;
		Slot.bBypassSmoothingForRawBvh = true;
		return Slot;
	}

	static UCurveFloat* CreateCurveInsideActionAsset(
		UAegisProceduralActionAsset* TargetAsset,
		const FString& CurveName,
		const TArray<TSharedPtr<FJsonValue>>& KeysArray,
		const double SourceDurationSeconds)
	{
		if (!TargetAsset)
		{
			return nullptr;
		}

		const FString ObjectName =
			FString::Printf(TEXT("Mocap_%s"), *SanitizeObjectName(CurveName));

		const FName UniqueName =
			MakeUniqueObjectName(TargetAsset, UCurveFloat::StaticClass(), FName(*ObjectName));

		UCurveFloat* Curve =
			NewObject<UCurveFloat>(TargetAsset, UniqueName, RF_Public | RF_Transactional);

		if (!Curve)
		{
			return nullptr;
		}

		Curve->FloatCurve.Reset();

		double LastNormalizedTime = 0.0;
		double LastValue = 0.0;
		bool bAddedAnyKey = false;

		for (const TSharedPtr<FJsonValue>& KeyValue : KeysArray)
		{
			const TSharedPtr<FJsonObject> KeyObject =
				KeyValue.IsValid() ? KeyValue->AsObject() : nullptr;

			if (!KeyObject.IsValid())
			{
				continue;
			}

			double TimeSeconds = 0.0;
			double Value = 0.0;

			if (!KeyObject->TryGetNumberField(TEXT("time"), TimeSeconds)
				|| !KeyObject->TryGetNumberField(TEXT("value"), Value))
			{
				continue;
			}

			const double NormalizedTime =
				SourceDurationSeconds > KINDA_SMALL_NUMBER
				? FMath::Clamp(TimeSeconds / SourceDurationSeconds, 0.0, 1.0)
				: FMath::Clamp(TimeSeconds, 0.0, 1.0);

			const FKeyHandle KeyHandle = Curve->FloatCurve.AddKey(
				static_cast<float>(NormalizedTime),
				static_cast<float>(Value)
			);
			Curve->FloatCurve.SetKeyInterpMode(KeyHandle, RCIM_Linear);

			LastNormalizedTime = NormalizedTime;
			LastValue = Value;
			bAddedAnyKey = true;
		}

		// Aegis evaluates imported action curves over normalized 0..1 time.
		// BVH usually stores the last frame at (FrameCount - 1) * FrameTime, while durationSeconds is FrameCount * FrameTime.
		// Add a final hold key at 1.0 so the last imported pose is preserved at the end of the action.
		if (bAddedAnyKey && LastNormalizedTime < 1.0 - KINDA_SMALL_NUMBER)
		{
			const FKeyHandle HoldKeyHandle = Curve->FloatCurve.AddKey(1.0f, static_cast<float>(LastValue));
		Curve->FloatCurve.SetKeyInterpMode(HoldKeyHandle, RCIM_Linear);
		}

		Curve->MarkPackageDirty();

		return Curve;
	}

	static bool AssignCurveToMatchingBoneSlot(
		UAegisProceduralActionAsset* TargetAsset,
		const FString& CurveName,
		const FString& JointName,
		const FString& ChannelName,
		UCurveFloat* Curve,
		FAegisImportedMocapCurveBinding& OutBinding,
		const bool bGeneratedNative)
	{
		if (!TargetAsset || !Curve)
		{
			return false;
		}

		const FName SourceJointFName(*JointName);
		const FName ResolvedJointFName = ResolveMocapJointAliasToAegisBone(TargetAsset, SourceJointFName);
		const EAegisMocapCurveTarget Target = DetectTarget(CurveName, ChannelName);
		const EAegisMocapCurveAxis Axis = DetectAxis(CurveName, ChannelName);

		OutBinding.SourceCurveName = FName(*CurveName);
		OutBinding.SourceJointName = SourceJointFName;
		OutBinding.SourceChannelName = FName(*ChannelName);
		OutBinding.Target = Target;
		OutBinding.Axis = Axis;

		if (!ResolvedJointFName.IsNone() && Target != EAegisMocapCurveTarget::None)
		{
			FAegisSocketBonePhaseCurves& ImportedSlot = EnsureImportedMocapBoneSlot(TargetAsset, ResolvedJointFName, bGeneratedNative);
			AssignToSlot(ImportedSlot, Target, Axis, Curve, bGeneratedNative);

			OutBinding.MatchedChainName = bGeneratedNative ? TEXT("Aegis_GeneratedNative_FullBody") : TEXT("Imported_CMU_Mixamo_FullBody");
			OutBinding.MatchedBoneName = ResolvedJointFName;
			OutBinding.bMatchedToPhaseBoneSlot = true;
			return true;
		}

		for (FAegisChainDef_Inline& Chain : TargetAsset->Chains)
		{
			if (Chain.Phases.Num() == 0)
			{
				continue;
			}

			FAegisActionPhaseBlendDef& Phase = Chain.Phases[0];

			for (FAegisSocketBonePhaseCurves& Slot : Phase.BoneCurves)
			{
				if (!NamesLikelyMatch(Slot.BoneName, ResolvedJointFName))
				{
					continue;
				}

				AssignToSlot(Slot, Target, Axis, Curve, bGeneratedNative);

				OutBinding.MatchedChainName = Chain.ChainName;
				OutBinding.MatchedBoneName = Slot.BoneName;
				OutBinding.bMatchedToPhaseBoneSlot = true;

				return true;
			}
		}

		return false;
	}

	static UAegisProceduralActionAsset* GetSelectedAegisActionAsset()
	{
		if (GEditor)
		{
			TArray<UObject*> SelectedObjects;
			GEditor->GetSelectedObjects()->GetSelectedObjects(SelectedObjects);

			for (UObject* Object : SelectedObjects)
			{
				if (UAegisProceduralActionAsset* Asset =
					Cast<UAegisProceduralActionAsset>(Object))
				{
					return Asset;
				}
			}
		}

		if (FModuleManager::Get().IsModuleLoaded(TEXT("ContentBrowser")))
		{
			FContentBrowserModule& ContentBrowserModule =
				FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

			TArray<FAssetData> SelectedAssets;
			ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

			for (const FAssetData& AssetData : SelectedAssets)
			{
				if (UAegisProceduralActionAsset* Asset =
					Cast<UAegisProceduralActionAsset>(AssetData.GetAsset()))
				{
					return Asset;
				}
			}
		}

		return nullptr;
	}
}

void FAegisMotionEditorModule::RegisterMenus()
{
#if WITH_EDITOR
	if (!UToolMenus::IsToolMenuUIEnabled())
	{
		return;
	}

	FToolMenuOwnerScoped OwnerScoped(this);

	if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")))
	{
		FToolMenuSection& Section =
			ToolsMenu->FindOrAddSection(
				TEXT("AegisMotion"),
				FText::FromString(TEXT("Aegis Motion"))
			);

		Section.AddMenuEntry(
			TEXT("AegisImportMocapCurves"),
			FText::FromString(TEXT("Import Aegis Animation JSON to Action Asset")),
			FText::FromString(TEXT("Import generated UE-native or legacy mocap JSON curves and auto-fill the selected Aegis Procedural Action Asset.")),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(
					this,
					&FAegisMotionEditorModule::ImportMocapCurvesToSelectedActionAsset
				)
			)
		);

		Section.AddMenuEntry(
			TEXT("AegisExportSelectedAnimSequencesTrainingJson"),
			FText::FromString(TEXT("Export Selected AnimSequences to Training JSON")),
			FText::FromString(TEXT("Sample selected Manny/Quinn AnimSequence assets into Aegis motion-prior training JSON files.")),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(
					this,
					&FAegisMotionEditorModule::ExportSelectedAnimSequencesToTrainingJson
				)
			)
		);
	}

	if (UToolMenu* AssetContextMenu = UToolMenus::Get()->ExtendMenu(TEXT("ContentBrowser.AssetContextMenu")))
	{
		FToolMenuSection& Section =
			AssetContextMenu->FindOrAddSection(
				TEXT("AegisMotion"),
				FText::FromString(TEXT("Aegis Motion"))
			);

		Section.AddMenuEntry(
			TEXT("AegisImportMocapCurves_ContextMenu"),
			FText::FromString(TEXT("Import Aegis Animation JSON to Action Asset")),
			FText::FromString(TEXT("Import generated UE-native or legacy mocap JSON curves into the selected Aegis Procedural Action Asset.")),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(
					this,
					&FAegisMotionEditorModule::ImportMocapCurvesToSelectedActionAsset
				)
			)
		);

		Section.AddMenuEntry(
			TEXT("AegisExportSelectedAnimSequencesTrainingJson_ContextMenu"),
			FText::FromString(TEXT("Export AnimSequence(s) to Aegis Training JSON")),
			FText::FromString(TEXT("Sample selected AnimSequence assets into Aegis motion-prior training JSON files.")),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(
					this,
					&FAegisMotionEditorModule::ExportSelectedAnimSequencesToTrainingJson
				)
			)
		);
	}
#endif
}


void FAegisMotionEditorModule::UnregisterMenus()
{
#if WITH_EDITOR
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
#endif
}


void FAegisMotionEditorModule::ExportSelectedAnimSequencesToTrainingJson()
{
#if WITH_EDITOR
	TArray<UAnimSequence*> AnimSequences = FAegisTrainingClipExporter::GetSelectedAnimSequences();
	if (AnimSequences.Num() == 0)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::FromString(TEXT("Select one or more Manny/Quinn AnimSequence assets in the Content Browser first."))
		);
		return;
	}

	FAegisTrainingClipExportOptions Options;
	Options.OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AegisTrainingExports"));
	Options.Action = TEXT("soccer_kick_overlay");
	Options.Style = TEXT("retargeted");
	Options.DominantLeg = TEXT("unknown");
	Options.License = TEXT("user_confirmed");
	Options.SkeletonProfile = TEXT("UE5_Mannequin");
	Options.SampleRate = 60;
	Options.bExtractRootMotion = false;
	Options.bGenerateFootContacts = true;

	TArray<FString> ExportedFiles;
	TArray<FString> Errors;

	for (const UAnimSequence* AnimSequence : AnimSequences)
	{
		FString OutFilePath;
		FText Error;
		if (FAegisTrainingClipExporter::ExportAnimSequenceToTrainingJson(AnimSequence, Options, OutFilePath, Error))
		{
			ExportedFiles.Add(OutFilePath);
		}
		else
		{
			Errors.Add(FString::Printf(TEXT("%s: %s"), *GetNameSafe(AnimSequence), *Error.ToString()));
		}
	}

	FString Message = FString::Printf(
		TEXT("Exported %d / %d AnimSequence asset(s) to:\n%s"),
		ExportedFiles.Num(),
		AnimSequences.Num(),
		*Options.OutputDirectory
	);

	if (Errors.Num() > 0)
	{
		Message += TEXT("\n\nErrors:\n");
		Message += FString::Join(Errors, TEXT("\n"));
	}

	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
#endif
}

void FAegisMotionEditorModule::ImportMocapCurvesToSelectedActionAsset()
{
#if WITH_EDITOR
	using namespace AegisMocapImportEditor;

	UAegisProceduralActionAsset* TargetAsset = GetSelectedAegisActionAsset();

	if (!TargetAsset)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::FromString(TEXT("Select an Aegis Procedural Action Asset in the Content Browser first."))
		);
		return;
	}

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	if (!DesktopPlatform)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::FromString(TEXT("DesktopPlatform module is unavailable."))
		);
		return;
	}

	TArray<FString> SelectedFiles;

	const bool bPicked = DesktopPlatform->OpenFileDialog(
		nullptr,
		TEXT("Import Aegis Animation Curve JSON"),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("Aegis Curve JSON (*.json)|*.json"),
		EFileDialogFlags::None,
		SelectedFiles
	);

	if (!bPicked || SelectedFiles.Num() == 0)
	{
		return;
	}

	FString JsonText;

	if (!FFileHelper::LoadFileToString(JsonText, *SelectedFiles[0]))
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::FromString(TEXT("Failed to read the selected JSON file."))
		);
		return;
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::FromString(TEXT("Failed to parse the selected Aegis curve JSON."))
		);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* CurvesArray = nullptr;

	if (!RootObject->TryGetArrayField(TEXT("curves"), CurvesArray))
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::FromString(TEXT("The selected JSON file does not contain a 'curves' array."))
		);
		return;
	}

	FString SourceFormat;
	FString PlaybackMode;
	FString SkeletonProfile;
	RootObject->TryGetStringField(TEXT("sourceFormat"), SourceFormat);
	RootObject->TryGetStringField(TEXT("playbackMode"), PlaybackMode);
	RootObject->TryGetStringField(TEXT("skeletonProfile"), SkeletonProfile);
	const bool bGeneratedNative = IsGeneratedNativeJson(SourceFormat, PlaybackMode);
	const bool bLiveBaseOverlay = bGeneratedNative && IsLiveBaseGeneratedOverlayJson(SourceFormat, PlaybackMode);

	TargetAsset->Modify();
	TargetAsset->SourceFormat = SourceFormat;
	TargetAsset->SkeletonProfile = SkeletonProfile;
	TargetAsset->PlaybackMode = bLiveBaseOverlay
		? EAegisActionPlaybackMode::LiveBaseGeneratedOverlay
		: (bGeneratedNative
			? EAegisActionPlaybackMode::GeneratedNativeQuaternion
			: EAegisActionPlaybackMode::MocapExactQuaternion);
	TargetAsset->GenerationSummary = bLiveBaseOverlay
		? FString::Printf(TEXT("Generated live-base overlay JSON (%s)"), *SourceFormat)
		: (bGeneratedNative
			? FString::Printf(TEXT("Generated UE-native animation JSON (%s)"), *SourceFormat)
			: FString::Printf(TEXT("Imported legacy mocap JSON (%s)"), *SourceFormat));

	if (bGeneratedNative)
	{
		TargetAsset->MocapJointRemapTable.Reset();
		TargetAsset->ImportedMocapBindings.Reset();
		TargetAsset->Chains.Reset();
	}
	else if (TargetAsset->MocapJointRemapTable.Num() == 0)
	{
		TargetAsset->ResetMocapJointRemapTableToDefaults();
	}

	TargetAsset->AutoFixup_PhaseNamesAndDefaults();
	TargetAsset->AutoFixup_PopulateSocketBones();
	TargetAsset->AutoFixup_PhaseBoneSlots();

	// Imported BVH curves should play as-authored first.
	// Disable the automatic triangular phase envelope on the first phase of every chain,
	// otherwise exact BVH values get faded in/out before the developer even starts tuning.
	for (FAegisChainDef_Inline& Chain : TargetAsset->Chains)
	{
		if (Chain.Phases.Num() > 0)
		{
			Chain.Phases[0].bUseAutomaticPhaseWeight = false;
			Chain.Phases[0].StartTime01 = 0.0f;
			Chain.Phases[0].PeakTime01 = 0.5f;
			Chain.Phases[0].EndTime01 = 1.0f;
		}
	}

	double DurationSeconds = 0.0;

	if (RootObject->TryGetNumberField(TEXT("durationSeconds"), DurationSeconds)
		&& DurationSeconds > 0.0)
	{
		TargetAsset->DurationSeconds = static_cast<float>(DurationSeconds);
	}

	TargetAsset->ImportedMocapBindings.Reset();
	TargetAsset->LastImportedMocapJsonPath = SelectedFiles[0];

	int32 ImportedCurveCount = 0;
	int32 MatchedCurveCount = 0;

	for (const TSharedPtr<FJsonValue>& CurveValue : *CurvesArray)
	{
		const TSharedPtr<FJsonObject> CurveObject =
			CurveValue.IsValid() ? CurveValue->AsObject() : nullptr;

		if (!CurveObject.IsValid())
		{
			continue;
		}

		FString CurveName;
		FString JointName;
		FString ChannelName;

		CurveObject->TryGetStringField(TEXT("curveName"), CurveName);
		CurveObject->TryGetStringField(TEXT("jointName"), JointName);
		CurveObject->TryGetStringField(TEXT("channelName"), ChannelName);

		if (CurveName.IsEmpty())
		{
			CurveName = FString::Printf(TEXT("Curve_%d"), ImportedCurveCount + 1);
		}

		if (JointName.IsEmpty())
		{
			FString ParsedJointName;
			if (CurveName.Split(TEXT("."), &ParsedJointName, nullptr))
			{
				JointName = ParsedJointName;
			}
		}

		// Generated JSON may include human-readable authoring/debug degree channels.
		// They are useful in external viewers but must not create stale asset slots.
		if (IsAuthoringDebugCurve(CurveName, ChannelName))
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* KeysArray = nullptr;

		if (!CurveObject->TryGetArrayField(TEXT("keys"), KeysArray) || !KeysArray)
		{
			continue;
		}

		UCurveFloat* Curve =
			CreateCurveInsideActionAsset(TargetAsset, CurveName, *KeysArray, DurationSeconds);

		if (!Curve)
		{
			continue;
		}

		FAegisImportedMocapCurveBinding Binding;

		if (AssignCurveToMatchingBoneSlot(
			TargetAsset,
			CurveName,
			JointName,
			ChannelName,
			Curve,
			Binding,
			bGeneratedNative))
		{
			MatchedCurveCount++;
		}
		else
		{
			Binding.SourceCurveName = FName(*CurveName);
			Binding.SourceJointName = FName(*JointName);
			Binding.SourceChannelName = FName(*ChannelName);
			Binding.Target = DetectTarget(CurveName, ChannelName);
			Binding.Axis = DetectAxis(CurveName, ChannelName);
			Binding.bMatchedToPhaseBoneSlot = false;
		}

		if (!bGeneratedNative)
		{
			TargetAsset->ImportedMocapBindings.Add(Binding);
		}
		ImportedCurveCount++;
	}

	TargetAsset->MarkPackageDirty();

	FMessageDialog::Open(
		EAppMsgType::Ok,
		FText::Format(
			FText::FromString(
				TEXT("Imported {0} Aegis animation curves into {1}.\nMatched {2} curves to generated/native playback slots.\n\nGenerated-native imports create a clean full-body quaternion action, skip debug/authoring curves automatically, and bind foot IK/contact curves when present.")
			),
			FText::AsNumber(ImportedCurveCount),
			FText::FromString(TargetAsset->GetName()),
			FText::AsNumber(MatchedCurveCount)
		)
	);
#endif
}