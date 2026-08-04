#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProgressComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UProgressComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProgressComponent();
	
	void InitProgress();
protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY()
	FTimerHandle TimerHandle;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progress", meta = (AllowPrivateAccess = true))
	float CurrentProgress = 0.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progress", meta = (AllowPrivateAccess = true))
	float MaxProgress = 100.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progress", meta = (AllowPrivateAccess = true))
	float AddProgressValue = 10.f;
};
