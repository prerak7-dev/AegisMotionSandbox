#include "ProceduralMotion/Rig/AegisChainDistributor.h"

void AegisRig::FAegisChainDistributor::BuildWeights(
    int32 NumBones,
    EAegisDistributionMode Mode,
    TArray<float>& OutWeights
)
{
    OutWeights.Reset();

    if (NumBones <= 0)
    {
        return;
    }

    OutWeights.SetNum(NumBones);

    switch (Mode)
    {
    case EAegisDistributionMode::Uniform:
    {
        const float W = 1.f / float(NumBones);
        for (int32 i = 0; i < NumBones; ++i)
        {
            OutWeights[i] = W;
        }
        break;
    }

    case EAegisDistributionMode::RampToEnd:
    default:
    {
        // Simple ramp: weights proportional to (i+1). End bone gets highest weight.
        float Sum = 0.f;
        for (int32 i = 0; i < NumBones; ++i)
        {
            OutWeights[i] = float(i + 1);
            Sum += OutWeights[i];
        }

        // Normalize
        if (Sum > 0.f)
        {
            for (int32 i = 0; i < NumBones; ++i)
            {
                OutWeights[i] /= Sum;
            }
        }
        break;
    }
    }
}

void AegisRig::FAegisChainDistributor::DistributeDeltaDegrees(
    const FAegisResolvedBoneChain& Chain,
    float TotalDeltaDegrees,
    EAegisDistributionMode Mode,
    TArray<FAegisBoneAngleDelta>& OutDeltas
)
{
    OutDeltas.Reset();

    if (!Chain.IsValid())
    {
        return;
    }

    const int32 NumBones = Chain.BoneNames.Num();

    TArray<float> Weights;
    BuildWeights(NumBones, Mode, Weights);

    OutDeltas.Reserve(NumBones);

    for (int32 i = 0; i < NumBones; ++i)
    {
        FAegisBoneAngleDelta D;
        D.BoneName = Chain.BoneNames[i];
        D.BoneIndex = Chain.BoneIndices[i];
        D.DeltaDegrees = TotalDeltaDegrees * Weights[i];
        OutDeltas.Add(D);
    }
}
