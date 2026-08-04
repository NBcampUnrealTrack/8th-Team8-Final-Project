#pragma once

#include "CoreMinimal.h"
#include "BuildingAppearance.generated.h"

USTRUCT(BlueprintType)
struct FOnBuildingMeshData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ProgressRatio = 0.f;              
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> BuildingMesh;   // 0 : mesh1 , 50 : mesh2 , 100 : mesh3
};

USTRUCT(BlueprintType)
struct FBuildingTierData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mesh")
	TArray<FOnBuildingMeshData> TierBuildingState; // tarray({0,mesh1}, {50,mesh2}, {100,mesh3})
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> CompleteSoundBase;
};

USTRUCT(BlueprintType)
struct FBuildingAppearance : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Info|Name")
	FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Info|Building")
	TObjectPtr<UStaticMesh> PreviewMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Info|Building")
	TArray<FBuildingTierData> AllTierBuildingStates;
	//    ........ hp
	
};
