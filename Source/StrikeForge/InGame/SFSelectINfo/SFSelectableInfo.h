#pragma once

#include "CoreMinimal.h"
#include "SFSelectableInfo.generated.h"

UENUM(BlueprintType)
enum class ESFSelectableType : uint8
{
    None                UMETA(DisplayName = "None"),
    Commander           UMETA(DisplayName = "Commander"),
    MainBuilding        UMETA(DisplayName = "Main Building"),
    ProductionBuilding  UMETA(DisplayName = "Production Building"),
    ResourceBuilding    UMETA(DisplayName = "Resource Building"),
    Unit                UMETA(DisplayName = "Unit")
};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFSelectableInfo
{
    GENERATED_BODY()

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

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection"
    )
    float CurrentHealth = 0.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection"
    )
    float MaximumHealth = 0.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "SF|Selection"
    )
    FName DataRowName = NAME_None;
};