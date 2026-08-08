#pragma once

#include "CoreMinimal.h"
#include "Datas/Resource/ResourceTypes.h"
#include "BuildingAppearance.generated.h"

USTRUCT(BlueprintType)
struct FOnBuildingMeshData
{
	GENERATED_BODY()
	
	// mesh by construct ratio
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<float,TSoftObjectPtr<UStaticMesh>> Ratio_Mesh;  
	// completed sound by tier
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> CompleteSoundBase;
	// can spawn unit lists
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Units")
	TArray<FName> SpawnableUnits;
	// current tier cost
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cost")
	FResources ConstructionCost;
};

USTRUCT(BlueprintType)
struct FBuildingTierData
{
	GENERATED_BODY()
	
	// 1tier 2tier 3tier : array
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mesh")
	TArray<FOnBuildingMeshData> TierBuildingState;

};

USTRUCT(BlueprintType)
struct FBuildingAppearance : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Info|Name")
	FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Info|Building|Class")
	TSubclassOf<AActor> BuildingClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info|Building|Grid")
	FIntPoint GridSize = FIntPoint(1, 1);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Info|Building|Mesh")
	TObjectPtr<UMaterialInstance> PreviewMeshMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Info|Building|Mesh")
	FBuildingTierData AllTierBuildingStates;
	//    ........ hp
	
};
