#pragma once

#include "CoreMinimal.h"
#include "ProceduralMotionTypes.h"

namespace AegisMotionStatUtils
{
    inline bool TryGetStat01(
        const TArray<FAegisStatSample>& Stats,
        const FName StatName,
        float& OutValue01
    )
    {
        for (const FAegisStatSample& S : Stats)
        {
            if (S.Name == StatName)
            {
                OutValue01 = FMath::Clamp(S.Value, 0.f, 1.f);
                return true;
            }
        }

        return false;
    }
}
