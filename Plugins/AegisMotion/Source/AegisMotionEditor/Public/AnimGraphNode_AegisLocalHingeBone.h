#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "ProceduralMotion/AnimNodes/AnimNode_AegisLocalHingeBone.h"
#include "AnimGraphNode_AegisLocalHingeBone.generated.h"

UCLASS()
class AEGISMOTIONEDITOR_API UAnimGraphNode_AegisLocalHingeBone : public UAnimGraphNode_SkeletalControlBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Settings")
    FAnimNode_AegisLocalHingeBone Node;

    // UEdGraphNode interface
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override
    {
        return FText::FromString(TEXT("Aegis Local Hinge Bone"));
    }

    virtual FText GetTooltipText() const override
    {
        return FText::FromString(TEXT("Rotate a single bone about a hinge axis in parent space with per-joint limits."));
    }

    virtual FText GetMenuCategory() const override
    {
        return FText::FromString(TEXT("AegisMotion"));
    }

protected:
    // UAnimGraphNode_SkeletalControlBase interface
    virtual const FAnimNode_SkeletalControlBase* GetNode() const override
    {
        return &Node;
    }
};
