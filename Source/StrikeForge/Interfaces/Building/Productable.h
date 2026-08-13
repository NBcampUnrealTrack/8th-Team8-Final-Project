// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ActorComponent/Product/ProductInfo.h"
#include "Datas/Resource/ResourceTypes.h"
#include "Productable.generated.h"

class UProductComponent;

UINTERFACE()
class UProductable : public UInterface
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FProductableData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> ProductableUnitFNames;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FProductInfo> CurrentProductingFNames;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FResources UpgradeCost;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<UProductComponent> ProductComponent;
};

class STRIKEFORGE_API IProductable
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void ProductUnits(FName InUnitName);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	FProductableData GetProductableData();
};
