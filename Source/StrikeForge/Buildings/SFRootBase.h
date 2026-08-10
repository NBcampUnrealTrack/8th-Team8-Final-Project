// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Datas/Building/BuildingAppearance.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Units/Selectable.h"
#include "SFRootBase.generated.h"

UCLASS(Abstract)
class STRIKEFORGE_API ASFRootBase 
	: public AActor
	, public ISelectable
{
	GENERATED_BODY()

public:
	ASFRootBase();
	// virtual FName OnSelected_Implementation() override { return UnitRowName; }
	virtual FName OnSelected_Implementation() override { return RowFName; }
	virtual void OnDeSelected_Implementation() override {}
	int32 GetTeamIndex() const { return TeamIndex; }
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Information", meta=(ExposeOnSpawn="true", AllowPrivateAccess=true))
	FName RowFName;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	int32 TeamIndex;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
};
