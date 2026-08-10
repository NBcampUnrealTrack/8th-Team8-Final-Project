// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Productable.generated.h"

UINTERFACE()
class UProductable : public UInterface
{
	GENERATED_BODY()
};

class STRIKEFORGE_API IProductable
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void ProductUnits(FName InUnitName);
};
