#include "ActionMotion/AegisActionMotionComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

UAegisActionMotionComponent::UAegisActionMotionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UAegisActionMotionComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UAegisActionMotionComponent::StartShoot(const FVector2D& StickDir, float ShotPower01, EAegisStrikeFoot InStrikeFoot)
{
    CachedStrikeFoot = InStrikeFoot;

    Output.StrikeFoot = CachedStrikeFoot;
    Output.bStrikeRightFoot = (CachedStrikeFoot == EAegisStrikeFoot::Right);

    // Normalize stick; if near zero, default forward.
    FVector2D SD = StickDir;
    if (SD.IsNearlyZero())
    {
        SD = FVector2D(1.f, 0.f);
    }
    SD.Normalize();

    CachedStickDir = SD;
    CachedShotPower01 = FMath::Clamp(ShotPower01, 0.f, 1.f);

    CurrentAction = EAegisSoccerAction::Shoot;
    ElapsedSeconds = 0.f;

    Output = FAegisActionMotionOutput{};
    Output.Action = CurrentAction;
}

void UAegisActionMotionComponent::StopAction()
{
    CurrentAction = EAegisSoccerAction::None;
    ElapsedSeconds = 0.f;
    Output = FAegisActionMotionOutput{};
}

FVector UAegisActionMotionComponent::ComputeShotDirectionWorld() const
{
    const AActor* Owner = GetOwner();
    if (!Owner)
    {
        return FVector::ForwardVector;
    }

    // Prefer controller yaw so stick direction is relative to camera facing.
    float YawDeg = Owner->GetActorRotation().Yaw;

    if (const APawn* Pawn = Cast<APawn>(Owner))
    {
        if (const AController* C = Pawn->GetController())
        {
            YawDeg = C->GetControlRotation().Yaw;
        }
    }

    const FRotator YawRot(0.f, YawDeg, 0.f);
    const FVector ForwardWS = YawRot.RotateVector(FVector::ForwardVector);
    const FVector RightWS = YawRot.RotateVector(FVector::RightVector);

    // StickDir: X forward, Y right
    FVector DirWS = (ForwardWS * CachedStickDir.X) + (RightWS * CachedStickDir.Y);
    DirWS.Z = 0.f;
    if (DirWS.IsNearlyZero())
    {
        return ForwardWS;
    }
    return DirWS.GetSafeNormal();
}

float UAegisActionMotionComponent::ComputeSignedYawToWorldDir(const FVector& ShotDirWS) const
{
    const AActor* Owner = GetOwner();
    if (!Owner) return 0.f;

    FVector Fwd = Owner->GetActorForwardVector();
    FVector Right = Owner->GetActorRightVector();
    Fwd.Z = 0.f; Right.Z = 0.f;

    Fwd.Normalize();
    Right.Normalize();

    FVector Dir = ShotDirWS;
    Dir.Z = 0.f;
    Dir.Normalize();

    const float X = FVector::DotProduct(Dir, Fwd);
    const float Y = FVector::DotProduct(Dir, Right);

    return FMath::RadiansToDegrees(FMath::Atan2(Y, X));
}

void UAegisActionMotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    Output = FAegisActionMotionOutput{};
    Output.Action = CurrentAction;

    Output.StrikeFoot = CachedStrikeFoot;
    Output.bStrikeRightFoot = (CachedStrikeFoot == EAegisStrikeFoot::Right);


    if (CurrentAction == EAegisSoccerAction::None)
    {
        return;
    }

    // Select profile
    const UAegisActionMotionProfile* Profile =
        (CurrentAction == EAegisSoccerAction::Shoot) ? ShootProfile : nullptr;

    if (!Profile || Profile->DurationSeconds <= 0.f || !Profile->TorsoYawWeight01)
    {
        // Missing data: stop action to avoid silent nonsense
        StopAction();
        return;
    }

    ElapsedSeconds += DeltaTime;

    const float Time01 = FMath::Clamp(ElapsedSeconds / Profile->DurationSeconds, 0.f, 1.f);
    Output.Time01 = Time01;

    Output.Contact01 = Profile->Contact01 ? FMath::Clamp(Profile->Contact01->GetFloatValue(Time01), 0.f, 1.f) : 0.f;
    Output.FollowPose01 = Profile->FollowPose01 ? FMath::Clamp(Profile->FollowPose01->GetFloatValue(Time01), 0.f, 1.f) : 0.f;
    Output.Replant01 = Profile->Replant01 ? FMath::Clamp(Profile->Replant01->GetFloatValue(Time01), 0.f, 1.f) : 0.f;


    Output.PlantLock01 = Profile->PlantFootLock01
        ? FMath::Clamp(Profile->PlantFootLock01->GetFloatValue(Time01), 0.f, 1.f)
        : 0.f;


    const float YawW01 = FMath::Clamp(Profile->TorsoYawWeight01->GetFloatValue(Time01), 0.f, 1.f);
    const float RollW01 = Profile->TorsoRollWeight01
        ? FMath::Clamp(Profile->TorsoRollWeight01->GetFloatValue(Time01), 0.f, 1.f)
        : 0.f;

    // --- Kick curves (0..1) -> degrees ---
    const float Hip01 = Profile->KickHipPitch01 ? FMath::Clamp(Profile->KickHipPitch01->GetFloatValue(Time01), 0.f, 1.f) : 0.f;
    const float Knee01 = Profile->KickKneePitch01 ? FMath::Clamp(Profile->KickKneePitch01->GetFloatValue(Time01), 0.f, 1.f) : 0.f;
    const float Ankle01 = Profile->KickAnklePitch01 ? FMath::Clamp(Profile->KickAnklePitch01->GetFloatValue(Time01), 0.f, 1.f) : 0.f;

    // Positive pitch should correspond to "kick forward" for your skeleton.
    // If it's backward, we’ll flip sign later (easy).
    Output.ShootKickHipPitchDeg = Hip01 * HipPitchDeg * CachedShotPower01;
    Output.ShootKickKneePitchDeg = Knee01 * KneePitchDeg * CachedShotPower01;
    Output.ShootKickAnklePitchDeg = Ankle01 * AnklePitchDeg * CachedShotPower01;

    // Expose overall action weight (use yaw curve as primary for now)
    Output.ActionWeight01 = YawW01;

    // Convert stick -> world direction and compute yaw delta
    const FVector ShotDirWS = ComputeShotDirectionWorld();
    const float DesiredYawDeg = ComputeSignedYawToWorldDir(ShotDirWS);

    // --- YAW ---
    float TorsoYaw = DesiredYawDeg * YawW01 * CachedShotPower01;
    TorsoYaw = FMath::Clamp(TorsoYaw, -MaxAbsTorsoYawDegrees, MaxAbsTorsoYawDegrees);
    Output.ShootTorsoYawDegrees = TorsoYaw;

    // --- ROLL / LEAN ---
    // Lean comes mostly from lateral stick (Y): push stick right => lean right (positive roll)
    // You can invert this if it feels backwards on your rig.
    const float StickLateral = FMath::Clamp(CachedStickDir.Y, -1.f, 1.f);

    float TorsoRoll = StickLateral * RollW01 * CachedShotPower01 * MaxAbsTorsoRollDegrees;
    TorsoRoll = FMath::Clamp(TorsoRoll, -MaxAbsTorsoRollDegrees, MaxAbsTorsoRollDegrees);
    Output.ShootTorsoRollDegrees = TorsoRoll;


    const float Follow01 = Profile->FollowThrough01
        ? FMath::Clamp(Profile->FollowThrough01->GetFloatValue(Time01), 0.f, 1.f)
        : 0.f;

    // Add (not multiply) so it's visible even when base curves return to 0.
    Output.ShootKickHipPitchDeg += Follow01 * FollowHipAddDeg * CachedShotPower01;
    Output.ShootKickKneePitchDeg += Follow01 * FollowKneeAddDeg * CachedShotPower01;
    Output.ShootKickAnklePitchDeg += Follow01 * FollowAnkleAddDeg * CachedShotPower01;

    const float FP = Output.FollowPose01;

    // Hold a clear follow-through shape after contact.
    // Additive so it works even if base curves decay to 0.
    Output.ShootKickHipPitchDeg += FP * FollowHipHoldDeg * CachedShotPower01;
    Output.ShootKickKneePitchDeg += FP * FollowKneeHoldDeg * CachedShotPower01;
    Output.ShootKickAnklePitchDeg += FP * FollowAnkleHoldDeg * CachedShotPower01;

    // End action at completion
    if (Time01 >= 1.f)
    {
        StopAction();
    }
}
