// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "Game/Subsystem/BuildingGridInfo.h"
#include "GameFramework/Actor.h"
#include "Interface/Minable.h"
#include "InGame/SFSelectINfo/SFSelectable.h"
#include "ResourceBase.generated.h"

class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFOnResourceNodeDepleted, AResourceBase*, Resource);

UCLASS(Abstract)
class STRIKEFORGE_API AResourceBase 
	: public AActor
	, public ISFSelectable
	, public IMinable
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AResourceBase();
	
	FSFOnResourceNodeDepleted OnDepleted;
	
	virtual int32 Extract(int32 InDigAmount) override;
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	// Getter
	FORCEINLINE EResourceType GetResourceType() const { return ResourceType; }
	FORCEINLINE int32 GetResourceDeposit() const { return ResourceDeposit; }
	FORCEINLINE int32 GetResourceTier() const { return ResourceTier; }
	FORCEINLINE float GetResourceRequiredWork() const { return ResourceRequiredWork; }
	
	FORCEINLINE const FBuildingGridInfo& GetRegisteredResourceGridInfo() const { return RegisteredResourceGridInfo; }
	FORCEINLINE bool IsDepleted() const { return ResourceDeposit <= 0; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BoxComponent;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resource")
	EResourceType ResourceType = EResourceType::Mineral;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "0"))
	int32 ResourceDeposit = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "1"))
	int32 ResourceTier = 1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "1"))
	float ResourceRequiredWork = 50.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resource|Grid")
	FIntPoint ResourceGridSize = FIntPoint(2, 2);
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Resource|Grid")
	FBuildingGridInfo RegisteredResourceGridInfo;
	bool bResourceGridRegistered  = false;
private:
};
