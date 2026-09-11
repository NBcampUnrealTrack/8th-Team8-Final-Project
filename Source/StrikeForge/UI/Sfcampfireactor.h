#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFCampfireActor.generated.h"

class UPointLightComponent;
class UNiagaraComponent;

UCLASS()
class STRIKEFORGE_API ASFCampfireActor : public AActor
{
	GENERATED_BODY()

public:
	ASFCampfireActor();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

private:
	// Ä·ÇÁÆÄÀÌ¾î ºÒºû
	UPROPERTY(VisibleAnywhere, Category = "Campfire|Light")
	TObjectPtr<UPointLightComponent> FireLight;

	// Å« ºÒ²É
	UPROPERTY(VisibleAnywhere, Category = "Campfire|Fire")
	TObjectPtr<UNiagaraComponent> MainFire;

	// ÀÛÀº ºÒ²É
	UPROPERTY(VisibleAnywhere, Category = "Campfire|Fire")
	TObjectPtr<UNiagaraComponent> SmallFire;

	// Æ¢´Â ºÒ¾¾
	UPROPERTY(VisibleAnywhere, Category = "Campfire|Ember")
	TObjectPtr<UNiagaraComponent> Embers;

	// ºû ¼¼±â
	UPROPERTY(EditAnywhere, Category = "Campfire|Light")
	float BaseIntensity = 6500.0f;

	UPROPERTY(EditAnywhere, Category = "Campfire|Light")
	float FlickerAmount = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Campfire|Light")
	float FlickerSpeed = 7.0f;

	// ºÒ²É Èçµé¸²
	UPROPERTY(EditAnywhere, Category = "Campfire|Fire")
	float FlameSwayAmount = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Campfire|Fire")
	float FlameSwaySpeed = 2.5f;

	float TimePassed = 0.0f;

	FVector MainFireBaseLocation;
	FVector SmallFireBaseLocation;
};