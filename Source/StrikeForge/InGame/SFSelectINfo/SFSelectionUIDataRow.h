#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Datas/Race/SFRaceType.h"
#include "SFSelectionUIDataRow.generated.h"

class UTexture2D;


/**
 * 종족·티어별 초상화
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFSelectionPortraitVariant
{
    GENERATED_BODY()

public:
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI"
    )
    ESFRace Race = ESFRace::None;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI",
        meta = (ClampMin = "1")
    )
    int32 Tier = 1;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI"
    )
    TSoftObjectPtr<UTexture2D> PortraitImage;
};


/**
 * 선택 UI 데이터 테이블 행
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFSelectionUIDataRow
    : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI"
    )
    FText DisplayName;

    // 기존 기본 이미지. variants가 없을 때 사용
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI"
    )
    TSoftObjectPtr<UTexture2D> PortraitImage;

    // 종족·티어별 초상화
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI"
    )
    TArray<FSFSelectionPortraitVariant> PortraitVariants;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI"
    )
    TSoftObjectPtr<UTexture2D> SelectionIcon;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI",
        meta = (MultiLine = true)
    )
    FText Description;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI|Visibility"
    )
    bool bShowHealth = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI|Visibility"
    )
    bool bShowAttackPower = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI|Visibility"
    )
    bool bShowMoveSpeed = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI|Visibility"
    )
    bool bShowSkillIcons = false;
};