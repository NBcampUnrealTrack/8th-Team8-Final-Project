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

DECLARE_DELEGATE_TwoParams(FSFOnRaceBuildingDataReady,
    ESFRace,   // 요청한 종족
    bool       // 성공 여부
);

DECLARE_DELEGATE_ThreeParams(FSFOnBuildingAssetsReady, USFBuildingDefinition*, // 준비된 Definition
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
    // 종족 Catalog용 비동기 실행 함수 -> 비동기완료시 HandleCatalogLoaded
    // HandleCatalogLoaded에서 건물모두 비동기로딩 실행 -> 비동기 완료시 HandleDefinitionsLoaded
    // 안에서 또 UI용 이미지 비동기 로딩 -> 이미지 비동기 완료시 그제서야 true 브로드캐스트
    // 각 건물안의 soft에셋은 로딩 x
    void RequestRaceDataAsync(ESFRace InRace, FSFOnRaceBuildingDataReady OnComplete);
    
    // 여기서 각 건물안 tier의 soft에셋 로딩 o
    /** 특정 건물 Tier의 런타임 리소스를 비동기로 준비한다. GameMode·Controller·Building에서 호출되며 CompleteBuildingRequest로 이어진다. */
    // 건물 Definition용 비동기 실행 함수
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
    // def 내용을 확인하고 있으면 true
    void CompleteRaceRequest(ESFRace InRace, bool bSuccess);
    
    /** UI 아이콘 로드 완료를 검증하고 강한 참조로 보관한다. HandleDefinitionsLoaded 이후 호출되며 CompleteRaceRequest로 이어진다. */
    void HandleUIAssetsLoaded(ESFRace InRace);

    /** 건물 Tier 로드 결과를 확정하고 대기 중인 콜백을 실행한다. RequestAsyncLoad 완료 시 호출되며 요청자별 완료 함수로 이어진다. */
    // 현재 건물정의와 티어숫자, 정의와 티어숫자를 결합한 key로 콜백함수를 조회
    // 배열을 돌면서 콜백함수 execute 실행. > 로딩을 기다리고있던 바인딩함수들이 실행
    void CompleteBuildingRequest(
        const FString& InRequestKey,
        USFBuildingDefinition* InDefinition,
        int32 InTierIndex,
        bool bSuccess);
private:
    
    // UObject를 GC로부터 보호하는 강한 참조 캐시.
    // Catalog 비동기로딩시 저장 TMap
    UPROPERTY(Transient)
    TMap<ESFRace, FSFLoadedRaceBuildingData> LoadedRaceData;
    /**
     * 진행 중인 Catalog/Definition 로드 핸들.
     *
     * FStreamableHandle은 UObject가 아니므로 UPROPERTY를 붙이지 않는다.
     */
    // 현재 로딩중
    // Catalog 비동기로딩시 저장 TMap
    TMap<ESFRace, TSharedPtr<FStreamableHandle>> ActiveRaceHandles;

    /**
     * 같은 종족에 대한 중복 요청의 완료 콜백.
     */
    // Catalog 비동기로딩시 저장 TMap
    TMap<ESFRace, TArray<FSFOnRaceBuildingDataReady>> PendingRaceCallbacks;

    /**
     * 건물의 메시, 클래스, 사운드 등을 유지하는 핸들.
     *
     * 초기 구현에서는 스테이지가 끝날 때 일괄 해제하는 것이 안전하다.
     */
    // Definition 비동기 로딩시 저장 TMap
    // DA이름+티어숫자합친 FString, Handle은??
    // 비동기요청을하고 return되는 handle을 저장
    // 현재종족의 티어의 건물이 로딩중 상태임을 handle로 가지고있음
    TMap<FString, TSharedPtr<FStreamableHandle>> ActiveBuildingHandles;
    
    // 델리게이트로 되어있지만 호출하는쪽에서 함수랑 연결된 델리게이트를 인풋으로 넣는다
    // 조건에 맞다면 execute할거고 바인딩된 함수가 실행.
    TMap<FString, TArray<FSFOnBuildingAssetsReady>> PendingBuildingCallbacks;
};