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

	/** 배지 기준 건물(처음 지은 건물 또는 마지막으로 클릭한 건물)의 XY와 클러스터에서 가장 높은 건물 상단을 사용한 배지 기준점입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	FVector BadgeWorldLocation = FVector::ZeroVector;
};

/** 생산 버프 판정과 격자 데칼 표시가 사용하는 원천 건물별 효과 구역입니다. */
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

	/** 효과 구역의 월드 중심입니다. Z는 원천 건물 높이이며 격자 데칼의 투영 높이에 사용합니다. */
	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	FVector WorldCenter = FVector::ZeroVector;

	/** 연결된 클러스터에 포함된 전체 원천 건물 수입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	int32 SourceBuildingCount = 0;

	/**
	 * 같은 재계산 결과 안에서 이 구역이 속한 클러스터 번호입니다.
	 * 재계산마다 다시 매겨지므로 같은 조회 결과 안에서 구역을 묶을 때만 사용합니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	int32 ClusterIndex = INDEX_NONE;

	/** 이 구역의 중심이 되는 원천 건물입니다. 선택한 건물이 속한 클러스터를 찾을 때 사용합니다. */
	UPROPERTY(BlueprintReadOnly, Category = "SF|Building Synergy")
	TWeakObjectPtr<ASFBaseBuilding> SourceBuilding;
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

	/** 건물을 시너지 계산 대상 목록에 추가하고, 완성 건물이면 재계산합니다(Batch 구간에서는 표시만). */
	void RegisterBuilding(ASFBaseBuilding* Building);
	/** 제거되는 건물과 적용 기록을 정리하고, 활성 건물이었다면 재계산합니다(Batch 구간에서는 표시만). */
	void UnregisterBuilding(ASFBaseBuilding* Building);
	/** 건설 완료·비활성화처럼 건물 상태가 바뀌었을 때 재계산합니다(Batch 구간에서는 표시만). */
	void NotifyBuildingChanged(ASFBaseBuilding* Building);

	/** 여러 건물을 한꺼번에 등록·변경하는 구간을 시작합니다. 구간 안에서는 재계산하지 않고 표시만 남깁니다. */
	void BeginUpdateBatch();
	/** 가장 바깥 구간이 끝나면 쌓인 변경을 한 번에 재계산합니다. BeginUpdateBatch와 반드시 짝을 이룹니다. */
	void EndUpdateBatch();

	/** 지정한 팀이 현재 보유한 계산 완료 시너지 구역만 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "SF|Building Synergy")
	TArray<FSFBuildingSynergyZoneInfo> GetSynergyZonesForTeam(int32 TeamIndex) const;
	/** 지정한 팀의 비활성 단계를 포함한 클러스터 진행 정보만 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "SF|Building Synergy")
	TArray<FSFBuildingSynergyClusterProgressInfo> GetClusterProgressInfosForTeam(
		int32 TeamIndex) const;

	/**
	 * 지정한 팀의 활성 원천 건물마다 클러스터 연결 판정에 쓰는 링크 격자를 반환합니다.
	 * 다른 원천 건물의 중심이 이 격자 안에 들어오면 두 건물이 같은 클러스터로 연결됩니다.
	 */
	void CollectClusterLinkGridInfos(
		int32 TeamIndex,
		const USFBuildingSynergyDefinition* SynergyDefinition,
		TArray<FBuildingGridInfo>& OutLinkGridInfos) const;

	/** 해당 건물에 HighestOnly로 적용 중인 가장 높은 배치 시너지 증가율을 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "SF|Building Synergy")
	float GetProductionBonusPercent(const ASFBaseBuilding* Building) const;

	/** 선택한 시너지 원천 건물을 소속 클러스터 배지의 기준 건물로 지정하고, 전체 재계산 없이 배지 X·Y만 옮깁니다. */
	void SelectBadgeAnchorBuilding(ASFBaseBuilding* Building);

	/** 구역과 클러스터 진행 정보가 모두 갱신됐음을 UI와 격자 데칼 표시 계층에 알립니다. */
	UPROPERTY(BlueprintAssignable, Category = "SF|Building Synergy")
	FSFOnBuildingSynergyStateChanged OnSynergyStateChanged;

	/** 월드 종료 시 적용했던 생산 배율과 임시 구역·건물 기록을 모두 정리합니다. */
	virtual void Deinitialize() override;

private:
	/** 시너지 결과가 바뀔 수 있음을 기록하고, Batch 구간이 아니면 즉시 FlushDirtySynergies를 실행합니다. */
	void MarkSynergiesDirty();
	/** 기록된 변경이 있을 때만 RecalculateSynergies를 한 번 실행합니다. */
	void FlushDirtySynergies();
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

	/** 이벤트 기반 클러스터 재계산 때 다시 만들고(배지 X·Y는 건물 클릭 때도 갱신), UI가 Definition과 함께 진행 상태를 계산합니다. */
	UPROPERTY(Transient)
	TArray<FSFBuildingSynergyClusterProgressInfo> ClusterProgressInfos;

	/**
	 * 마지막 재계산에서 원천 건물이 속한 ClusterProgressInfos 번호입니다. 시너지를 여러 개 주는 건물은 번호도 여러 개입니다.
	 * 건물 클릭 시 전체 재계산 없이 소속 클러스터 배지 위치만 고칠 때 사용합니다.
	 */
	TMap<TWeakObjectPtr<ASFBaseBuilding>, TArray<int32>> BuildingClusterIndices;

	/**
	 * 클러스터 배지를 띄울 기준 건물과 기록 순번입니다. 클러스터마다 순번이 가장 큰 건물 위에 배지가 뜹니다.
	 * 새 클러스터의 첫 건물이 기록되고, 건물을 클릭하면 더 큰 순번으로 다시 기록됩니다.
	 */
	TMap<TWeakObjectPtr<ASFBaseBuilding>, int32> BadgeAnchorStamps;
	int32 NextBadgeAnchorStamp = 0;

	bool bIsRecalculating = false;

	/** BeginUpdateBatch 중첩 깊이입니다. 0보다 크면 변경을 표시만 하고 재계산을 미룹니다. */
	int32 UpdateBatchDepth = 0;
	/** 마지막 재계산 이후 시너지 결과가 바뀔 수 있는 변경이 있었는지 나타냅니다. */
	bool bSynergyDirty = false;
};
