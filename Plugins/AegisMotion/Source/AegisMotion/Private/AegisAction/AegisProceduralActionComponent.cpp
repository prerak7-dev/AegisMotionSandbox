#include "AegisAction/AegisProceduralActionComponent.h"

UAegisProceduralActionComponent::UAegisProceduralActionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAegisProceduralActionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAegisProceduralActionComponent::StartAction(UAegisProceduralActionAsset* Action, float InAlpha)
{
	State.ActionAsset = Action;
	State.ActionAlpha = FMath::Clamp(InAlpha, 0.f, 1.f);
	State.Time01 = 0.f;
	State.bIsRunning = (Action != nullptr);
	ElapsedSeconds = 0.f;
}

void UAegisProceduralActionComponent::StopAction()
{
	State = FAegisActionRuntimeState{};
	ElapsedSeconds = 0.f;
}

void UAegisProceduralActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!State.bIsRunning || !State.ActionAsset || State.ActionAsset->DurationSeconds <= 0.f)
	{
		return;
	}

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
	}
}
