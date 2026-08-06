#pragma once

#include "CoreMinimal.h"
#include "Datas/Building/BuildingAppearance.h"
#include "GameFramework/Actor.h"
#include "BuildingBase.generated.h"

//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FChangedBuildingState, EBuildingState, InCurrentState);

class UProgressComponent;
class UHealthComponent;

UENUM(BlueprintType)
enum class EBuildingState : uint8
{
	Placing,
	Building,
	Completed
};

UCLASS()
class STRIKEFORGE_API ABuildingBase : public AActor
{
	GENERATED_BODY()

public:
	ABuildingBase();
	
	//FChangedBuildingState OnChangedBuildingState;
	
	void LoadTierMeshList(int32 InCurrentTier);
	void LoadTierRatioList(int32 InCurrentTier);
	
	void BindFunction();
	void UnBindFunction();
	UFUNCTION()
	void ChangeMeshByRatio(int32 InCurrentRatioIndex);
	UFUNCTION()
	void CompletedBuilding();
	void SetBuildingState(const EBuildingState InState);
	
	void InitInfo(const FBuildingAppearance* InData);
	UFUNCTION(BlueprintCallable)
	void UpgradeBuilding(int32 UpgradeValue = 1);
	UFUNCTION(BlueprintCallable)
	void StartBuilding();
	void SetMesh(UStaticMesh* InMesh);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UHealthComponent> HealthComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UProgressComponent> ProgressComponent;
private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Information", meta=(ExposeOnSpawn="true", AllowPrivateAccess=true))
	FName RowFName;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	FText DisplayName;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	int32 TeamIndex;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	int32 CurrentTier = 0;
	
	
	UPROPERTY()
	FBuildingTierData AllTierMeshesSoftArray;
	UPROPERTY()
	TArray<UStaticMesh*> CurrentTierMeshes;
	UPROPERTY()
	TArray<float> CurrentTierRatios;
	UPROPERTY()
	TObjectPtr<USoundBase> BuildingCompleteSound;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta=(AllowPrivateAccess=true))
	EBuildingState CurrentMode = EBuildingState::Placing;
};
