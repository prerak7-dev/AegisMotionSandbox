#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActionMotion/AegisActionMotionProfile.h"
#include "AegisActionMotionComponent.generated.h"

UENUM(BlueprintType)
enum class EAegisStrikeFoot : uint8
{
    Right,
    Left
};

USTRUCT(BlueprintType)
struct FAegisActionMotionOutput
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Action")
    EAegisSoccerAction Action = EAegisSoccerAction::None;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Action")
    float Time01 = 0.f;

    /** Additive torso yaw (degrees) for the current action. */
    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Shoot")
    float ShootTorsoYawDegrees = 0.f;

    /** 0..1 strength of the action (useful for blending). */
    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Action")
    float ActionWeight01 = 0.f;

    /** Additive torso roll (degrees) for the current action. (+ = lean right) */
    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Shoot")
    float ShootTorsoRollDegrees = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Shoot|Kick")
    float ShootKickHipPitchDeg = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Shoot|Kick")
    float ShootKickKneePitchDeg = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Shoot|Kick")
    float ShootKickAnklePitchDeg = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Shoot|Foot")
    EAegisStrikeFoot StrikeFoot = EAegisStrikeFoot::Right;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Shoot|Foot")
    bool bStrikeRightFoot = true;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Shoot|Plant")
    float PlantLock01 = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Shoot|Phases")
    float Contact01 = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Shoot|Phases")
    float FollowPose01 = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Shoot|Phases")
    float Replant01 = 0.f;

};

UCLASS(ClassGroup = (Aegis), meta = (BlueprintSpawnableComponent))
class AEGISSOCCERAIM_API UAegisActionMotionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAegisActionMotionComponent();

    /** Assign a profile for shoot action. (Later you can have a map of action->profile) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Shoot")
    TObjectPtr<UAegisActionMotionProfile> ShootProfile = nullptr;

    /** Clamp for safety */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Shoot")
    float MaxAbsTorsoYawDegrees = 60.f;

    /** Output updated every tick */
    UPROPERTY(BlueprintReadOnly, Category = "Aegis|Action")
    FAegisActionMotionOutput Output;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Shoot")
    float MaxAbsTorsoRollDegrees = 15.f;   // lean clamp (tune)


    /** Start the shoot action using stick direction (X forward, Y right). */
    UFUNCTION(BlueprintCallable, Category = "Aegis|Action")
    void StartShoot(const FVector2D& StickDir, float ShotPower01, EAegisStrikeFoot InStrikeFoot);

    UFUNCTION(BlueprintCallable, Category = "Aegis|Action")
    void StopAction();

    UFUNCTION(BlueprintCallable, Category = "Aegis|Action")
    FAegisActionMotionOutput GetActionOutput() const { return Output; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Shoot|Kick")
    float HipPitchDeg = 35.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Shoot|Kick")
    float KneePitchDeg = 70.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Shoot|Kick")
    float AnklePitchDeg = 25.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Shoot|Kick|FollowThrough")
    float FollowHipAddDeg = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Shoot|Kick|FollowThrough")
    float FollowKneeAddDeg = 12.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Shoot|Kick|FollowThrough")
    float FollowAnkleAddDeg = 18.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Shoot|Kick|FollowPose")
    float FollowHipHoldDeg = 18.f;     // thigh forward

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Shoot|Kick|FollowPose")
    float FollowKneeHoldDeg = 35.f;    // knee bent (keeps knee below hip visually)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|Shoot|Kick|FollowPose")
    float FollowAnkleHoldDeg = 10.f;   // relaxed toe point

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    // Current state
    EAegisSoccerAction CurrentAction = EAegisSoccerAction::None;
    float ElapsedSeconds = 0.f;

    FVector2D CachedStickDir = FVector2D(1.f, 0.f);
    float CachedShotPower01 = 1.f;

    /** Convert stick direction to a world direction using controller yaw (or actor yaw fallback). */
    FVector ComputeShotDirectionWorld() const;

    /** Compute signed yaw (degrees) from owner forward to ShotDir in owner-space basis. */
    float ComputeSignedYawToWorldDir(const FVector& ShotDirWS) const;

    EAegisStrikeFoot CachedStrikeFoot = EAegisStrikeFoot::Right;

};
