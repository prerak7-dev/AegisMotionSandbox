#include "Commandlets/AegisExportAnimSequencesCommandlet.h"

#include "AegisTrainingClipExporter.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

UAegisExportAnimSequencesCommandlet::UAegisExportAnimSequencesCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

static FString AegisGetParamString(const FString& Params, const TCHAR* Key, const FString& DefaultValue)
{
	FString Out;
	return FParse::Value(*Params, Key, Out) ? Out : DefaultValue;
}

static int32 AegisGetParamInt(const FString& Params, const TCHAR* Key, const int32 DefaultValue)
{
	int32 Out = DefaultValue;
	FParse::Value(*Params, Key, Out);
	return Out;
}

static bool AegisGetParamBool(const FString& Params, const TCHAR* Key, const bool bDefaultValue)
{
	if (FParse::Param(*Params, Key))
	{
		return true;
	}
	const FString Value = AegisGetParamString(Params, Key, bDefaultValue ? TEXT("true") : TEXT("false"));
	return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("1"));
}

int32 UAegisExportAnimSequencesCommandlet::Main(const FString& Params)
{
	const FString ContentPath = AegisGetParamString(Params, TEXT("ContentPath="), TEXT("/Game"));
	const FString OutputDir = AegisGetParamString(Params, TEXT("OutputDir="), FPaths::ProjectSavedDir() / TEXT("AegisTrainingExports"));
	const FString Action = AegisGetParamString(Params, TEXT("Action="), TEXT("unknown"));
	const FString Style = AegisGetParamString(Params, TEXT("Style="), TEXT("retargeted"));
	const FString DominantLeg = AegisGetParamString(Params, TEXT("DominantLeg="), TEXT("unknown"));
	const FString License = AegisGetParamString(Params, TEXT("License="), TEXT("user_confirmed"));
	const FString SkeletonProfile = AegisGetParamString(Params, TEXT("SkeletonProfile="), TEXT("UE5_Mannequin"));
	const int32 SampleRate = AegisGetParamInt(Params, TEXT("SampleRate="), 60);
	const bool bExtractRootMotion = AegisGetParamBool(Params, TEXT("ExtractRootMotion="), false);
	const bool bGenerateFootContacts = AegisGetParamBool(Params, TEXT("GenerateFootContacts="), true);

	UE_LOG(LogTemp, Display, TEXT("[Aegis V45] Batch export starting"));
	UE_LOG(LogTemp, Display, TEXT("[Aegis V45] ContentPath=%s"), *ContentPath);
	UE_LOG(LogTemp, Display, TEXT("[Aegis V45] OutputDir=%s"), *OutputDir);

	IFileManager::Get().MakeDirectory(*OutputDir, true);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	AssetRegistry.SearchAllAssets(true);

	FARFilter Filter;
	Filter.PackagePaths.Add(*ContentPath);
	Filter.bRecursivePaths = true;
	// UE5.x Asset Registry class filtering.
	// Avoid UE_VERSION_NEWER_THAN here because that macro is not available in all module include contexts.
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());

	TArray<FAssetData> AssetData;
	AssetRegistry.GetAssets(Filter, AssetData);

	if (AssetData.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Aegis V45] No AnimSequence assets found under %s"), *ContentPath);
		return 1;
	}

	FAegisTrainingClipExportOptions Options;
	Options.OutputDirectory = OutputDir;
	Options.Action = Action;
	Options.Style = Style;
	Options.DominantLeg = DominantLeg;
	Options.License = License;
	Options.SkeletonProfile = SkeletonProfile;
	Options.SampleRate = SampleRate;
	Options.bExtractRootMotion = bExtractRootMotion;
	Options.bGenerateFootContacts = bGenerateFootContacts;

	int32 SuccessCount = 0;
	int32 FailureCount = 0;

	for (const FAssetData& Data : AssetData)
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(Data.GetAsset());
		if (!AnimSequence)
		{
			continue;
		}

		FString OutFile;
		FText Error;
		if (FAegisTrainingClipExporter::ExportAnimSequenceToTrainingJson(AnimSequence, Options, OutFile, Error))
		{
			++SuccessCount;
			UE_LOG(LogTemp, Display, TEXT("[Aegis V45] Exported %s -> %s"), *AnimSequence->GetName(), *OutFile);
		}
		else
		{
			++FailureCount;
			UE_LOG(LogTemp, Error, TEXT("[Aegis V45] Failed %s: %s"), *AnimSequence->GetName(), *Error.ToString());
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[Aegis V45] Batch export complete. Success=%d Failure=%d"), SuccessCount, FailureCount);
	return FailureCount == 0 ? 0 : 2;
}
