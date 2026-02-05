#pragma once

#include "CoreMinimal.h"

/** Simple per-bone delta (for now: degrees around a chosen axis). */
struct FAegisBoneAngleDelta
{
    FName BoneName = NAME_None;
    int32 BoneIndex = INDEX_NONE;

    /** Additive angle delta in degrees. */
    float DeltaDegrees = 0.f;
};
