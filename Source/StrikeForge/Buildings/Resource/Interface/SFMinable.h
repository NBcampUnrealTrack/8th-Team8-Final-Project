#pragma once

#include "CoreMinimal.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "Datas/Building/MiningData/SFMiningCellTarget.h"
#include "UObject/Interface.h"
#include "SFMinable.generated.h"

class UMiningComponent;

UINTERFACE()
class UMinable : public UInterface
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FMinableData
{
	GENERATED_BODY()
	// 현재의 추출량, 남은자원량, 채굴진행도 비
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ExtractAmount;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ResourceLeftAmount;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GetMiningProgressRatio;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EResourceType ResourceType;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FResources UpgradeCost;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<UMiningComponent> MiningComponent;
	// << MiningComponent에 있는 알림 델리게이트 >>
	// 이번 타임에 자원을 캔 양			: OnResourceExtracted				-반환파라미터 (EResourceType ResourceType, int32 Amount)
	// 연결된 자원의 남은 양				: OnRemainingDepositChanged			-반환파라미터 (EResourceType ResourceType, int32 Remaining)
	// 채굴진행도 float 비율 0.0 ~ 1.0	: OnExtractProgressRatioChanged		-반환파라미터 (EResourceType ResourceType, float CurrentExtractRatio)
	// 고갈시							: OnResourceDepleted				-반환파라미터 없음
};

class STRIKEFORGE_API IMinable
{
	GENERATED_BODY()

public:
	// 광산이 취급하는 자원 종류
	virtual EResourceType GetMineResourceType() const = 0;
	
	// 광산건물의 자원 catch size
	virtual FIntPoint GetResourceSearchGridSize() const = 0;

	// 검색 영역에서 찾은 실제 채굴 셀 목록을 전달합니다.
	virtual bool SetMiningTargets(const TArray<FSFMiningCellTarget>& InTargets) = 0;

	// 건물이 등록 해제될 때 모든 채굴 대상을 비웁니다.
	virtual void ClearMiningTargets() = 0;
	
	// 광산 정보 Getter
	virtual const FMinableData GetMinableData() = 0;
};