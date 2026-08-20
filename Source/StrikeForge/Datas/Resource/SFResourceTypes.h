#pragma once
#include "CoreMinimal.h"
#include "SFResourceTypes.generated.h"

UENUM(BlueprintType)
enum class EResourceType: uint8
{
	Mineral,
	Ether
};

USTRUCT(BlueprintType)
struct FResources
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	int32 Mineral = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	int32 Ether = 0;

};