// SFStageData.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Datas/Race/SFRaceType.h"
#include "SFStageData.generated.h"

UENUM(BlueprintType)
enum class ESFStageProgressState : uint8
{
    Locked      UMETA(DisplayName = "Locked"),
    Unlocked    UMETA(DisplayName = "Unlocked"),
    Cleared     UMETA(DisplayName = "Cleared")
};

/**
 * 한 스테이지에 참여하는 하나의 **적팀** 설정.
 *
 * 종족으로부터 Catalog를 조회하므로
 * BuildingCatalog를 이 구조체에 직접 저장하지 않는다.
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFStageTeamSetup
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|Team", meta = (ClampMin = "1", UIMin = "1"))
    int32 TeamIndex = 1;

    /** 해당 AI 팀이 사용할 종족 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|Team")
    ESFRace Race = ESFRace::Human;
};

class UWorld;
class UTexture2D;

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFMinimapSettings
{
    GENERATED_BODY()

public:
    // NavMeshBoundsVolume에서 범위를 자동으로 구한다.
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Stage|Minimap"
    )
    bool bAutoDetectBounds = true;

    // 자동 탐색 실패 또는 특수맵에서 사용할 범위
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Stage|Minimap"
    )
    FVector2D WorldMin =
        FVector2D(-5000.0f, -5000.0f);

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Stage|Minimap"
    )
    FVector2D WorldMax =
        FVector2D(5000.0f, 5000.0f);

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Stage|Minimap",
        meta = (ClampMin = "1000.0")
    )
    float CaptureHeight = 10000.0f;

    // Rotate terrain, fog, markers and pointer coordinates together in 90-degree steps.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|Minimap",
        meta = (ClampMin = "0", ClampMax = "3", UIMin = "0", UIMax = "3"))
    int32 ClockwiseQuarterTurns = 0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Stage|Minimap",
        meta = (ClampMin = "0.0")
    )
    float BoundsPadding = 200.0f;
};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFStageBuildingRule
{
    GENERATED_BODY()

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Stage|Building"
    )
    FName BuildingId = NAME_None;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Stage|Building"
    )
    bool bCanBuild = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Stage|Building",
        meta = (ClampMin = "1", ClampMax = "3", UIMin = "1", UIMax = "3")
    )
    int32 MaxAllowedTier = 3;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Stage|Building"
    )
    TArray<FName> RequiredBuildingIds;
};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFStageData : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage")
    int32 StageOrder = 1;

    // 로비 UI에서 표시할 스테이지 이름
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage")
    FText DisplayName;

    // 스테이지 시작 시 이동할 레벨
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage")
    TSoftObjectPtr<UWorld> StageLevel;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Stage|Minimap"
    )
    FSFMinimapSettings Minimap;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Stage|Building"
    )
    TArray<FSFStageBuildingRule> BuildingRules;

    /**
     * 스테이지가 시작되는 순간 플레이어 팀에 지급할 광물.
     *
     * GameMode의 하드코딩/행별 예외가 아니라 DT_SFStage 각 행에서 조절한다.
     * 0도 유효한 값이므로, 자원을 주지 않는 특수 스테이지도 만들 수 있다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|Resource", meta = (ClampMin = "0"))
    int32 StartingMineral = 205;

    /** 스테이지 시작 시 플레이어 팀에 지급할 에테르. 0도 유효하다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|Resource", meta = (ClampMin = "0"))
    int32 StartingEther = 100;

    /* 스테이지 진입 시 플레이어에게 기본 적용할 종족. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|Team")
    ESFRace PlayerRace = ESFRace::None;

    /** Two fixed bases: authored team 0 is Etherian, team 1 is Beast.
     * The selected faction is remapped to player team 0 before actors begin play. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|Team")
    bool bUseFactionBases = false;
    
    // 스테이지에 참여하는 AI 팀 설정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|Team")
    TArray<FSFStageTeamSetup> TeamSetups;
    
    // 스테이지 잠금 이미지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|UI")
    TSoftObjectPtr<UTexture2D> LockedImage;

    // 스테이지 풀림 이미지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|UI")
    TSoftObjectPtr<UTexture2D> UnlockedImage;

    // 스테이지 클리어 이미지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|UI")
    TSoftObjectPtr<UTexture2D> ClearedImage;
};
