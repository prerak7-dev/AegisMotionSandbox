#include "AegisAction/AegisProceduralActionComponent.h"

#include "Components/SkeletalMeshComponent.h"

UAegisProceduralActionComponent::UAegisProceduralActionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetComponentTickEnabled(false);
}

void UAegisProceduralActionComponent::BeginPlay()
{
	Super::BeginPlay();
	SetActionTickEnabled(State.bIsRunning);
}

void UAegisProceduralActionComponent::SetActionTickEnabled(bool bEnabled)
{
	const bool bShouldTick = bEnabled
#if WITH_EDITOR
		|| (bDebugScrubEnabled && State.ActionAsset != nullptr)
#endif
		;
	SetComponentTickEnabled(bShouldTick);
	PrimaryComponentTick.SetTickFunctionEnable(bShouldTick);
}

float UAegisProceduralActionComponent::GetEffectiveTime01() const
{
#if WITH_EDITOR
	if (bDebugScrubEnabled && State.ActionAsset && State.ActionAsset->DurationSeconds > 0.f)
	{
		const float S = FMath::Clamp(DebugScrubTimeSeconds, 0.f, State.ActionAsset->DurationSeconds);
		return FMath::Clamp(S / State.ActionAsset->DurationSeconds, 0.f, 1.f);
	}
#endif
	return State.Time01;
}

void UAegisProceduralActionComponent::StartAction(UAegisProceduralActionAsset* Action, float InAlpha)
{
	State.ActionAsset = Action;
	State.ActionAlpha = FMath::Clamp(InAlpha, 0.f, 1.f);
	State.Time01 = 0.f;
	State.bIsRunning = (Action != nullptr);
	ElapsedSeconds = 0.f;
	DebugScrubTimeSeconds = 0.f;
	if (Action)
	{
		++ActionInstanceId;
	}
	SetActionTickEnabled(State.bIsRunning);
#if WITH_EDITOR
	RequestScrubPoseRefresh();
#endif
}

void UAegisProceduralActionComponent::StopAction()
{
	State = FAegisActionRuntimeState{};
	ElapsedSeconds = 0.f;
	SetActionTickEnabled(false);
#if WITH_EDITOR
	RequestScrubPoseRefresh();
#endif
}

void UAegisProceduralActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if WITH_EDITOR
	if (bDebugScrubEnabled && State.ActionAsset)
	{
		DebugScrubTimeSeconds = FMath::Clamp(DebugScrubTimeSeconds, 0.f, State.ActionAsset->DurationSeconds);
		RequestScrubPoseRefresh();
	}
#endif

	if (!State.ActionAsset || State.ActionAsset->DurationSeconds <= 0.f)
	{
		SetActionTickEnabled(false);
		return;
	}

	if (!State.bIsRunning)
	{
#if WITH_EDITOR
		if (bDebugScrubEnabled)
		{
			State.Time01 = GetEffectiveTime01();
			SetActionTickEnabled(true);
			return;
		}
#endif
		SetActionTickEnabled(false);
		return;
	}

#if WITH_EDITOR
	if (bDebugScrubEnabled && bFreezeTimeWhenScrubbing)
	{
		State.Time01 = GetEffectiveTime01();
		return;
	}
#endif

	ElapsedSeconds += DeltaTime;
	State.Time01 = FMath::Clamp(ElapsedSeconds / State.ActionAsset->DurationSeconds, 0.f, 1.f);

	if (ElapsedSeconds >= State.ActionAsset->DurationSeconds)
	{
		State.bIsRunning = false;
		OnActionFinished.Broadcast(State.ActionAsset);

		if (bAutoClearOnFinish)
		{
			StopAction();
		}
		else
		{
			SetActionTickEnabled(false);
		}
	}
}

#if WITH_EDITOR
void UAegisProceduralActionComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	SetActionTickEnabled(State.bIsRunning);
	RequestScrubPoseRefresh();
}

void UAegisProceduralActionComponent::RequestScrubPoseRefresh() const
{
	if (const AActor* Owner = GetOwner())
	{
		if (USkeletalMeshComponent* SkelComp = Owner->FindComponentByClass<USkeletalMeshComponent>())
		{
			SkelComp->TickAnimation(0.f, false);
			SkelComp->RefreshBoneTransforms();
			SkelComp->RefreshSlaveComponents();
			SkelComp->UpdateComponentToWorld();
			SkelComp->MarkRenderTransformDirty();
			SkelComp->MarkRenderDynamicDataDirty();
		}
	}
}
#endif
