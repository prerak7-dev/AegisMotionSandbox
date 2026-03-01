#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "ProceduralMotion/AnimNodes/AnimNode_AegisProceduralMotionDriver.h"
#include "AnimGraphNode_AegisProceduralMotionDriver.generated.h"

UCLASS()
class AEGISMOTIONEDITOR_API UAnimGraphNode_AegisProceduralMotionDriver : public UAnimGraphNode_SkeletalControlBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Settings")
    FAnimNode_AegisProceduralMotionDriver Node;

    // UEdGraphNode
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FLinearColor GetNodeTitleColor() const override;

    // UAnimGraphNode_Base
    virtual FString GetNodeCategory() const override;

protected:
    virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
