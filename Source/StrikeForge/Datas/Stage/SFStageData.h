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
    
    /* 스테이지 진입 시 플레이어에게 기본 적용할 종족. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|Team")
    ESFRace PlayerRace = ESFRace::None;
    
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