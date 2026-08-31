// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InGame/SFSelectINfo/SFSelectable.h"
#include "SFRootBase.generated.h"

class UBoxComponent;

UCLASS(Abstract)
class STRIKEFORGE_API ASFRootBase 
	: public AActor
	, public ISFSelectable
{
	GENERATED_BODY()

public:
	ASFRootBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	// virtual FName OnSelected_Implementation() override { return UnitRowName; }
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	
	int32 GetTeamIndex() const;
	FORCEINLINE void SetRowName(const FName& InNewName) { RowName = InNewName; }
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Information", meta=(ExposeOnSpawn="true", AllowPrivateAccess=true))
	FName RowName;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	int32 TeamIndex;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<USceneComponent> SceneComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UBoxComponent> BoxComponent;
	
};
