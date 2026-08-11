#pragma once
#include "CoreMinimal.h"
#include "ResourceTypes.generated.h"

USTRUCT(BlueprintType)
struct FResources
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	int32 Mineral = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	int32 Ether = 0;

};