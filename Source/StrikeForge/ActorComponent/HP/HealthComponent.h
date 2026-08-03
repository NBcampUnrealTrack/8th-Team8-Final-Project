#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHealthValueChanged, float, InBeforeHP, float, InCurrentHP, float, InMaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHealthStateChanged);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHealthComponent();
	
	void ApplyDamage(float InDamage);
	
#pragma region Properties
	
	UPROPERTY(BlueprintAssignable)
	FHealthValueChanged OnHealthValueChanged;
	UPROPERTY(BlueprintAssignable)
	FHealthStateChanged OnHealthStateChanged;
	
	UPROPERTY()
	uint8 bIsDead;
#pragma endregion 
protected:
	virtual void BeginPlay() override;
	
private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = true))
	float CurrentHealth = 0.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = true))
	float MaxHealth = 100.f;
};
