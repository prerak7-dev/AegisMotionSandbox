#include "AegisProceduralActionComponentDetails.h"

#include "AegisAction/AegisProceduralActionComponent.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IDetailCustomization> FAegisProceduralActionComponentDetails::MakeInstance()
{
	return MakeShared<FAegisProceduralActionComponentDetails>();
}

void FAegisProceduralActionComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	UAegisProceduralActionComponent* Comp = nullptr;
	if (Objects.Num() > 0)
	{
		Comp = Cast<UAegisProceduralActionComponent>(Objects[0].Get());
	}

	IDetailCategoryBuilder& DebugCat = DetailBuilder.EditCategory(TEXT("Aegis|Debug"));

	const TSharedRef<IPropertyHandle> DebugEnabledHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAegisProceduralActionComponent, bDebugScrubEnabled));
	const TSharedRef<IPropertyHandle> DebugTimeHandle    = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAegisProceduralActionComponent, DebugScrubTimeSeconds));
	const TSharedRef<IPropertyHandle> FreezeHandle       = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAegisProceduralActionComponent, bFreezeTimeWhenScrubbing));

	DebugCat.AddProperty(DebugEnabledHandle);
	DebugCat.AddProperty(FreezeHandle);

	DebugCat.AddCustomRow(FText::FromString(TEXT("ScrubTimeSlider")))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Scrub Time")))
	]
	.ValueContent()
	.MinDesiredWidth(320.f)
	[
		SNew(SSlider)
		.IsEnabled_Lambda([Comp]()
		{
			return Comp && Comp->bDebugScrubEnabled && Comp->GetCurrentActionAsset() != nullptr;
		})
		.Value_Lambda([Comp]()
		{
			if (!Comp) return 0.f;
			const UAegisProceduralActionAsset* Asset = Comp->GetCurrentActionAsset();
			if (!Asset || Asset->DurationSeconds <= KINDA_SMALL_NUMBER) return 0.f;
			return FMath::Clamp(Comp->DebugScrubTimeSeconds / Asset->DurationSeconds, 0.f, 1.f);
		})
		.OnValueChanged_Lambda([Comp](float NewValue)
		{
			if (!Comp) return;
			const UAegisProceduralActionAsset* Asset = Comp->GetCurrentActionAsset();
			if (!Asset || Asset->DurationSeconds <= KINDA_SMALL_NUMBER) return;

			const float NewSeconds = FMath::Clamp(NewValue, 0.f, 1.f) * Asset->DurationSeconds;

			const FScopedTransaction Tx(NSLOCTEXT("AegisMotion", "AegisScrubTime", "Change Aegis Scrub Time"));
			Comp->Modify();
			Comp->DebugScrubTimeSeconds = FMath::Clamp(NewSeconds, 0.f, Asset->DurationSeconds);
		})
	];

	// Expose raw seconds field too (precise typing)
	DebugCat.AddProperty(DebugTimeHandle);
}
