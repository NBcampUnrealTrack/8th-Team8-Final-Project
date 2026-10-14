#pragma once

#include "CoreMinimal.h"
#include "SFMinimapMarkerInfo.generated.h"

class AActor;
class UBorder;

UENUM(BlueprintType)
enum class ESFMinimapMarkerType : uint8
{
    None        UMETA(DisplayName = "None"),
    Ally        UMETA(DisplayName = "Ally"),
    Enemy       UMETA(DisplayName = "Enemy"),
    Building    UMETA(DisplayName = "Building"),
    AllyBuilding  UMETA(DisplayName = "Ally Building"),
    EnemyBuilding UMETA(DisplayName = "Enemy Building"),
    Commander UMETA(DisplayName = "Commander")
};

USTRUCT()
struct FSFMinimapMarkerEntry
{
    GENERATED_BODY()

    TWeakObjectPtr<AActor> TargetActor;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> MarkerWidget = nullptr;

    ESFMinimapMarkerType MarkerType =
        ESFMinimapMarkerType::None;

    float DangerTimeRemaining = 0.0f;

    bool bWasTierActivated = false;

    int32 LastKnownTier = INDEX_NONE;
};