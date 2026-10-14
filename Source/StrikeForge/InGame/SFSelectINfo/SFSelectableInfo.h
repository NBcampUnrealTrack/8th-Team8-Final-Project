#pragma once

#include "CoreMinimal.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "SFSelectableInfo.generated.h"

class UAnimationAsset;
class USFBuffComponent;
class UTexture2D;

UENUM(BlueprintType)
enum class ESFSelectableType : uint8
{
    None                UMETA(DisplayName = "None"),
    Commander           UMETA(DisplayName = "Commander"),
    MainBuilding        UMETA(DisplayName = "Main Building"),
    NormalBuilding      UMETA(DisplayName = "Normal Building"),
    ProductionBuilding  UMETA(DisplayName = "Production Building"),
    ResourceBuilding    UMETA(DisplayName = "Resource Building"),
    Resource            UMETA(DisplayName = "Resource"),
    Unit                UMETA(DisplayName = "Unit")
};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFSelectableInfo
{
    GENERATED_BODY()

    // 변수 영역
public:
    // 어떤 원본 행과 UI 행을 조회할지 결정하는 공통 키
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection"
    )
    FName DataRowName = NAME_None;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection"
    )
    ESFSelectableType SelectableType =
        ESFSelectableType::None;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection"
    )
    FText DisplayName;

    // HPComponent에서 가져오는 현재 런타임 체력
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection|Stat"
    )
    float CurrentHealth = 0.0f;

    // 액터 또는 스탯 컴포넌트에서 가져오는 현재 최대 체력
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection|Stat"
    )
    float MaximumHealth = 0.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection|Stat"
    )
    float AttackPower = 0.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection|Stat"
    )
    float MoveSpeed = 0.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
		Category = "SF|Selection|Stat"
    )
    int32 Tier = 0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection|UI"
    )
    TObjectPtr<UTexture2D> PortraitImage = nullptr;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection|UI"
    )
    TObjectPtr<UAnimationAsset> PortraitAnimation = nullptr;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection|UI"
    )
    TObjectPtr<UTexture2D> SelectionIcon = nullptr;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection|UI",
        meta = (
            MultiLine = true
            )
    )
    FText Description;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection|UI"
    )
    bool bShowHealth = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection|UI"
    )
    bool bShowAttackPower = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection|UI"
    )
    bool bShowMoveSpeed = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection|UI"
    )
    bool bShowSkillIcons = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Selection|UI"
    )
    TWeakObjectPtr<USFBuffComponent> BuffCompPtr;
};