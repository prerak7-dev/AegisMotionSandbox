#pragma once

#include "CoreMinimal.h"

/**
 * World-space inputs used by procedural motion modifiers.
 * Pure data: no logic, no UObject ownership.
 */
struct FAegisWorldContext
{
    /** World position of the ball (authoritative, replicated). */
    FVector BallWorldPosition = FVector::ZeroVector;

    /** Component-to-world transform of the character mesh. */
    FTransform MeshComponentTransform;

    /** Optional: anchor point in component space (e.g. pelvis or spine base). */
    FVector AnchorComponentSpace = FVector::ZeroVector;

    bool IsValid() const
    {
        return MeshComponentTransform.IsValid();
    }
};
