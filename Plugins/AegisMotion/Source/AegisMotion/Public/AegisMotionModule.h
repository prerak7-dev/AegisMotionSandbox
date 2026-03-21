#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAegisMotion, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogAegisMotionEditorPreview, Log, All);

class FAegisMotionModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};
