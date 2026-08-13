// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "SFNexusBuilding.generated.h"

UCLASS()
class STRIKEFORGE_API ASFNexusBuilding : public ASFBaseBuilding
{
	GENERATED_BODY()

public:
	ASFNexusBuilding(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void OnBuildingDestroyed() override;
};
