#pragma once

#include "CoreMinimal.h"
#include "Buildings/SFRootBase.h"
#include "Datas/Building/BuildingAppearance.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Units/Damagable.h"
#include "SFBaseBuilding.generated.h"

class UProgressComponent;
class UHealthComponent;
class UCombatTargetComponent;
class UNiagaraSystem;
class UWidgetComponent;

UENUM(BlueprintType)
enum class EBuildingState : uint8
{
	None,
	Preview,
	UnderConstruction,
	Completed,
	Destroyed
};
UENUM(BlueprintType)
enum class ESFBuildingSpawnMode : uint8
{
	ViewPortPlacedMode UMETA(DisplayName = "Viewport placed"),
	PlayerPlacedMode UMETA(DisplayName = "PlayerController placed")
};

USTRUCT()
struct FBuildingTierRuntimeAssets
{
	GENERATED_BODY()

	int32 TierIndex = INDEX_NONE;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> ConstructionMeshes;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CompletedMesh = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> DamageMeshes;

	UPROPERTY()
	TObjectPtr<UStaticMesh> DestroyedMesh = nullptr;
	
	UPROPERTY()
	TObjectPtr<USoundBase> ConstructionStartSound = nullptr;

	UPROPERTY()
	TObjectPtr<USoundBase> ConstructionCompleteSound = nullptr;

	bool IsPreparedFor(int32 InTierIndex) const
	{
		return TierIndex == InTierIndex && IsValid(CompletedMesh);
	}

	void Reset()
	{
		TierIndex = INDEX_NONE;
		
		ConstructionMeshes.Reset();
		CompletedMesh = nullptr;
		DestroyedMesh = nullptr;
		DamageMeshes.Reset();
		
		ConstructionStartSound = nullptr;
		ConstructionCompleteSound = nullptr;
	}
};

UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API ASFBaseBuilding
	: public ASFRootBase
	, public IDamagable

{
	GENERATED_BODY()

public:
	ASFBaseBuilding(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void TakeDamaged_Implementation(float InDamage) override;
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	virtual const FHealthData GetHealthData_Implementation() override;
	
	/** 프리뷰 색상 갱신 */
	void ApplyPreviewMaterial(bool bCanPlace);
	
	// 커맨더 건설 시작 시작함수
	UFUNCTION(BlueprintCallable, Category = "SF|Building")
	bool TryStartConstruction();
	
	// 업그레이드 함수
	UFUNCTION(BlueprintCallable, Category = "SF|Building")
	void UpgradeBuilding(int32 UpgradeValue = 1);

	// Getter
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Building")
	FORCEINLINE FResources GetConstructCost() const { return CurrentConstructBuildCost; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Building")
	FORCEINLINE FIntPoint GetGridSize() const { return CachedGridSize; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Building")
	FORCEINLINE ESFBuildingSpawnMode GetSpawnMode() const { return SpawnMode; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Building")
	FBoxSphereBounds GetBuildBounds() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	// APPLY BUILDING's ABILITY
	virtual void OnTierActivated(const FBuildingTier& ActivatedTier) {}
	// DELETE BUILDING's ABILITY
	virtual void OnTierDeactivated(const FBuildingTier& DeactivatedTier) {}
	// when construct destroyed, play effects
	virtual void OnBuildingDestroyed() {}
	
	virtual void NotifyActorBeginCursorOver() override;
	virtual void NotifyActorEndCursorOver() override;
private:
	bool LoadBuildingDefinition();
	bool PrepareTier(int32 TierIndex);
	bool CanStartPreparedConstruction() const;
	bool CommitPreparedTier();
	bool FinalizeTierActivation();
	
	// Deactivate Ability
	void DeactivateActiveTier();
	
	void SetBuildingState(const EBuildingState InState);
	void ApplyBuildingState(const EBuildingState InState);
	void ApplySpawnMode();
	
	// constructing
	void EnterPreviewState();
	
	void BeginConstruction();
	
	void EnterCompletedState();
	
	void EnterDestroyedState();
	void ApplyDestroyedCollision();
	void PlayDestroyEffect();
	void PlayConstructionCompleteSound();
	
	// delegate
	void BindComponentDelegates();
	void UnbindComponentDelegates();

	// progress가 완료되면 호출되게 bind되있음
	// Finalize -> Committed(pre값->cur값) -> Set(completed)State -> Activate
	UFUNCTION() 
	void HandleCompleteConstruction();
	
	// 데미지 적용조건 함수
	bool CanReceiveDamage() const;
	// 체력 변경에 따른 이벤트함수
	UFUNCTION() // BIND with healthcomponent.applydamage
	void HandleHealthChanged(float InBeforeHP, float InCurrentHP, float InMaxHP);
	void ChangeDamageStage(int32 InDamageStageIndex);
	UFUNCTION()
	void HandleHealthZero();
	
	// 메쉬 변경 함수
	UFUNCTION()
	void ChangeMeshByRatio(int32 InCurrentRatioIndex);
	void RefreshVisual();
	UStaticMesh* ResolveMesh() const;
	UStaticMesh* ResolveDamageMesh(const FBuildingTierRuntimeAssets& Assets) const;
	const FBuildingTier* GetCurrentTier() const;
protected:
	// components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UHealthComponent> HealthComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UWidgetComponent> WidgetComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UProgressComponent> ProgressComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component", meta=(AllowPrivateAccess=true))
	TObjectPtr<UCombatTargetComponent> CombatTargetComponent;
	
	// ui
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	FText DisplayName;
	// 적용전과 후 tier 인덱스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Information", meta=(ExposeOnSpawn="true",AllowPrivateAccess=true))
	int32 CurrentTier = 0;
	int32 PendingTier = INDEX_NONE;
	int32 CurrentStageIndex = 0;

	// cost
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	FResources CurrentConstructBuildCost;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	FResources CurrentUpgradeBuildCost;
	
	// 전체 행 cached
	UPROPERTY(Transient) 
	FBuildingAppearance CachedDefinition;
	/**
	 * 건설이 완료되어 현재 건물에 실제 적용 중인 티어 에셋.
	 * Completed 및 Destroyed 상태에서 사용한다.
	 */
	UPROPERTY(Transient) // Advance
	FBuildingTierRuntimeAssets ActiveTierAssets;
	/**
	 * 최초 건설 또는 업그레이드 중인 대상 티어 에셋.
	 * Preview 및 UnderConstruction 상태에서 사용한다.
	 */
	UPROPERTY(Transient) // 일시적으로 가지고 있는 에셋(temp용)
	FBuildingTierRuntimeAssets PendingTierAssets;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta=(AllowPrivateAccess=true))
	EBuildingState CurrentBuildingState = EBuildingState::None;
private:
	/** 배치 컴포넌트가 겹침 검사에 사용하는 데이터 테이블의 GridSize. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	FIntPoint CachedGridSize = FIntPoint(1, 1);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BuildingPreviewMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OverlayMID;
	
	UPROPERTY()
	TObjectPtr<USoundBase> DestroyedSound;
	UPROPERTY()
	TObjectPtr<UNiagaraSystem> DestroyedEffect;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State", meta=(ExposeOnSpawn="true", AllowPrivateAccess=true))
	ESFBuildingSpawnMode SpawnMode = ESFBuildingSpawnMode::PlayerPlacedMode;
	/* 업그레이드 중에는 이전 티어를 유지하고,
	* 업그레이드 완료 시 새 티어로 교체한다.
	*/// 거의 한순간 1회용 캐싱용도
	int32 ActivatedGameplayTier = INDEX_NONE;
};
