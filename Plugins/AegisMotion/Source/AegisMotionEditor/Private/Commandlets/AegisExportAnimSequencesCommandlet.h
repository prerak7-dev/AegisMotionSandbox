#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AegisExportAnimSequencesCommandlet.generated.h"

/**
 * Batch exports AnimSequence assets to Aegis training JSON.
 *
 * Example:
 * UnrealEditor-Cmd.exe Project.uproject -run=AegisExportAnimSequences
 *   -ContentPath="/Game/AegisMotionTraining/BandaiNamco/RetargetedToManny"
 *   -OutputDir="C:/AegisPipeline/sample-data/training-json"
 *   -Action="soccer_kick_overlay"
 *   -Style="active"
 *   -DominantLeg="right"
 *   -SampleRate=60
 */
UCLASS()
class UAegisExportAnimSequencesCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UAegisExportAnimSequencesCommandlet();

	virtual int32 Main(const FString& Params) override;
};
