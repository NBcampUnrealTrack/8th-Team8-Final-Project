#pragma once

#include "CoreMinimal.h"
#include "Buildings/RootBase.h"
#include "Datas/Building/BuildingAppearance.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Units/Damagable.h"
#include "BuildingBase.generated.h"

//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FChangedBuildingState, EBuildingState, InCurrentState);

class UProgressComponent;
class UHealthComponent;

UENUM(BlueprintType)
enum class EBuildingState : uint8
{
	None,
	Preview,
	UnderConstruction,
	Completed
};

UCLASS()
class STRIKEFORGE_API ABuildingBase 
	: public ARootBase
	, public IDamagable

{
	GENERATED_BODY()

public:
	ABuildingBase();
	
	//FChangedBuildingState OnChangedBuildingState;
	
	void InitializeBuildingData();
	void InitInfo(const FBuildingAppearance* InData);
	void LoadTierMeshList(int32 InCurrentTier);
	void LoadTierRatioList(int32 InCurrentTier);
	
	// state manager
	UFUNCTION(BlueprintCallable)
	void SetBuildingState(const EBuildingState InState);
	void ApplyBuildingState(const EBuildingState InState);
	
	// preview state
	void EnterPreviewState();
	
	// begin construct
	void BeginConstruction();
	void ApplyPreviewMesh();
	void ApplyPreviewMaterial(uint8 bIsPlaced);
	UFUNCTION()
	void ChangeMeshByRatio(int32 InCurrentRatioIndex);
	void SetMesh(UStaticMesh* InMesh);
	
	// completed construct
	UFUNCTION()
	void CompleteConstruction();
	void EnterCompletedState();
	void PlayBuildingEffect();
	
	// component delegate
	void BindFunction();
	void UnBindFunction();

	UFUNCTION(BlueprintCallable)
	void UpgradeBuilding(int32 UpgradeValue = 1);
	
	// inferfaces
	virtual FName OnSelected_Implementation() override;
	virtual void OnDeSelected_Implementation() override;
	
	virtual void TakeDamaged_Implementation(float InDamage) override;
	UFUNCTION()
	void OnTakeAttacking(float InBeforeHP, float InCurrentHP, float InMaxHP);
	UFUNCTION()
	void OnDeath();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UHealthComponent> HealthComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UProgressComponent> ProgressComponent;
private:

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
	UPROPERTY()
	TObjectPtr<UMaterialInstance> BuildingPreviewMaterialInstance;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> OverlayMID;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State", meta=(AllowPrivateAccess=true))
	EBuildingState CurrentBuildingState;
};
