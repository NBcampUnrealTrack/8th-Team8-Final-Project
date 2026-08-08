#pragma once

#include "CoreMinimal.h"
#include "BuildingAppearance.generated.h"

USTRUCT(BlueprintType)
struct FOnBuildingMeshData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<float,TSoftObjectPtr<UStaticMesh>> Ratio_Mesh;  
	// UPROPERTY(EditAnywhere, BlueprintReadOnly)
	// float ProgressRatio = 0.f;              
	// UPROPERTY(EditAnywhere, BlueprintReadOnly)
	// TSoftObjectPtr<UStaticMesh> BuildingMesh;   // 0 : mesh1 , 50 : mesh2 , 100 : mesh3
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> CompleteSoundBase;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Units")
	TArray<FName> SpawnableUnits;
};

USTRUCT(BlueprintType)
struct FBuildingTierData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mesh")
	TArray<FOnBuildingMeshData> TierBuildingState; // tarray({0,mesh1}, {50,mesh2}, {100,mesh3})
	// construct sound?

	
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
