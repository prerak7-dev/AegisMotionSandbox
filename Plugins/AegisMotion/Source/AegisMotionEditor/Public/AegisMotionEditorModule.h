#pragma once

#include "Modules/ModuleManager.h"

class FAegisMotionEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterCustomizations();
	void UnregisterCustomizations();

	void RegisterVisualizers();
	void UnregisterVisualizers();

	void RegisterMenus();
	void UnregisterMenus();
	void ImportMocapCurvesToSelectedActionAsset();
	void ExportSelectedAnimSequencesToTrainingJson();

	TSharedPtr<class FComponentVisualizer> ActionComponentVisualizer;
};
