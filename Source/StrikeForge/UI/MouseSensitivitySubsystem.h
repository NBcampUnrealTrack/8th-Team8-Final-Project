#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MouseSensitivitySubsystem.generated.h"

UCLASS()
class STRIKEFORGE_API UMouseSensitivitySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetSensitivity(float NewSensitivity);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	float GetSensitivity() const { return CurrentSensitivity; }

private:
	UPROPERTY()
	float CurrentSensitivity = 1.0f;
};