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

	TSharedPtr<class FComponentVisualizer> ActionComponentVisualizer;
};
