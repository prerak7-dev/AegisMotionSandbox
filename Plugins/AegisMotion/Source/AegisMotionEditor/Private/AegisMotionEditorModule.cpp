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
#include "AegisAction/AegisProceduralActionComponent.h"

IMPLEMENT_MODULE(FAegisMotionEditorModule, AegisMotionEditor)

void FAegisMotionEditorModule::StartupModule()
{
#if WITH_EDITOR
	RegisterCustomizations();
	RegisterVisualizers();
#endif
}

void FAegisMotionEditorModule::ShutdownModule()
{
#if WITH_EDITOR
	UnregisterVisualizers();
	UnregisterCustomizations();
#endif
}

void FAegisMotionEditorModule::RegisterCustomizations()
{
#if WITH_EDITOR
	if (!FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FModuleManager::Get().LoadModule("PropertyEditor");
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomClassLayout(
		UAegisProceduralActionComponent::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FAegisProceduralActionComponentDetails::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();
#endif
}

void FAegisMotionEditorModule::UnregisterCustomizations()
{
#if WITH_EDITOR
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(UAegisProceduralActionComponent::StaticClass()->GetFName());
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

	GUnrealEd->RegisterComponentVisualizer(UAegisProceduralActionComponent::StaticClass()->GetFName(), ActionComponentVisualizer);
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
		GUnrealEd->UnregisterComponentVisualizer(UAegisProceduralActionComponent::StaticClass()->GetFName());
	}
	ActionComponentVisualizer.Reset();
#endif
}
