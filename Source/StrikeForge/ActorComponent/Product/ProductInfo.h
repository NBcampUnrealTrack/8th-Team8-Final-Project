#pragma once
#include "CoreMinimal.h"
#include "ProductInfo.generated.h"

USTRUCT(BlueprintType)
struct FProductInfo
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "Product")
	FName RowName = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Product")
	float RequiredWork = 0.f;
};