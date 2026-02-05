#pragma once

#include "CoreMinimal.h"
#include "ProceduralMotion/Rig/Types/AegisBoneChainTypes.h"
#include "ProceduralMotion/Rig/Types/AegisDistributedMotionTypes.h"
#include "ProceduralMotion/Types/AegisRigEnums.h"


namespace AegisRig
{
    class FAegisChainDistributor
    {
    public:
        /**
         * Distribute a single delta across the chain.
         * OutDeltas will have one entry per chain bone.
         */
        static void DistributeDeltaDegrees(
            const FAegisResolvedBoneChain& Chain,
            float TotalDeltaDegrees,
            EAegisDistributionMode Mode,
            TArray<FAegisBoneAngleDelta>& OutDeltas
        );

    private:
        static void BuildWeights(
            int32 NumBones,
            EAegisDistributionMode Mode,
            TArray<float>& OutWeights
        );
    };
}
