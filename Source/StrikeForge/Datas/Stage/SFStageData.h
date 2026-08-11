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
class UStaticMesh;

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

    // 스테이지 잠금 메쉬
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|Mesh")
    TSoftObjectPtr<UStaticMesh> LockedMesh;

    // 스테이지 풀림 메쉬
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|Mesh")
    TSoftObjectPtr<UStaticMesh> UnlockedMesh;

    // 스테이지 클리어 메쉬
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage|Mesh")
    TSoftObjectPtr<UStaticMesh> ClearedMesh;
};