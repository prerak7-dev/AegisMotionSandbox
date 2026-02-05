// Copyright Epic Games, Inc. All Rights Reserved.

#include "AegisMotion.h"

#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPath.h"
#include "Animation/Skeleton.h"

// Procedural motion smoke test
#include "ProceduralMotion/AegisProceduralMotionEvaluator.h"
#include "ProceduralMotion/Modifiers/AegisStatScalarModifier.h"
#include "ProceduralMotion/Modifiers/AegisCurveModifier.h"

// Rig/chain resolver
#include "ProceduralMotion/Rig/AegisBoneChainResolver.h"

#include "ProceduralMotion/Rig/AegisChainDistributor.h"
#include "ProceduralMotion/Modifiers/AegisAimYawAtWorldPointModifier.h"


#define LOCTEXT_NAMESPACE "FAegisMotionModule"

// Toggle smoke tests without touching logic elsewhere.
#ifndef AEGISMOTION_ENABLE_SMOKETEST
#define AEGISMOTION_ENABLE_SMOKETEST 1
#endif

#ifndef AEGISMOTION_ENABLE_RIG_SMOKETEST
#define AEGISMOTION_ENABLE_RIG_SMOKETEST 1
#endif

static void RunAegisMotionSmokeTest()
{
    const FSoftObjectPath CurveSoftPath(TEXT("/Script/Engine.CurveFloat'/Game/AegisMotion/Curves/CM_TestCurve_Time_To_DelataDeg.CM_TestCurve_Time_To_DelataDeg'"));
    const UCurveFloat* Curve = Cast<UCurveFloat>(CurveSoftPath.TryLoad());

    if (!Curve)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AegisMotion] SmokeTest failed to load curve at: %s"), *CurveSoftPath.ToString());
        return;
    }

    FAegisProceduralMotionInput Input;
    Input.TimeSeconds = 1.0f;
    Input.BoneName = "spine_01";
    Input.BaseAngleDegrees = 10.f;
    Input.Stats = { { "Stamina", 0.5f } };

    FAegisProceduralMotionEvaluator Eval;
    Eval.AddModifier(MakeShared<FAegisCurveModifier>(Curve, 1.0f));                 // +15 at t=1 (from your curve)
    Eval.AddModifier(MakeShared<FAegisStatScalarModifier>("Stamina", 20.f));       // +10 (0.5 * 20)

    const float OutAngle = Eval.EvaluateAngleDegrees(Input);
    UE_LOG(LogTemp, Log, TEXT("[AegisMotion] SmokeTest OutAngle=%.2f (expected ~35.00)"), OutAngle);
}

static void RunAegisRigChainResolverSmokeTest()
{
    // Your copied reference string resolves to this object path:
    // /Script/Engine.Skeleton'/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin'
    //
    // FSoftObjectPath accepts the object path portion:
    const FSoftObjectPath SkeletonPath(TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"));
    const USkeleton* Skeleton = Cast<USkeleton>(SkeletonPath.TryLoad());

    if (!Skeleton)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AegisMotion][Rig] Failed to load USkeleton at: %s"), *SkeletonPath.ToString());
        UE_LOG(LogTemp, Warning, TEXT("[AegisMotion][Rig] Tip: Right-click the Skeleton asset -> Copy Reference, then ensure the object path exists."));
        return;
    }

    FString Error;
    FAegisResolvedBoneChain SpineChain;

    // UE5 mannequin typically has spine_01..spine_05
    const bool bOk = AegisRig::FAegisBoneChainResolver::ResolveStartEnd(
        Skeleton,
        "Spine",
        "spine_01",
        "spine_05",
        SpineChain,
        Error
    );

    if (!bOk)
    {
        UE_LOG(LogTemp, Error, TEXT("[AegisMotion][Rig] Failed to resolve chain: %s"), *Error);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[AegisMotion][Rig] Resolved chain '%s' bones=%d"),
        *SpineChain.ChainName.ToString(), SpineChain.BoneNames.Num());

    for (int32 i = 0; i < SpineChain.BoneNames.Num(); ++i)
    {
        UE_LOG(LogTemp, Log, TEXT("[AegisMotion][Rig]  %02d: %s (index=%d)"),
            i,
            *SpineChain.BoneNames[i].ToString(),
            SpineChain.BoneIndices[i]
        );
    }

    // Simulate a ball 300 units to the character's right in world space
    FAegisProceduralMotionInput AimInput;
    AimInput.World.MeshComponentTransform = FTransform::Identity;
    AimInput.World.BallWorldPosition = FVector(0.f, 300.f, 0.f);
    AimInput.World.AnchorComponentSpace = FVector::ZeroVector;

    FAegisProceduralMotionEvaluator AimEval;
    AimEval.AddModifier(
        MakeShared<FAegisAimYawAtWorldPointModifier>(60.f)
    );

    const float AimYawDeg = AimEval.EvaluateAngleDegrees(AimInput);

    UE_LOG(LogTemp, Log,
        TEXT("[AegisMotion][Aim] Ball yaw delta = %.2f deg"),
        AimYawDeg
    );


    // Distribute a sample yaw delta across the spine chain
    const float TotalYawDeltaDeg = 30.f;

    TArray<FAegisBoneAngleDelta> Deltas;
    AegisRig::FAegisChainDistributor::DistributeDeltaDegrees(
        SpineChain,
        TotalYawDeltaDeg,
        EAegisDistributionMode::RampToEnd,
        Deltas
    );

    UE_LOG(LogTemp, Log, TEXT("[AegisMotion][Rig] Distribute %.2f deg across '%s' (mode=RampToEnd)"),
        TotalYawDeltaDeg, *SpineChain.ChainName.ToString());

    float SumCheck = 0.f;
    for (const FAegisBoneAngleDelta& D : Deltas)
    {
        SumCheck += D.DeltaDegrees;
        UE_LOG(LogTemp, Log, TEXT("[AegisMotion][Rig]  %s -> %+0.3f deg"), *D.BoneName.ToString(), D.DeltaDegrees);
    }

    UE_LOG(LogTemp, Log, TEXT("[AegisMotion][Rig] SumCheck = %.3f deg (expected %.3f)"),
        SumCheck, TotalYawDeltaDeg);

}

void FAegisMotionModule::StartupModule()
{
    UE_LOG(LogTemp, Log, TEXT("AegisMotion module loaded"));

#if AEGISMOTION_ENABLE_SMOKETEST
    RunAegisMotionSmokeTest();
#endif

#if AEGISMOTION_ENABLE_RIG_SMOKETEST
    RunAegisRigChainResolverSmokeTest();
#endif
}

void FAegisMotionModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAegisMotionModule, AegisMotion)
