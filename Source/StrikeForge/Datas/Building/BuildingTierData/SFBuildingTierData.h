#pragma once

#include "CoreMinimal.h"
#include "Datas/Building/BuildingAppearance.h"
#include "SFBuildingTierData.generated.h"

/**
 * 모든 건물 Definition이 공통으로 사용하는 Tier 설정.
 *
 * 정적 설정만 보관하며 현재 체력, 진행도, CurrentTier 같은
 * 런타임 상태는 포함하지 않는다.
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFBuildingTierData
{
    GENERATED_BODY()

    /* 건설 작업량과 건설·업그레이드 비용. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier|Construction")
    FBuildingConstructionData Construction;

    /* 최대 체력 등 공통 능력치. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier|Stats")
    FBuildingStatData Stats;

    /* 건설·완성·손상·파괴 메시. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier|Visual")
    FBuildingVisualData Visual;

    /* 건설 시작·완료 사운드. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier|Audio")
    FBuildingAudioData Audio;
};

/**
 * House에서 사용하는 Tier 설정. ** 상위 구조체를 상속받고 있다 **
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFHouseBuildingTierData
    : public FSFBuildingTierData
{
    GENERATED_BODY()

    /* 해당 Tier가 완성됐을 때 제공하는 최대 인구. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier|House", meta = (ClampMin = "0"))
    int32 PopulationCapacity = 0;
};

/**
 * Mining에서 사용하는 Tier 설정.
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFMiningBuildingTierData
    : public FSFBuildingTierData
{
    GENERATED_BODY()

    /** 초당 누적되는 기본 채굴 작업량. 값이 클수록 채굴 속도가 빨라진다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier|Mining", meta = (ClampMin = "0.1"))
    float ExtractBaseWorkAmount = 10.f;

    /** 채굴 작업 1회가 완료됐을 때 획득하는 자원량. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier|Mining",  meta = (ClampMin = "1"))
    int32 ExtractAmount = 5;
};

/**
 * Production에서 사용하는 Tier 설정.
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFProductionBuildingTierData
    : public FSFBuildingTierData
{
    GENERATED_BODY()

    /**
     * 해당 Tier에서 생산 가능한 유닛의 RowName 목록.
     *
     * 유닛 데이터는 현재 DT를 유지하므로 FName 브리지를 사용한다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier|Production")
    TArray<FName> SpawnableUnits;
};