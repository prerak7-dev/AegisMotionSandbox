#include "AnimGraphNodes/AnimGraphNode_AegisProceduralMotionDriver.h"

#define LOCTEXT_NAMESPACE "AegisMotion"

FText UAnimGraphNode_AegisProceduralMotionDriver::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("AegisProceduralMotionDriverTitle", "Aegis Procedural Motion Driver");
}

FText UAnimGraphNode_AegisProceduralMotionDriver::GetTooltipText() const
{
    return LOCTEXT("AegisProceduralMotionDriverTooltip", "Unified procedural motion driver (spine/head/hips + optional limb smoothing) driven by a motion model.");
}

FLinearColor UAnimGraphNode_AegisProceduralMotionDriver::GetNodeTitleColor() const
{
    return FLinearColor(0.25f, 0.8f, 0.55f);
}

FString UAnimGraphNode_AegisProceduralMotionDriver::GetNodeCategory() const
{
    return TEXT("Aegis|Procedural Motion");
}

#undef LOCTEXT_NAMESPACE
