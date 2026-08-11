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