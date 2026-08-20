// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Minable.generated.h"

class ASFMiningBuilding;

UINTERFACE()
class UMinable : public UInterface
{
	GENERATED_BODY()
};

class STRIKEFORGE_API IMinable
{
	GENERATED_BODY()
public:
	virtual int32 Extract(const int32 InDigAmount) = 0;
};
