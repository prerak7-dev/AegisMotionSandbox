#pragma once

#include "ProceduralMotion/Types/ProceduralMotionTypes.h"

class IAegisMotionModifier
{
public:
    virtual ~IAegisMotionModifier() = default;

    virtual float EvaluateDeltaDegrees(
        const FAegisProceduralMotionInput& Input
    ) const = 0;

    virtual FName GetDebugName() const { return NAME_None; }
};
