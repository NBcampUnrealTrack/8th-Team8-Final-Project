// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "SFProductionBuilding.generated.h"

UCLASS()
class STRIKEFORGE_API ASFProductionBuilding : public ASFBaseBuilding
{
	GENERATED_BODY()

public:
	ASFProductionBuilding();

protected:

	virtual void BeginPlay() override;
	virtual void OnConstructionFinished() override;
};
