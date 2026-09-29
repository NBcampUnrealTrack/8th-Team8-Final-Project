#pragma once

#include "CoreMinimal.h"
#include "Datas/Building/Synergy/SFBuildingSynergyTypes.h"
#include "Game/Subsystem/BuildingGridInfo.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFBuildingSynergySubsystem.generated.h"

class ASFBaseBuilding;
class USFBuildingSynergyDefinition;

/** 아직 활성화되지 않은 1개짜리 구역까지 포함하는 클러스터 UI 진행 정보입니다. */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFBuildingSynergyClusterProgressInfo
{
	GENERATED_BODY()

	/** 현재 단계·다음 요구 수·아이콘을 필요할 때 조회하는 단일 원본입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	TObjectPtr<USFBuildingSynergyDefinition> Definition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	int32 TeamIndex = INDEX_NONE;

	/** 연결된 클러스터에 포함된 전체 원천 건물 수입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	int32 SourceBuildingCount = 0;

	/** 합쳐진 점유 격자의 XY 중앙과 가장 높은 건물 상단을 사용한 배지 기준점입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	FVector BadgeWorldLocation = FVector::ZeroVector;
};

/** 생산 버프 판정과 Niagara 경계가 사용하는 원천 건물별 효과 구역입니다. */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFBuildingSynergyZoneInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	TObjectPtr<USFBuildingSynergyDefinition> Definition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	int32 TeamIndex = INDEX_NONE;

	/** 실제 버프 판정과 경계 표시가 함께 사용하는 단일 효과 구역입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	FBuildingGridInfo EffectGridInfo;

	/** 효과 구역의 월드 중심으로, Niagara 경계의 기준 위치입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	FVector WorldCenter = FVector::ZeroVector;

	/** 효과 구역의 월드 크기로, Niagara 경계 크기 계산에 사용합니다. */
	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	FVector2D WorldSize = FVector2D::ZeroVector;

	/** 연결된 클러스터에 포함된 전체 원천 건물 수입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	int32 SourceBuildingCount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFOnBuildingSynergyStateChanged);

/**
 * 건물 Definition이 제공하는 DataAsset 규칙을 읽어 배치 시너지를 계산합니다.
 * Tick 없이 건물 완성·업그레이드·파괴 이벤트에서만 다시 계산합니다.
 */
UCLASS()
class STRIKEFORGE_API USFBuildingSynergySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** WorldContext가 속한 월드의 시너지 Subsystem을 안전하게 반환합니다. */
	static USFBuildingSynergySubsystem* Get(const UObject* WorldContext);

	/** 건물을 시너지 계산 대상 목록에 추가하고 현재 구역을 즉시 다시 계산합니다. */
	void RegisterBuilding(ASFBaseBuilding* Building);
	/** 제거되는 건물과 적용 기록을 정리하고 남은 건물 기준으로 다시 계산합니다. */
	void UnregisterBuilding(ASFBaseBuilding* Building);
	/** 건설 완료·비활성화처럼 건물 상태가 바뀌었을 때 전체 시너지를 갱신합니다. */
	void NotifyBuildingChanged(ASFBaseBuilding* Building);

	/** 지정한 팀이 현재 보유한 계산 완료 시너지 구역만 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "SF|Building Synergy")
	TArray<FSFBuildingSynergyZoneInfo> GetSynergyZonesForTeam(int32 TeamIndex) const;
	/** 지정한 팀의 비활성 단계를 포함한 클러스터 진행 정보만 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "SF|Building Synergy")
	TArray<FSFBuildingSynergyClusterProgressInfo> GetClusterProgressInfosForTeam(
		int32 TeamIndex) const;

	/** 해당 건물에 HighestOnly로 적용 중인 가장 높은 배치 시너지 증가율을 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "SF|Building Synergy")
	float GetProductionBonusPercent(const ASFBaseBuilding* Building) const;

	/** 구역과 클러스터 진행 정보가 모두 갱신됐음을 UI와 Niagara 표현 계층에 알립니다. */
	UPROPERTY(BlueprintAssignable, Category = "SF|Building Synergy")
	FSFOnBuildingSynergyStateChanged OnSynergyStateChanged;

	/** 월드 종료 시 적용했던 생산 배율과 임시 구역·건물 기록을 모두 정리합니다. */
	virtual void Deinitialize() override;

private:
	/** 활성 원천 건물을 클러스터링하고 구역 생성·대상 판정·배율 적용을 한 번에 갱신합니다. */
	void RecalculateSynergies();
	/** 생산속도 인터페이스를 통해 대상 건물에 BuildingSynergy 배율을 등록합니다. */
	static void ApplyProductionModifier(
		ASFBaseBuilding* Building,
		float Multiplier);
	/** 대상 건물에서 BuildingSynergy 배율만 제거하여 다른 출처의 버프는 유지합니다. */
	static void RemoveProductionModifier(ASFBaseBuilding* Building);

	TSet<TWeakObjectPtr<ASFBaseBuilding>> RegisteredBuildings;
	TMap<TWeakObjectPtr<ASFBaseBuilding>, float> AppliedMultipliers;

	UPROPERTY(Transient)
	TArray<FSFBuildingSynergyZoneInfo> SynergyZones;

	/** 이벤트 기반 클러스터 재계산 때만 갱신하며, UI가 Definition과 함께 진행 상태를 계산합니다. */
	UPROPERTY(Transient)
	TArray<FSFBuildingSynergyClusterProgressInfo> ClusterProgressInfos;

	bool bIsRecalculating = false;
};
