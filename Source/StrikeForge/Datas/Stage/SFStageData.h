// SFStageData.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SFStageData.generated.h"

UENUM(BlueprintType)
enum class ESFStageProgressState : uint8
{
    Locked      UMETA(DisplayName = "Locked"),
    Unlocked    UMETA(DisplayName = "Unlocked"),
    Cleared     UMETA(DisplayName = "Cleared")
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

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Stage|Minimap",
        meta = (ClampMin = "0.0")
    )
    float BoundsPadding = 200.0f;
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