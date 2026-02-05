#include "AegisAimMathLibrary.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

static void GetBasisVectorsWS(
    AActor* SourceActor,
    bool bUseMeshForwardRight,
    FVector& OutOriginWS,
    FVector& OutForwardWS,
    FVector& OutRightWS
)
{
    OutOriginWS = SourceActor->GetActorLocation();
    OutForwardWS = SourceActor->GetActorForwardVector();
    OutRightWS = SourceActor->GetActorRightVector();

    if (bUseMeshForwardRight)
    {
        if (ACharacter* Char = Cast<ACharacter>(SourceActor))
        {
            if (USkeletalMeshComponent* Mesh = Char->GetMesh())
            {
                OutOriginWS = Mesh->GetComponentLocation();
                OutForwardWS = Mesh->GetForwardVector();
                OutRightWS = Mesh->GetRightVector();
            }
        }
    }
}

bool UAegisAimMathLibrary::ComputeAimYawDegrees_FromActors(
    AActor* SourceActor,
    AActor* TargetActor,
    float& OutYawDegrees,
    bool bUseMeshForwardRight,
    float MaxAbsYawDegrees,
    bool bFlattenZ
)
{
    OutYawDegrees = 0.f;

    if (!SourceActor || !TargetActor)
    {
        return false;
    }

    FVector OriginWS, ForwardWS, RightWS;
    GetBasisVectorsWS(SourceActor, bUseMeshForwardRight, OriginWS, ForwardWS, RightWS);

    FVector DirWS = TargetActor->GetActorLocation() - OriginWS;

    if (bFlattenZ)
    {
        DirWS.Z = 0.f;
        ForwardWS.Z = 0.f;
        RightWS.Z = 0.f;
    }

    if (DirWS.IsNearlyZero())
    {
        return false;
    }

    DirWS.Normalize();
    ForwardWS.Normalize();
    RightWS.Normalize();

    // Signed yaw relative to forward/right basis
    const float X = FVector::DotProduct(DirWS, ForwardWS);
    const float Y = FVector::DotProduct(DirWS, RightWS);

    const float YawRad = FMath::Atan2(Y, X);
    float YawDeg = FMath::RadiansToDegrees(YawRad);

    if (MaxAbsYawDegrees > 0.f)
    {
        YawDeg = FMath::Clamp(YawDeg, -MaxAbsYawDegrees, MaxAbsYawDegrees);
    }

    OutYawDegrees = YawDeg;
    return true;
}
