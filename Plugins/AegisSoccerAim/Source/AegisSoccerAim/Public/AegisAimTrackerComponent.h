#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AegisAimTrackerComponent.generated.h"

USTRUCT(BlueprintType)
struct FAegisAimOutput
{
    GENERATED_BODY()

    // --- Torso (spine) ---
    UPROPERTY(BlueprintReadOnly, Category = "Aegis|SoccerAim|Torso")
    float TorsoRawYawDegrees = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|SoccerAim|Torso")
    float TorsoSmoothedYawDegrees = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|SoccerAim|Torso")
    float TorsoFinalYawDegrees = 0.f;

    // --- Head/Neck ---
    UPROPERTY(BlueprintReadOnly, Category = "Aegis|SoccerAim|Head")
    float HeadRawYawDegrees = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|SoccerAim|Head")
    float HeadSmoothedYawDegrees = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Aegis|SoccerAim|Head")
    float HeadFinalYawDegrees = 0.f;

    // --- Shared ---
    UPROPERTY(BlueprintReadOnly, Category = "Aegis|SoccerAim")
    float Engagement01 = 0.f;
};


/**
 * Tracks a target actor (e.g. ball) and produces:
 * - Raw yaw to target
 * - Engagement (0..1)
 * - Smoothed yaw
 * - Final yaw = SmoothedYaw * Engagement
 *
 * Designed to be read by an AnimInstance each frame.
 */
UCLASS(ClassGroup = (Aegis), meta = (BlueprintSpawnableComponent))
class AEGISSOCCERAIM_API UAegisAimTrackerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAegisAimTrackerComponent();

    /** The target actor we aim at (soccer ball). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|SoccerAim")
    TWeakObjectPtr<AActor> TargetActor;

    /** If true and owner is a Character, use mesh forward/right instead of actor forward/right (recommended for Manny). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|SoccerAim")
    bool bUseMeshForwardRight = true;

    /** Engagement distance range (cm). Inside Min => 1.0, beyond Max => 0.0 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|SoccerAim|Engagement")
    float EngageDistanceMin = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|SoccerAim|Engagement")
    float EngageDistanceMax = 4000.f;

    /** Additional engagement factor (e.g. only engage when “has interest”). 0..1 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|SoccerAim|Engagement")
    float ManualEngagement01 = 1.f;

    /** If true, ignore vertical difference (yaw-only). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|SoccerAim")
    bool bFlattenZ = true;

    /** Output values updated every tick. */
    UPROPERTY(BlueprintReadOnly, Category = "Aegis|SoccerAim")
    FAegisAimOutput Output;

    // Torso tuning
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|SoccerAim|Torso")
    float TorsoYawScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|SoccerAim|Torso")
    float TorsoMaxAbsYawDegrees = 60.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|SoccerAim|Torso|Smoothing")
    float TorsoSmoothOmega = 10.f;

    // Head tuning
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|SoccerAim|Head")
    float HeadYawScale = 1.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|SoccerAim|Head")
    float HeadMaxAbsYawDegrees = 85.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aegis|SoccerAim|Head|Smoothing")
    float HeadSmoothOmega = 18.f;


public:
    UFUNCTION(BlueprintCallable, Category = "Aegis|SoccerAim")
    void SetTargetActor(AActor* NewTarget);

    UFUNCTION(BlueprintCallable, Category = "Aegis|SoccerAim")
    FAegisAimOutput GetAimOutput() const { return Output; }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    // Internal spring state
    float YawVel = 0.f;

    float ComputeEngagement01(const FVector& OwnerPos, const FVector& TargetPos) const;
    static float CriticallyDampedStep(float Current, float& InOutVel, float Target, float DeltaTime, float Omega);
    float TorsoYawVel = 0.f;
    float HeadYawVel = 0.f;

};


