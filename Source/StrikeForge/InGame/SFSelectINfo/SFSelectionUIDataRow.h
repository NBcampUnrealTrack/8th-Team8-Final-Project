#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SFSelectionUIDataRow.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFSelectionUIDataRow
    : public FTableRowBase
{
    GENERATED_BODY()

    // 변수 영역
public:
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI"
    )
    FText DisplayName;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI"
    )
    TSoftObjectPtr<UTexture2D> PortraitImage;

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
        meta = (
            MultiLine = true
            )
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