#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "UnitProductData.generated.h"

USTRUCT(BlueprintType)
struct FUnitProductData: public FTableRowBase
{
	GENERATED_BODY()
	
	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Product")
	// FName UnitRowName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Product")
	float UnitProductionDuration;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Product")
	FResources UnitProductionCost;
	
};
