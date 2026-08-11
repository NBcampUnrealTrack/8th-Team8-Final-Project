// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Movable.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UMovable : public UInterface
{
	GENERATED_BODY()
};

class STRIKEFORGE_API IMovable
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void MoveTo(const FVector& InDestination);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void StopMove();
};
