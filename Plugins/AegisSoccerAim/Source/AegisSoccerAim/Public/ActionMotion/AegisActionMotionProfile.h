#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AegisActionMotionProfile.generated.h"

class UCurveFloat;

UENUM(BlueprintType)
enum class EAegisSoccerAction : uint8
{
    None,
    Shoot,
    Pass,
    Header,
    Dive
};

UCLASS(BlueprintType)
class AEGISSOCCERAIM_API UAegisActionMotionProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Action")
    EAegisSoccerAction Action = EAegisSoccerAction::Shoot;

    /** Total duration in seconds for this action playback. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Action")
    float DurationSeconds = 0.6f;

    /** 0..1 weight over time that scales the stick-direction yaw delta applied to the torso. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Shoot")
    TObjectPtr<UCurveFloat> TorsoYawWeight01 = nullptr;

    /** Optional: 0..1 curve marking the strike moment region (later used for contact/impulse). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Shoot")
    TObjectPtr<UCurveFloat> StrikeWindow01 = nullptr;

    /** 0..1 weight over time that scales the lean/roll during shoot. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Shoot")
    TObjectPtr<UCurveFloat> TorsoRollWeight01 = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Shoot|Kick")
    TObjectPtr<UCurveFloat> KickHipPitch01 = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Shoot|Kick")
    TObjectPtr<UCurveFloat> KickKneePitch01 = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Shoot|Kick")
    TObjectPtr<UCurveFloat> KickAnklePitch01 = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Shoot|Plant")
    TObjectPtr<UCurveFloat> PlantFootLock01 = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Shoot|Kick")
    TObjectPtr<UCurveFloat> FollowThrough01 = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Shoot|Phases")
    TObjectPtr<UCurveFloat> Contact01 = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Shoot|Phases")
    TObjectPtr<UCurveFloat> FollowPose01 = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis|Shoot|Phases")
    TObjectPtr<UCurveFloat> Replant01 = nullptr;

};
