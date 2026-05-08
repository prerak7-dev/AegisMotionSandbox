#pragma once

#include "CoreMinimal.h"

class UAnimSequence;

struct FAegisTrainingClipExportOptions
{
	FString OutputDirectory;
	FString Action = TEXT("unknown");
	FString Style = TEXT("retargeted");
	FString DominantLeg = TEXT("unknown");
	FString License = TEXT("user_confirmed");
	FString SkeletonProfile = TEXT("UE5_Mannequin");
	int32 SampleRate = 60;
	bool bExtractRootMotion = false;
	bool bGenerateFootContacts = true;
};

class FAegisTrainingClipExporter
{
public:
	static bool ExportAnimSequenceToTrainingJson(
		const UAnimSequence* AnimSequence,
		const FAegisTrainingClipExportOptions& Options,
		FString& OutFilePath,
		FText& OutError
	);

	static TArray<UAnimSequence*> GetSelectedAnimSequences();
};
