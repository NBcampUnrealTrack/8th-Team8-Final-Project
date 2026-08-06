// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Datas/Building/BuildingAppearance.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Units/Selectable.h"
#include "RootBase.generated.h"

UCLASS(Abstract)
class STRIKEFORGE_API ARootBase 
	: public AActor
	, public ISelectable
{
	GENERATED_BODY()

public:
	ARootBase();
	// virtual FName OnSelected_Implementation() override { return UnitRowName; }
	virtual FName OnSelected_Implementation() override { return RowFName; }
	virtual void OnDeSelected_Implementation() override {}
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Information", meta=(ExposeOnSpawn="true", AllowPrivateAccess=true))
	FName RowFName;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
};
