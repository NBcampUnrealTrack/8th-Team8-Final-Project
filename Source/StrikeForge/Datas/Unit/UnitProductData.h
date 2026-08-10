#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UnitProductData.generated.h"

USTRUCT(BlueprintType)
struct FResources
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Product")
	int32 Mineral = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Product")
	int32 Ether = 0;	
};

USTRUCT(BlueprintType)
struct FUnitProductData: public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Product")
	FName UnitRowName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Product")
	float UnitProductionDuration;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Product")
	FResources UnitProductionCost;
	
};
