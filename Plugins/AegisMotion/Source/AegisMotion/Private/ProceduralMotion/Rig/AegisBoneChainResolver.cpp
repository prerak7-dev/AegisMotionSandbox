#include "ProceduralMotion/Rig/AegisBoneChainResolver.h"

#include "Animation/Skeleton.h"

static bool ValidateSkeleton(const USkeleton* Skeleton, FString& OutError)
{
    if (!Skeleton)
    {
        OutError = TEXT("Skeleton is null.");
        return false;
    }

    const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
    if (Ref.GetNum() <= 0)
    {
        OutError = TEXT("Skeleton reference skeleton has no bones.");
        return false;
    }

    return true;
}

static int32 FindBoneIndexChecked(const FReferenceSkeleton& Ref, const FName BoneName)
{
    return Ref.FindBoneIndex(BoneName);
}

bool AegisRig::FAegisBoneChainResolver::ResolveChain(
    const USkeleton* Skeleton,
    const FAegisBoneChainDefinition& Definition,
    FAegisResolvedBoneChain& OutChain,
    FString& OutError
)
{
    OutChain = FAegisResolvedBoneChain{};
    OutChain.ChainName = Definition.ChainName;

    if (!ValidateSkeleton(Skeleton, OutError))
    {
        return false;
    }

    if (Definition.ChainName.IsNone())
    {
        OutError = TEXT("ChainName is None.");
        return false;
    }

    if (Definition.Mode == EAegisChainDefinitionMode::StartEnd)
    {
        return ResolveStartEnd(Skeleton, Definition.ChainName, Definition.StartBone, Definition.EndBone, OutChain, OutError);
    }

    return ResolveExplicitList(Skeleton, Definition.ChainName, Definition.Bones, OutChain, OutError);
}

bool AegisRig::FAegisBoneChainResolver::ResolveStartEnd(
    const USkeleton* Skeleton,
    FName ChainName,
    FName StartBone,
    FName EndBone,
    FAegisResolvedBoneChain& OutChain,
    FString& OutError
)
{
    OutChain = FAegisResolvedBoneChain{};
    OutChain.ChainName = ChainName;

    if (!ValidateSkeleton(Skeleton, OutError))
    {
        return false;
    }

    if (ChainName.IsNone())
    {
        OutError = TEXT("ChainName is None.");
        return false;
    }
    if (StartBone.IsNone() || EndBone.IsNone())
    {
        OutError = TEXT("StartBone or EndBone is None.");
        return false;
    }

    const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
    const int32 StartIndex = FindBoneIndexChecked(Ref, StartBone);
    const int32 EndIndex = FindBoneIndexChecked(Ref, EndBone);

    if (StartIndex == INDEX_NONE)
    {
        OutError = FString::Printf(TEXT("StartBone '%s' not found in skeleton."), *StartBone.ToString());
        return false;
    }
    if (EndIndex == INDEX_NONE)
    {
        OutError = FString::Printf(TEXT("EndBone '%s' not found in skeleton."), *EndBone.ToString());
        return false;
    }

    // Walk from End -> parents until we reach Start.
    TArray<int32> IndicesReversed;
    int32 Current = EndIndex;

    while (Current != INDEX_NONE)
    {
        IndicesReversed.Add(Current);

        if (Current == StartIndex)
        {
            break;
        }

        Current = Ref.GetParentIndex(Current);
    }

    if (IndicesReversed.Last() != StartIndex)
    {
        OutError = FString::Printf(
            TEXT("StartBone '%s' is not an ancestor of EndBone '%s' (cannot resolve a single parent chain)."),
            *StartBone.ToString(),
            *EndBone.ToString()
        );
        return false;
    }

    Algo::Reverse(IndicesReversed);

    OutChain.BoneIndices = IndicesReversed;
    OutChain.BoneNames.Reserve(OutChain.BoneIndices.Num());
    for (int32 Idx : OutChain.BoneIndices)
    {
        OutChain.BoneNames.Add(Ref.GetBoneName(Idx));
    }

    return OutChain.IsValid();
}

bool AegisRig::FAegisBoneChainResolver::ResolveExplicitList(
    const USkeleton* Skeleton,
    FName ChainName,
    const TArray<FName>& Bones,
    FAegisResolvedBoneChain& OutChain,
    FString& OutError
)
{
    OutChain = FAegisResolvedBoneChain{};
    OutChain.ChainName = ChainName;

    if (!ValidateSkeleton(Skeleton, OutError))
    {
        return false;
    }

    if (ChainName.IsNone())
    {
        OutError = TEXT("ChainName is None.");
        return false;
    }

    if (Bones.Num() < 2)
    {
        OutError = TEXT("ExplicitList requires at least 2 bones.");
        return false;
    }

    const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();

    TArray<int32> Indices;
    Indices.Reserve(Bones.Num());

    for (const FName& BoneName : Bones)
    {
        const int32 BoneIndex = FindBoneIndexChecked(Ref, BoneName);
        if (BoneIndex == INDEX_NONE)
        {
            OutError = FString::Printf(TEXT("Bone '%s' not found in skeleton."), *BoneName.ToString());
            return false;
        }
        Indices.Add(BoneIndex);
    }

    // Validate contiguity: each next bone must be a descendant along direct parent links.
    for (int32 i = 1; i < Indices.Num(); ++i)
    {
        const int32 ParentIndex = Ref.GetParentIndex(Indices[i]);
        if (ParentIndex != Indices[i - 1])
        {
            OutError = FString::Printf(
                TEXT("ExplicitList is not a direct parent chain at '%s' -> '%s'. Expected '%s' parent to be '%s'."),
                *Ref.GetBoneName(Indices[i - 1]).ToString(),
                *Ref.GetBoneName(Indices[i]).ToString(),
                *Ref.GetBoneName(Indices[i]).ToString(),
                *Ref.GetBoneName(Indices[i - 1]).ToString()
            );
            return false;
        }
    }

    OutChain.BoneIndices = Indices;
    OutChain.BoneNames = Bones;
    return OutChain.IsValid();
}
