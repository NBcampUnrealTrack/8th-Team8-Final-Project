#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SFBuildingCatalog.generated.h"

class USFBuildingDefinition;

UCLASS(BlueprintType)
class STRIKEFORGE_API USFBuildingCatalog
	: public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog")
	TArray<TSoftObjectPtr<USFBuildingDefinition>> Buildings;
};