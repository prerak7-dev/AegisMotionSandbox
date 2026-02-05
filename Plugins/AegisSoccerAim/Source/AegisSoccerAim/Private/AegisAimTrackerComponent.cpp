#include "AegisAimTrackerComponent.h"
#include "AegisAimMathLibrary.h"

#include "GameFramework/Actor.h"

UAegisAimTrackerComponent::UAegisAimTrackerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UAegisAimTrackerComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UAegisAimTrackerComponent::SetTargetActor(AActor* NewTarget)
{
    TargetActor = NewTarget;
}

float UAegisAimTrackerComponent::ComputeEngagement01(const FVector& OwnerPos, const FVector& TargetPos) const
{
    const float Dist = FVector::Distance(OwnerPos, TargetPos);

    // Map distance to [1..0] with smooth falloff
    // Dist <= Min => 1
    // Dist >= Max => 0
    if (EngageDistanceMax <= EngageDistanceMin)
    {
        return 0.f;
    }

    const float T = FMath::Clamp((Dist - EngageDistanceMin) / (EngageDistanceMax - EngageDistanceMin), 0.f, 1.f);
    const float DistanceEngage = 1.f - T;

    return FMath::Clamp(DistanceEngage * FMath::Clamp(ManualEngagement01, 0.f, 1.f), 0.f, 1.f);
}

// Critically damped spring towards Target (stable, no overshoot for typical params)
float UAegisAimTrackerComponent::CriticallyDampedStep(float Current, float& InOutVel, float Target, float DeltaTime, float Omega)
{
    // Based on continuous-time critically damped spring:
    // x'' + 2*Omega*x' + Omega^2*(x-target) = 0
    // Discretized with stable exponential form.
    if (DeltaTime <= 0.f || Omega <= 0.f)
    {
        InOutVel = 0.f;
        return Target;
    }

    const float Exp = FMath::Exp(-Omega * DeltaTime);

    const float Delta = Current - Target;
    const float Temp = (InOutVel + Omega * Delta) * DeltaTime;

    InOutVel = (InOutVel - Omega * Temp) * Exp;
    return Target + (Delta + Temp) * Exp;
}

void UAegisAimTrackerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AActor* Owner = GetOwner();
    AActor* Target = TargetActor.Get();

    if (!Owner || !Target)
    {
        Output = FAegisAimOutput{};
        TorsoYawVel = 0.f;
        HeadYawVel = 0.f;
        return;
    }

    // Compute a shared raw yaw (unclamped except for safety). We'll scale/clamp separately for torso/head.
    float SharedRawYaw = 0.f;
    const bool bOk = UAegisAimMathLibrary::ComputeAimYawDegrees_FromActors(
        Owner,
        Target,
        SharedRawYaw,
        bUseMeshForwardRight,
        180.f,        // compute wide, clamp later per-part
        bFlattenZ
    );

    if (!bOk)
    {
        Output = FAegisAimOutput{};
        TorsoYawVel = 0.f;
        HeadYawVel = 0.f;
        return;
    }

    const float Engage01 = ComputeEngagement01(Owner->GetActorLocation(), Target->GetActorLocation());

    // --- Torso ---
    const float TorsoRaw = FMath::Clamp(SharedRawYaw * TorsoYawScale, -TorsoMaxAbsYawDegrees, TorsoMaxAbsYawDegrees);
    const float TorsoSmoothed = CriticallyDampedStep(Output.TorsoSmoothedYawDegrees, TorsoYawVel, TorsoRaw, DeltaTime, TorsoSmoothOmega);
    const float TorsoFinal = TorsoSmoothed * Engage01;

    // --- Head ---
    const float HeadRaw = FMath::Clamp(SharedRawYaw * HeadYawScale, -HeadMaxAbsYawDegrees, HeadMaxAbsYawDegrees);
    const float HeadSmoothed = CriticallyDampedStep(Output.HeadSmoothedYawDegrees, HeadYawVel, HeadRaw, DeltaTime, HeadSmoothOmega);
    const float HeadFinal = HeadSmoothed * Engage01;

    // Write output
    Output.Engagement01 = Engage01;

    Output.TorsoRawYawDegrees = TorsoRaw;
    Output.TorsoSmoothedYawDegrees = TorsoSmoothed;
    Output.TorsoFinalYawDegrees = TorsoFinal;

    Output.HeadRawYawDegrees = HeadRaw;
    Output.HeadSmoothedYawDegrees = HeadSmoothed;
    Output.HeadFinalYawDegrees = HeadFinal;
}

