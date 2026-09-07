#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Datas/Race/SFRaceType.h"

#include "SFBuildingAssetSubsystem.generated.h"

struct FStreamableHandle;
class USFBuildingCatalog;
class USFBuildingDefinition;
class UTexture2D;
/**
 * 한 종족의 Catalog와 Definition을 강하게 참조한다.
 *
 * 비동기 로드 핸들을 해제해도 이 구조체가 보관하는 UObject는
 * Subsystem이 비울 때까지 GC되지 않는다.
 */
USTRUCT()
struct FSFLoadedRaceBuildingData
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<USFBuildingCatalog> Catalog = nullptr;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USFBuildingDefinition>> Definitions;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> UIAssets;
    
    UPROPERTY(Transient)
    bool bReady = false;
};

DECLARE_DELEGATE_TwoParams(
    FSFOnRaceBuildingDataReady,
    ESFRace,   // 요청한 종족
    bool       // 성공 여부
);

DECLARE_DELEGATE_ThreeParams(
    FSFOnBuildingAssetsReady,
    USFBuildingDefinition*, // 준비된 Definition
    int32,                   // TierIndex
    bool                     // 성공 여부
);

UCLASS()
class STRIKEFORGE_API USFBuildingAssetSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Subsystem 종료 시 남은 로드와 캐시를 정리한다. GameInstance가 호출하며 ReleaseStageAssets 후 부모 종료로 이어진다. */
    virtual void Deinitialize() override;

    /** 종족 Catalog·Definition·UI 리소스를 비동기로 준비한다. GameMode에서 호출되며 HandleCatalogLoaded로 이어진다. */
    void RequestRaceDataAsync(ESFRace InRace, FSFOnRaceBuildingDataReady OnComplete);

    /** 특정 건물 Tier의 런타임 리소스를 비동기로 준비한다. GameMode·Controller·Building에서 호출되며 CompleteBuildingRequest로 이어진다. */
    void RequestBuildingAssetsAsync(
        USFBuildingDefinition* InDefinition,
        int32 InTierIndex,
        FSFOnBuildingAssetsReady OnComplete);
    
    /** 준비가 완료된 종족 Catalog를 반환한다. HandleRaceDataReady에서 호출되며 GameState의 팀 등록으로 이어진다. */
    USFBuildingCatalog* GetLoadedCatalog(ESFRace InRace) const;

    /** 종족 데이터 캐시가 사용 가능한 상태인지 검사한다. RequestRaceDataAsync에서 호출되며 즉시 완료 또는 신규 로드를 결정한다. */
    bool IsRaceDataReady(ESFRace InRace) const;

    /** 진행 중인 로드를 취소하고 스테이지 캐시를 해제한다. 레벨 이동과 Deinitialize에서 호출되며 모든 요청 상태를 초기화한다. */
    void ReleaseStageAssets();

private:
    /** Catalog 로드 완료를 처리한다. RequestRaceDataAsync의 완료 콜백으로 호출되며 Definition 로드로 이어진다. */
    void HandleCatalogLoaded(ESFRace InRace);

    /** Definition 로드 완료를 검증하고 UI 경로를 수집한다. HandleCatalogLoaded 이후 호출되며 UI 로드로 이어진다. */
    void HandleDefinitionsLoaded(ESFRace InRace);

    /** 종족 로드 결과를 확정하고 대기 중인 콜백을 실행한다. 각 종족 로드 단계에서 호출되며 GameMode의 HandleRaceDataReady로 이어진다. */
    void CompleteRaceRequest(ESFRace InRace, bool bSuccess);
    
    /** UI 아이콘 로드 완료를 검증하고 강한 참조로 보관한다. HandleDefinitionsLoaded 이후 호출되며 CompleteRaceRequest로 이어진다. */
    void HandleUIAssetsLoaded(ESFRace InRace);

    /** 건물 Tier 로드 결과를 확정하고 대기 중인 콜백을 실행한다. RequestAsyncLoad 완료 시 호출되며 요청자별 완료 함수로 이어진다. */
    void CompleteBuildingRequest(
        const FString& InRequestKey,
        USFBuildingDefinition* InDefinition,
        int32 InTierIndex,
        bool bSuccess);
private:
    /**
     * UObject를 GC로부터 보호하는 강한 참조 캐시.
     */
    UPROPERTY(Transient)
    TMap<ESFRace, FSFLoadedRaceBuildingData> LoadedRaceData;

    /**
     * 진행 중인 Catalog/Definition 로드 핸들.
     *
     * FStreamableHandle은 UObject가 아니므로 UPROPERTY를 붙이지 않는다.
     */
    TMap<ESFRace, TSharedPtr<FStreamableHandle>> ActiveRaceHandles;

    /**
     * 같은 종족에 대한 중복 요청의 완료 콜백.
     */
    TMap<ESFRace, TArray<FSFOnRaceBuildingDataReady>>
        PendingRaceCallbacks;

    /**
     * 건물의 메시, 클래스, 사운드 등을 유지하는 핸들.
     *
     * 초기 구현에서는 스테이지가 끝날 때 일괄 해제하는 것이 안전하다.
     */
    TMap<FString, TSharedPtr<FStreamableHandle>>
        ActiveBuildingHandles;
    
    TMap<FString, TArray<FSFOnBuildingAssetsReady>>
    PendingBuildingCallbacks;
};