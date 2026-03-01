#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AegisAction/AegisProceduralActionAsset.h"
#include "AegisProceduralActionComponent.generated.h"

USTRUCT(BlueprintType)
struct AEGISMOTION_API FAegisActionRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Aegis|Action")
	TObjectPtr<UAegisProceduralActionAsset> ActionAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Aegis|Action")
	float Time01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Aegis|Action")
	float ActionAlpha = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Aegis|Action")
	bool bIsRunning = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAegisActionFinishedSig, UAegisProceduralActionAsset*, Action);

UCLASS(ClassGroup = (Aegis), meta = (BlueprintSpawnableComponent))
class AEGISMOTION_API UAegisProceduralActionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAegisProceduralActionComponent();

	UFUNCTION(BlueprintCallable, Category = "Aegis|Action")
	void StartAction(UAegisProceduralActionAsset* Action, float InAlpha = 1.f);

	UFUNCTION(BlueprintCallable, Category = "Aegis|Action")
	void StopAction();

	UFUNCTION(BlueprintCallable, Category = "Aegis|Action")
	FAegisActionRuntimeState GetState() const { return State; }

	// --- Accessors used by the AnimNode ---
	UFUNCTION(BlueprintCallable, Category = "Aegis|Action")
	bool IsActionActive() const { return State.bIsRunning && State.ActionAsset != nullptr; }

	UFUNCTION(BlueprintCallable, Category = "Aegis|Action")
	UAegisProceduralActionAsset* GetCurrentActionAsset() const { return State.ActionAsset; }

	UFUNCTION(BlueprintCallable, Category = "Aegis|Action")
	float GetActionTime01() const { return State.Time01; }

	UFUNCTION(BlueprintCallable, Category = "Aegis|Action")
	float GetActionAlpha() const { return State.ActionAlpha; }

	UPROPERTY(BlueprintAssignable, Category = "Aegis|Action")
	FAegisActionFinishedSig OnActionFinished;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	FAegisActionRuntimeState State;

	UPROPERTY()
	float ElapsedSeconds = 0.f;

	UPROPERTY(EditAnywhere, Category = "Aegis|Action")
	bool bAutoClearOnFinish = false;
};
