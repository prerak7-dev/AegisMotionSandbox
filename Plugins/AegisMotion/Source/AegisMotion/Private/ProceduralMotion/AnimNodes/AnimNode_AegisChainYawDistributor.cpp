#include "ProceduralMotion/AnimNodes/AnimNode_AegisChainYawDistributor.h"

#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"
#include "Algo/Reverse.h"

int32 PivotChainIndex = INDEX_NONE;


void FAnimNode_AegisChainYawDistributor::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
    FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);
}

void FAnimNode_AegisChainYawDistributor::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
    FAnimNode_SkeletalControlBase::CacheBones_AnyThread(Context);
    RebuildChainCache(Context.AnimInstanceProxy->GetRequiredBones());
}

void FAnimNode_AegisChainYawDistributor::UpdateInternal(const FAnimationUpdateContext& Context)
{
    FAnimNode_SkeletalControlBase::UpdateInternal(Context);
}

void FAnimNode_AegisChainYawDistributor::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
    StartBone.Initialize(RequiredBones);
    EndBone.Initialize(RequiredBones);

    RebuildChainCache(RequiredBones);
}

bool FAnimNode_AegisChainYawDistributor::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
    return StartBone.IsValidToEvaluate(RequiredBones) && EndBone.IsValidToEvaluate(RequiredBones);
}

void FAnimNode_AegisChainYawDistributor::RebuildChainCache(const FBoneContainer& RequiredBones)
{
    ChainCompact.Reset();
    Weights.Reset();

    if (!StartBone.IsValidToEvaluate(RequiredBones) || !EndBone.IsValidToEvaluate(RequiredBones))
    {
        return;
    }

    const FCompactPoseBoneIndex StartCompact = StartBone.GetCompactPoseIndex(RequiredBones);
    const FCompactPoseBoneIndex EndCompact = EndBone.GetCompactPoseIndex(RequiredBones);

    if (StartCompact == INDEX_NONE || EndCompact == INDEX_NONE)
    {
        return;
    }

    // Walk from End -> parents until we reach Start (all in CompactPose space)
    TArray<FCompactPoseBoneIndex> ChainReversed;
    FCompactPoseBoneIndex Current = EndCompact;

    while (Current != INDEX_NONE)
    {
        ChainReversed.Add(Current);

        if (Current == StartCompact)
        {
            break;
        }

        Current = RequiredBones.GetParentBoneIndex(Current);
    }

    if (ChainReversed.Num() == 0 || ChainReversed.Last() != StartCompact)
    {
        // Start is not an ancestor of End in the current required-bones container
        return;
    }

    Algo::Reverse(ChainReversed);
    ChainCompact = MoveTemp(ChainReversed);

    PivotChainIndex = INDEX_NONE;

    if (!PivotBoneName.IsNone())
    {
        for (int32 i = 0; i < ChainCompact.Num(); ++i)
        {
            // Convert compact pose index to bone name
            const FName BoneName = RequiredBones.GetReferenceSkeleton().GetBoneName(
                RequiredBones.GetSkeletonPoseIndexFromCompactPoseIndex(ChainCompact[i]).GetInt()
            );

            if (BoneName == PivotBoneName)
            {
                PivotChainIndex = i;
                break;
            }
        }
    }

    // If pivot not found, default to end of chain
    if (PivotChainIndex == INDEX_NONE && ChainCompact.Num() > 0)
    {
        PivotChainIndex = ChainCompact.Num() - 1;
    }


    RebuildWeights();
}


