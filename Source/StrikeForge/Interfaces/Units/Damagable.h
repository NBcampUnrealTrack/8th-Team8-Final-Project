#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Damagable.generated.h"

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFDamageInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	float Damage = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	TWeakObjectPtr<AActor> HitActor;
};

UINTERFACE()
class UDamagable : public UInterface
{
	GENERATED_BODY()
};

class STRIKEFORGE_API IDamagable
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	void TakeDamaged(const FSFDamageInfo& DamageInfo);
};
