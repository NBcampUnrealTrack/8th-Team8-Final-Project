#pragma once

#include "CoreMinimal.h"
#include "SFBuildingType.h"
#include "Engine/DataAsset.h"
#include "SFBuildingDefinition.generated.h"

class ASFBaseBuilding;
class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;
class UTexture2D;

struct FSFBuildingTierData;

UCLASS(Abstract, BlueprintType)
class STRIKEFORGE_API USFBuildingDefinition
    : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    static const FPrimaryAssetType BuildingAssetType;
    
    /** Asset Manager 스캔 시 Building Primary Asset ID를 만든다. 엔진에서 호출되며 등록·조회에 사용된다. */
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
    
    /** Definition의 Tier 개수를 반환한다. CacheLoadedDefinitionData에서 호출되며 각 Tier 조회로 이어진다. */
    virtual int32 GetTierCount() const;
    
    /** 지정한 Tier 데이터를 반환한다. 경로 수집과 건물 캐시 준비에서 호출되며 리소스 로드·적용으로 이어진다. */
    virtual const FSFBuildingTierData* FindTier(int32 TierIndex) const;

    // Async
    
    /** UI 아이콘의 Soft 경로를 수집한다. HandleDefinitionsLoaded에서 호출되며 UI 비동기 로드로 이어진다. */
    void GetUIAssetPaths(TArray<FSoftObjectPath>& OutPaths) const;

    /** 지정 Tier의 클래스·메시·효과·사운드 경로를 수집한다. RequestBuildingAssetsAsync에서 호출되며 런타임 비동기 로드로 이어진다. */
    bool GetRuntimeAssetPaths(int32 InTierIndex, TArray<FSoftObjectPath>& OutPaths) const;

    /** 지정 Tier의 필수 리소스가 모두 메모리에 있는지 검사한다. 비동기 완료 시 호출되며 CompleteBuildingRequest로 이어진다. */
    bool AreRuntimeAssetsLoaded(int32 InTierIndex) const;
public:
    // 전체 건물 Definition에서 고유해야 하는 ID. 종족을 포함한 이름을 권장한다. ex) HumanHouse, AnimalHouse
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,  AssetRegistrySearchable, Category = "Identity")
    FName BuildingId;
    
    // * UI에 표시되는 건물 이름.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;
    
    // 모든 종족의 건물 카테고리 분류
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    ESFBuildingType BuildingType;
    
    // * 건설 UI 등에 표시되는 건물 아이콘.
    UPROPERTY(
    EditDefaultsOnly,
    BlueprintReadOnly,
    Category = "UI",
    meta = (AssetBundles = "UI"))
    TSoftObjectPtr<UTexture2D> Icon;
    
    // * 현재 DT_UI와 연결하기 위한 임시 브리지. Unit 선택 UI가 DA로 전환될 때 함께 제거한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Legacy")
    FName SelectionUIRowName;
    
    // * 이 Definition으로 생성할 실제 건물 Actor 클래스.
    UPROPERTY(
    EditDefaultsOnly,
    BlueprintReadOnly,
    Category = "Spawn",
    meta = (AssetBundles = "Spawn"))
    TSoftClassPtr<ASFBaseBuilding> BuildingClass;
    
    // * 건물이 점유하는 Grid 크기.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "1"))
    FIntPoint GridSize = FIntPoint(1, 1);
    
    // 건물 배치 Preview에서 사용할 Material.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement")
    TSoftObjectPtr<UMaterialInterface> PreviewMaterial;
    
    // 추후 티어별 해금이 있다면 사용할 멤버변수
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Requirement", meta = (ClampMin = "1"))
    int32 RequiredNexusTier = 1;
    
    // 건설중 상태 sound
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Selection")
    TSoftObjectPtr<USoundBase> UnderConstructionSelectionSound;

    // 건설완료 상태 sound
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Selection")
    TSoftObjectPtr<USoundBase> CompletedSelectionSound;
    
    // * 건물 파괴 시 재생할 공통 사운드.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    TSoftObjectPtr<USoundBase> DestroyedSound;
    
    // * 건물 건설 시 재생할 공통 Niagara Effect.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect|Niagara")
    TSoftObjectPtr<UNiagaraSystem> ConstructEffect;
    
    // * 건물 파괴 시 재생할 공통 Niagara Effect.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect|Niagara")
    TSoftObjectPtr<UNiagaraSystem> DestroyEffect;
    
    /** 손상 단계에서 파단면에 붙일 불꽃 Effect.* Infinite 루프 시스템이어야 한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect|Niagara")
    TSoftObjectPtr<UNiagaraSystem> DamagedBurningEffect;
};