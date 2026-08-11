#pragma once
#include "CoreMinimal.h"
#include "ProductInfo.generated.h"

USTRUCT(BlueprintType)
struct FProductInfo
{
	GENERATED_BODY()
	FName RowName;
	float Duration;
};