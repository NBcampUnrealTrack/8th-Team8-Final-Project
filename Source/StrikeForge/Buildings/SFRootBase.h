// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InGame/SFSelectINfo/SFSelectable.h"
#include "SFRootBase.generated.h"

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	int32 TeamIndex; // << 죽은 코드: combatcomponent가 teamindex 로직을 수행한다
						// 하지만 불수불가결하게 gamemode가 teamindex를 주는방향으로 개선되어야할것
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
};
