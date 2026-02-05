#pragma once

#include "CoreMinimal.h"
#include "ProceduralMotion/Rig/Types/AegisBoneChainTypes.h"

class USkeleton;

namespace AegisRig
{
    class FAegisBoneChainResolver
    {
    public:
        /** Resolve a chain definition against a USkeleton. Returns false and fills OutError if it fails. */
        static bool ResolveChain(
            const USkeleton* Skeleton,
            const FAegisBoneChainDefinition& Definition,
            FAegisResolvedBoneChain& OutChain,
            FString& OutError
        );

        /** Convenience: Resolve by start/end bone names. */
        static bool ResolveStartEnd(
            const USkeleton* Skeleton,
            FName ChainName,
            FName StartBone,
            FName EndBone,
            FAegisResolvedBoneChain& OutChain,
            FString& OutError
        );

        /** Convenience: Resolve an explicit list (must be contiguous parent chain). */
        static bool ResolveExplicitList(
            const USkeleton* Skeleton,
            FName ChainName,
            const TArray<FName>& Bones,
            FAegisResolvedBoneChain& OutChain,
            FString& OutError
        );
    };
}
