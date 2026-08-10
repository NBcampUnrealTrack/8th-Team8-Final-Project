#pragma once

#include "CoreMinimal.h"
#include "Buildings/SFRootBase.h"
#include "Datas/Building/BuildingAppearance.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Units/Damagable.h"
#include "SFBaseBuilding.generated.h"

//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FChangedBuildingState, EBuildingState, InCurrentState);

class UProgressComponent;
class UHealthComponent;
class UCombatTargetComponent;

UENUM(BlueprintType)
enum class EBuildingState : uint8
{
	None,
	Preview,
	UnderConstruction,
	Completed
};
// repec=============================================
UENUM(BlueprintType)
enum class ESFBuildingSpawnMode : uint8
{
	ViewPortPlacedMode UMETA(DisplayName = "Viewport placed"),
	PlayerPlacedMode UMETA(DisplayName = "PlayerController placed")
};
//=====================================================
UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API ASFBaseBuilding 
	: public ASFRootBase
	, public IDamagable

{
	GENERATED_BODY()

public:
	ASFBaseBuilding(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	virtual void OnBuildingDestroyed() {};
	virtual void OnConstructionFinished() {};
	virtual void TakeDamaged_Implementation(float InDamage) override;
	
	//FChangedBuildingState OnChangedBuildingState;
	
	void InitializeBuildingData();
	virtual void InitInfo(const FBuildingAppearance* InData);
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
	void ApplyPreviewMaterial(const bool CanPlaced);
	UFUNCTION()
	void ChangeMeshByRatio(int32 InCurrentRatioIndex);
	void SetMesh(UStaticMesh* InMesh);
	
	// completed construct
	UFUNCTION()
	void HandleCompleteConstruction();
	void EnterCompletedState();
	void PlayBuildingEffect();
	
	// component delegate
	void BindFunctionWithComponent();
	void UnBindFunctionWithComponent();

	UFUNCTION(BlueprintCallable)
	void UpgradeBuilding(int32 UpgradeValue = 1);
	
	// inferfaces
	// virtual FName OnSelected_Implementation() override;
	// virtual void OnDeSelected_Implementation() override;
	
	UFUNCTION()
	void OnTakeAttacking(float InBeforeHP, float InCurrentHP, float InMaxHP);
	UFUNCTION()
	void HandleHealthZero();
	
	// repec ========================================================================
	void ApplySpawnMode();
	UFUNCTION(BlueprintCallable, Category = "SF|Building")
	bool TryStartConstruction();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Building")
	FORCEINLINE FIntPoint GetGridSize() const { return CachedGridSize; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Building")
	FORCEINLINE ESFBuildingSpawnMode GetSpawnMode() const { return SpawnMode; }
	//================================================================================
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Building")
	int32 GetTeamIndex() const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UHealthComponent> HealthComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UProgressComponent> ProgressComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UCombatTargetComponent> CombatTargetComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Information", meta=(ExposeOnSpawn="true",AllowPrivateAccess=true))
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
	
	// repec ============================================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State", meta=(ExposeOnSpawn="true", AllowPrivateAccess=true))
	ESFBuildingSpawnMode SpawnMode = ESFBuildingSpawnMode::ViewPortPlacedMode;

	/** FBuildingAppearance::GridSize 를 InitInfo 시점에 캐싱해 둔 값. 배치 컴포넌트가 겹침 검사에 쓴다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	FIntPoint CachedGridSize = FIntPoint(1, 1);
private:
	
};
