// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/Minable.h"
#include "ResourceBase.generated.h"

class UBoxComponent;

UENUM(BlueprintType)
enum class EResourceType: uint8
{
	Mineral,
	Ether
};

UCLASS()
class STRIKEFORGE_API AResourceBase 
	: public AActor
	, public IMinable
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AResourceBase();
	
	EResourceType GetResourceType() const;
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BoxComponent;
	
	EResourceType ResourceType;
	
	int32 ResourceDeposit;
private:
};
