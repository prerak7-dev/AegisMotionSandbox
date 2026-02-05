#include "ProceduralMotion/AnimNodes/AnimNode_AegisLocalHingeBone.h"
#include "Animation/AnimInstanceProxy.h"


void FAnimNode_AegisLocalHingeBone::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
    FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);
}


void FAnimNode_AegisLocalHingeBone::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
    FAnimNode_SkeletalControlBase::CacheBones_AnyThread(Context);
    Bone.Initialize(Context.AnimInstanceProxy->GetRequiredBones());
}

bool FAnimNode_AegisLocalHingeBone::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
    return Bone.IsValidToEvaluate(RequiredBones);
}

void FAnimNode_AegisLocalHingeBone::EvaluateSkeletalControl_AnyThread(
    FComponentSpacePoseContext& Output,
    TArray<FBoneTransform>& OutBoneTransforms)
{
    const FBoneContainer& RequiredBones = Output.Pose.GetPose().GetBoneContainer();

    const FCompactPoseBoneIndex BoneIndex = Bone.GetCompactPoseIndex(RequiredBones);
    if (BoneIndex == FCompactPoseBoneIndex(INDEX_NONE))
    {
        return;
    }

    const float UseAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
    if (UseAlpha <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const float ClampedDeg = FMath::Clamp(AngleDegrees, MinDegrees, MaxDegrees) * UseAlpha;

    const FCompactPoseBoneIndex ParentIndex = RequiredBones.GetParentBoneIndex(BoneIndex);
    if (ParentIndex == FCompactPoseBoneIndex(INDEX_NONE))
    {
        return;
    }

    const FTransform BoneCS = Output.Pose.GetComponentSpaceTransform(BoneIndex);
    const FTransform ParentCS = Output.Pose.GetComponentSpaceTransform(ParentIndex);

    const FQuat ParentInv = ParentCS.GetRotation().Inverse();
    const FQuat BonePS = ParentInv * BoneCS.GetRotation();

    FVector Axis = ParentSpaceAxis.GetSafeNormal();
    if (Axis.IsNearlyZero())
    {
        Axis = FVector(0.f, 1.f, 0.f);
    }

    const FQuat DeltaPS(Axis, FMath::DegreesToRadians(ClampedDeg));
    const FQuat NewBonePS = DeltaPS * BonePS;

    FTransform NewBoneCS = BoneCS;
    NewBoneCS.SetRotation(ParentCS.GetRotation() * NewBonePS);

    OutBoneTransforms.Add(FBoneTransform(BoneIndex, NewBoneCS));
}


void FAnimNode_AegisLocalHingeBone::GatherDebugData(FNodeDebugData& DebugData)
{
    FString DebugLine = DebugData.GetNodeName(this);
    DebugData.AddDebugItem(DebugLine);
}
