#pragma once
#include "CoreMinimal.h"
#include "SFResourceTypes.generated.h"

UENUM(BlueprintType)
enum class EResourceType: uint8
{
	Mineral,
	Ether
	// Population
};

USTRUCT(BlueprintType)
struct FResources
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	int32 Mineral = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	int32 Ether = 0;
	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	// int32 CurrentPopulation = 0;
	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	// int32 MaxPopulation = 0;
};