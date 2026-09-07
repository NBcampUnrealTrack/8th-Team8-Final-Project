#pragma once

#include "CoreMinimal.h"
#include "Buildings/SFRootBase.h"
#include "Game/Subsystem/BuildingGridInfo.h"
#include "Datas/Building/BuildingTierData/SFBuildingTierData.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Units/Damagable.h"
#include "SFBaseBuilding.generated.h"

class UPlayVFXComponent;
class UTimelineComponent;
class UProgressComponent;
class UHealthComponent;
class UCombatTargetComponent;
class USFBuffComponent;
class UNiagaraSystem;
class UWidgetComponent;

class USFBuildingDefinition;
// gc
class UGeometryCollection;
class ASFBuildingDestructionActor;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSFOnBuildingTierActivated,
	int32,
	ActivatedTier
);

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
	// gc
	UPROPERTY()
	TObjectPtr<UGeometryCollection> DestructionCollection = nullptr;
	
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
		DestructionCollection = nullptr; // gc
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
	// 생성될때 BaseBuilding의 StaticComponent크기를 그대로 복사해서 스폰됨.
	ASFBaseBuilding(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void TakeDamaged_Implementation(const FSFDamageInfo& DamageInfo) override;
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	virtual const FHealthData GetHealthData_Implementation() override;
	virtual USoundBase* GetSelectionSound_Implementation() override;
	/** 프리뷰 색상 갱신 */
	void ApplyPreviewMaterial(bool bCanPlace);
	
	// 커맨더 건설 시작 시작함수
	UFUNCTION(BlueprintCallable, Category = "SF|Building")
	bool TryStartConstruction();
	
	// 업그레이드 함수
	/** 다음 Tier 리소스를 비동기로 요청한다. 업그레이드 UI나 Blueprint에서 호출되며 HandleUpgradeAssetsReady로 이어진다. */
	UFUNCTION(BlueprintCallable, Category = "SF|Building")
	void UpgradeBuilding(int32 UpgradeValue = 1);
	
	// React
	void PlayHitReaction(const FVector& LocalDirection);
	
	// Grid System
	
	bool TryRegisterGrid(const FBuildingGridInfo& GridInfo);
	//FIntPoint GetEffectiveGridSize() const;
	
	// Getter
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Building")
	FORCEINLINE int32 GetCurrentTier() const { return CurrentTier; }
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Building")
	FResources GetConstructCost() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Building")
	FResources GetUpgradeCost() const;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Building")
	FORCEINLINE FIntPoint GetGridSize() const { return ConfiguredGridSize; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Building")
	FORCEINLINE ESFBuildingSpawnMode GetSpawnMode() const { return SpawnMode; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Building")
	FBoxSphereBounds GetBuildBounds() const;
	
	UFUNCTION(BlueprintPure, Category = "SF|Building|Definition")
	FORCEINLINE USFBuildingDefinition* GetBuildingDefinition() const { return BuildingDefinition; }
	
	FORCEINLINE bool IsTierActivated() const { return bIsTierActivated; }
	
	// setter
	UFUNCTION(BlueprintCallable, BlueprintSetter, Category = "SF|Building")
	FORCEINLINE void SetSpawnMode(ESFBuildingSpawnMode InSpawnType) { SpawnMode = InSpawnType; }

	void SetMyTeamIndex(const int32 InTeamIndex);
	
	bool SetBuildingDefinition(USFBuildingDefinition* InDefinition);

	UPROPERTY(BlueprintAssignable,Category = "SF|Building")
	FSFOnBuildingTierActivated OnBuildingTierActivated;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void OnBuildingDefinitionLoaded() {}
	// APPLY BUILDING's ABILITY
	/**
	 * CurrentTier가 확정된 뒤 호출된다.
	 *
	 * 파생 건물은 GetBuildingDefinition()과 GetCurrentTier()를 사용해
	 * 자신의 전용 Tier 데이터를 조회한다.
	 */
	virtual void OnTierActivated() {}
	// PRODUCT STOP
	virtual void OnUpgradeStarted() {}
	// DELETE BUILDING's ABILITY
	// 건물이 부숴질때 호출
	/**
	 * 현재 적용된 건물 기능을 제거할 때 호출된다.
	 *
	 * 파생 건물은 실제로 적용해 둔 런타임 값을 회수해야 한다.
	 */
	virtual void OnTierDeactivated() {}
	// when construct destroyed, play effects
	// 
	virtual void OnBuildingDestroyed() {}
	
	virtual void NotifyActorBeginCursorOver() override;
	virtual void NotifyActorEndCursorOver() override;
	
	const FSFBuildingTierData* GetTierInfo(int32 TierIndex) const;
	const FSFBuildingTierData* GetCurrentTierInfo() const;
	// React Delegate Function
	UFUNCTION()
	void UpdateHitReaction(float CurveValue);
	UFUNCTION()
	void FinishHitReaction();
	
	// grid system
	bool TryRegistInitialGrid();
	bool bGridRegistered = false;
private:
	/** 로드된 Definition의 공통 데이터를 건물에 캐시한다. BeginPlay에서 호출되며 PrepareLoadedTier로 이어진다. */
	bool CacheLoadedDefinitionData();
	/** 로드된 Tier 리소스를 Pending 캐시에 준비한다. BeginPlay와 업그레이드 완료에서 호출되며 건설 상태 적용으로 이어진다. */
	bool PrepareLoadedTier(int32 TierIndex);
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
	void PlayDestroyEffect();
	void PlayConstructionCompleteSound();
	
	// delegate
	void BindComponentDelegates();
	void UnbindComponentDelegates();

	// progress가 완료되면 호출되게 bind되있음
	// Handle -> Finalize -> Committed(pre값->cur값) -> Set(completed)State -> Activate
	UFUNCTION() 
	void HandleCompleteConstruction();
	
	// 데미지 적용조건 함수
	bool CanReceiveDamage() const;
	/** 체력 변경을 완료 건물의 손상 외형에 반영한다. */
	UFUNCTION() // BIND with healthcomponent.applydamage
	void HandleHealthChanged(float InBeforeHP, float InCurrentHP, float InMaxHP);
	/** 체력 비율에 맞춰 GC 또는 DamageMesh 단계를 갱신한다. */
	void UpdateDamageVisual(float PreviousHealthRatio, float CurrentHealthRatio);
	UFUNCTION()
	void HandleHealthZero();
	// 박스 콜리젼 셋Set
	void SetBoxExtent();

	// 메쉬 변경 함수
	UFUNCTION()
	void HandleConstructionProgress(float PreviousRatio, float CurrentRatio);
	void RefreshVisual();
	UStaticMesh* ResolveMesh() const;
	UStaticMesh* ResolveDamageMesh(const FBuildingTierRuntimeAssets& Assets) const;

	/** GC 액터가 없으면 생성하고 기존 StaticMesh를 숨긴다. */
	bool EnsureDestructionActor();

	/** 기존 GC를 제거하고 StaticMesh를 다시 표시한다. */
	void ClearDestructionVisual();
	
	/** 건설 준비 중이면 PendingTier, 아니면 CurrentTier의 데이터를 반환한다. */
	const FSFBuildingTierData* GetCostTierInfo() const;
	
	/** 완성 메시의 바닥 영역을 기준으로 단계별 불꽃을 생성한다. */
	void PlayBurningEffect(int32 DamageStageIndex);
	
	// async
	/** 업그레이드 리소스 로드 결과를 검증하고 비용을 차감한다. Subsystem 콜백으로 호출되며 PrepareLoadedTier와 BeginConstruction으로 이어진다. */
	void HandleUpgradeAssetsReady(
	USFBuildingDefinition* InDefinition,
	int32 InTierIndex,
	bool bSuccess);

	int32 RequestedUpgradeTier = INDEX_NONE;
protected:
	// components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component|Health")
	TObjectPtr<UHealthComponent> HealthComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component|Widget")
	TObjectPtr<UWidgetComponent> WidgetComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component|Mesh")
	TObjectPtr<UProgressComponent> ProgressComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Component|Combat")
	TObjectPtr<UCombatTargetComponent> CombatTargetComponent;
	// etc.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component|React")
	TObjectPtr<UTimelineComponent> HitReactionTimeline;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component|Niagara")
	TObjectPtr<UPlayVFXComponent> PlayVFXComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Component|Buff")
	TObjectPtr<USFBuffComponent> BuffComponent;
	
	// ui
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	FText DisplayName;
	// 적용전과 후 tier 인덱스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Information", meta=(ExposeOnSpawn="true",AllowPrivateAccess=true))
	int32 CurrentTier = 0;
	int32 PendingTier = INDEX_NONE;
	int32 CurrentStageIndex = 0;
	// 이번 건설에서 증가시킬 전체 체력
	float ConstructionCurrentHealthIncrease  = 0.f;

	/**
	 * Definition의 공통 Tier 부분을 복사한 런타임 캐시.
	 *
	 * 공유 DA를 변경하지 않고 건설·손상 단계를 정렬하기 위해 사용한다.
	 * House/Mining/Production 전용 필드는 이곳에 복사하지 않는다.
	 */
	UPROPERTY(Transient)
	TArray<FSFBuildingTierData> CachedCommonTiers;
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
#pragma region React
protected:
	UPROPERTY(EditDefaultsOnly, Category = "HitReact")
	TObjectPtr<UCurveFloat> HitReactionCurve;

	UPROPERTY(EditDefaultsOnly, Category = "HitReact")
	float HitMoveDistance = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "HitReact")
	float HitRotationAmount = 2.0f;

	/** 이번 HitReact가 움직일 실제 시각 컴포넌트 */
	TWeakObjectPtr<USceneComponent> ActiveHitReactionTarget;

	/** HitReact 시작 전 대상의 World Transform */
	FTransform OriginalHitReactionTransform =
		FTransform::Identity;

	/** 현재 화면에 표시되는 컴포넌트를 반환한다. */
	USceneComponent* ResolveHitReactionTarget() const;

	/** 진행 중인 HitReact를 중단하고 대상을 원위치로 복구한다. */
	void StopHitReaction();
	
	FVector HitLocalDirection = FVector::ForwardVector;

#pragma endregion 
	/** 배치 컴포넌트가 겹침 검사에 사용하는 데이터 테이블의 GridSize. */
	UPROPERTY()
	FIntPoint ConfiguredGridSize = FIntPoint(1, 1);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	FBuildingGridInfo RegisteredGridInfo;
	
	// preview ghost
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BuildingPreviewMaterialInstance;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OverlayMID;
	
	// sound
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> UnderConstructionSelectionSound = nullptr;
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CompletedSelectionSound = nullptr;
	UPROPERTY()
	TObjectPtr<USoundBase> DestroyedSound;
	// niagara
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> ConstructionEffect;
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> DestroyedEffect;
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> DamagedBurningEffect;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State", meta=(ExposeOnSpawn="true", AllowPrivateAccess=true))
	ESFBuildingSpawnMode SpawnMode = ESFBuildingSpawnMode::PlayerPlacedMode;

	// 티어 기능이 현재 적용되어 있는지 나타낸다.
	bool bIsTierActivated = false;
private:
	// 빌딩DA
	UPROPERTY(EditInstanceOnly,BlueprintReadOnly,Category = "SF|Building|Definition",meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USFBuildingDefinition> BuildingDefinition;
	
	// gc
	UPROPERTY(Transient)
	TObjectPtr<ASFBuildingDestructionActor> DestructionActor;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction",
	meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ASFBuildingDestructionActor> DestructionActorClass;
};
