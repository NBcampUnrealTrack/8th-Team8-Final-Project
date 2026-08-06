#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Damagable.generated.h"

UINTERFACE()
class UDamagable : public UInterface
{
	GENERATED_BODY()
};

class STRIKEFORGE_API IDamagable
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void TakeDamaged(float InDamage);
};
