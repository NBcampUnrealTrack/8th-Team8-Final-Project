// SFStageData.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SFStageData.generated.h"

class UWorld;

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFStageData : public FTableRowBase
{
    GENERATED_BODY()

public:
    // 로비 UI에서 표시할 스테이지 이름
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage")
    FText DisplayName;

    // 스테이지 시작 시 이동할 레벨
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage")
    TSoftObjectPtr<UWorld> StageLevel;
};