void FAnimNode_AegisChainYawDistributor::RebuildWeights()
{
    Weights.Reset();

    const int32 NumBones = ChainCompact.Num();
    if (NumBones <= 0)
    {
        return;
    }

    Weights.SetNum(NumBones);

    if (DistributionMode == EAegisDistributionMode::Uniform)
    {
        const float W = 1.f / float(NumBones);
        for (int32 i = 0; i < NumBones; ++i)
        {
            Weights[i] = W;
        }
    }

    else if (DistributionMode == EAegisDistributionMode::PivotToEnd)
    {
        const int32 N = ChainCompact.Num();
        Weights.SetNumZeroed(N);

        const int32 P = FMath::Clamp(PivotChainIndex, 0, N - 1);

        // We want weights increasing toward pivot/end.
        // Map index i to [0..1] based on distance to pivot.
        // Bones closer to pivot get higher weight.
        float Sum = 0.f;

        for (int32 i = 0; i < N; ++i)
        {
            const int32 Dist = FMath::Abs(i - P);

            // Invert distance: pivot gets 1, far bones get smaller.
            const float W = 1.f / (1.f + Dist);

            Weights[i] = W;
            Sum += W;
        }

        // Normalize
        if (Sum > KINDA_SMALL_NUMBER)
        {
            for (float& W : Weights) W /= Sum;
        }
    }


    else // RampToEnd
    {
        float Sum = 0.f;
        for (int32 i = 0; i < NumBones; ++i)
        {
            Weights[i] = float(i + 1);
            Sum += Weights[i];
        }
        if (Sum > 0.f)
        {
            for (int32 i = 0; i < NumBones; ++i)
            {
                Weights[i] /= Sum;
            }
        }
    }

}

void FAnimNode_AegisChainYawDistributor::EvaluateSkeletalControl_AnyThread(
    FComponentSpacePoseContext& Output,
    TArray<FBoneTransform>& OutBoneTransforms
)
{
    OutBoneTransforms.Reset();

    if (bRequireValidChain && ChainCompact.Num() == 0)
    {
        return;
    }

    // If chain not cached, try rebuilding using current required bones
    if (ChainCompact.Num() == 0)
    {
        RebuildChainCache(Output.AnimInstanceProxy->GetRequiredBones());
        if (bRequireValidChain && ChainCompact.Num() == 0)
        {
            return;
        }
    }

    if (ChainCompact.Num() != Weights.Num())
    {
        RebuildWeights();
        if (ChainCompact.Num() != Weights.Num())
        {
            return;
        }
    }

    const float ClampedYaw = FMath::Clamp(TotalYawDeltaDegrees, -MaxAbsYawDegrees, MaxAbsYawDegrees);
    const float ClampedRoll = FMath::Clamp(TotalRollDeltaDegrees, -MaxAbsRollDegrees, MaxAbsRollDegrees);

    if (FMath::IsNearlyZero(ClampedYaw) && FMath::IsNearlyZero(ClampedRoll))
    {
        return;
    }

    const FVector UpAxisCS = FVector::UpVector;       // Z
    const FVector ForwardAxisCS = FVector::ForwardVector;  // X

    OutBoneTransforms.Reserve(ChainCompact.Num());

    for (int32 i = 0; i < ChainCompact.Num(); ++i)
    {
        const FCompactPoseBoneIndex BoneIndex = ChainCompact[i];

        FTransform BoneCS = Output.Pose.GetComponentSpaceTransform(BoneIndex);

        const float BoneYawDeg = ClampedYaw * Weights[i];
        const float BoneRollDeg = ClampedRoll * Weights[i];

        const FQuat DeltaYaw(UpAxisCS, FMath::DegreesToRadians(BoneYawDeg));
        const FQuat DeltaRoll(ForwardAxisCS, FMath::DegreesToRadians(BoneRollDeg));

        // Apply yaw then roll in component space
        const FQuat NewRot = (DeltaRoll * DeltaYaw) * BoneCS.GetRotation();
        BoneCS.SetRotation(NewRot);

        OutBoneTransforms.Add(FBoneTransform(BoneIndex, BoneCS));
    }


    // Required: sort by bone index
    OutBoneTransforms.Sort([](const FBoneTransform& A, const FBoneTransform& B)
        {
            return A.BoneIndex < B.BoneIndex;
        });
}
