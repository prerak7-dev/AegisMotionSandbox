#include "AnimGraph/AnimGraphNode_AegisChainYawDistributor.h"

#define LOCTEXT_NAMESPACE "AegisMotionAnimGraph"

FText UAnimGraphNode_AegisChainYawDistributor::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("AegisChainYawDistributorTitle", "Aegis Chain Yaw Distributor");
}

FText UAnimGraphNode_AegisChainYawDistributor::GetTooltipText() const
{
    return LOCTEXT("AegisChainYawDistributorTooltip", "Distributes a yaw delta across a bone chain (Start->End).");
}

FLinearColor UAnimGraphNode_AegisChainYawDistributor::GetNodeTitleColor() const
{
    return FLinearColor(0.30f, 0.70f, 1.0f);
}

FString UAnimGraphNode_AegisChainYawDistributor::GetNodeCategory() const
{
    return TEXT("AegisMotion");
}

#undef LOCTEXT_NAMESPACE
