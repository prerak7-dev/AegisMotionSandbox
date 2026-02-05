#pragma once
#include "Modules/ModuleManager.h"

class FAegisMotionEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};
