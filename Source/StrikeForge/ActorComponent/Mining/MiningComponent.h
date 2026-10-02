// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "Datas/Building/MiningData/SFMiningCellTarget.h"
#include "ActorComponent/Progress/SFProgressTracker.h"
#include "Components/ActorComponent.h"
#include "MiningComponent.generated.h"

class AResourceBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSFOnResourceExtracted, EResourceType, ResourceType, int32, Amount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSFOnRemainingDepositChanged, EResourceType, ResourceType, int32, Remaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSFOnExtractProgressRatio, EResourceType, ResourceType, float, CurrentExtractRatio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFOnResourceDepleted);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UMiningComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMiningComponent();
	
	// delegate
	// 이번타임에 자원을 캔량
	UPROPERTY(BlueprintAssignable, Category = "Mining|Info|Delegate")
	FSFOnResourceExtracted OnResourceExtracted;
	// 연결된 자원의 남은 양.
	UPROPERTY(BlueprintAssignable, Category = "Mining|Info|Delegate")
	FSFOnRemainingDepositChanged OnRemainingDepositChanged;
	// ui progress
	// 채굴진행도
	UPROPERTY(BlueprintAssignable, Category = "Mining|Info|Delegate")
	FSFOnExtractProgressRatio OnExtractProgressRatioChanged;
	// 고갈시
	UPROPERTY(BlueprintAssignable, Category = "Mining|Info|Delegate")
	FSFOnResourceDepleted OnResourceDepleted;
	
	// RequiredWork
	// BaseWorkRate
	void InitMineProperties(float InBaseWorkAmount, int32 InExtractAmount);
	
	bool TryStartMining();

	// void PauseMining();
	// void ResumeMining();
	void StopMining();
	
	// 광산이 실제로 채굴할 셀 목록을 등록합니다.
	bool SetMiningTargets(const TArray<FSFMiningCellTarget>& InTargets);
	
	void ClearMiningTargets();
	/** 출처별 채굴 작업 배율을 추가하거나 같은 식별자의 기존 값을 교체합니다. */
	void SetWorkRateModifier(FName ModifierId, float InMultiplier);
	/** 지정한 출처의 배율을 제거하고 남은 배율들로 최종 작업속도를 다시 계산합니다. */
	void RemoveWorkRateModifier(FName ModifierId);
	/** 현재 자원 요구 작업량과 작업 배율을 기준으로 초당 예상 채굴량을 계산합니다. */
	float GetIncomeRatePerSeconds() const;
	FORCEINLINE void SetMineResourceType(EResourceType InResourceType) { MineResourceType = InResourceType; }
	FORCEINLINE EResourceType GetMineResourceType() const { return MineResourceType; }
	FORCEINLINE int32 GetExtractAmount() const { return ExtractAmount; }
	int32 GetRemainingDeposit() const;
	float GetExtractProgressRatio() const;
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void TickMining();

	// 실제 채굴 정책에 따라 대상 셀을 정렬합니다.
	void SortMiningTargetsByDeposit();
	
	// 사용할 수 없는 앞쪽 대상을 제거하고
	// MiningTargets[0]을 현재 대상으로 준비합니다.
	bool PrepareFrontTarget();

	// 현재 선택된 셀 정보입니다.
	const FSFMiningCellTarget* GetCurrentMiningTarget() const;

	// 현재 선택된 셀의 자원 액터입니다.
	// 셀이 고갈되었거나 액터가 사라졌으면 nullptr을 반환합니다.
	AResourceBase* GetCurrentResource() const;

	// 모든 채굴 대상이 고갈됐을 때 호출합니다.
	void FinishMiningTargets();
	
private:
	/** ProgressComponent와 동일한 계산기를 재사용한다 */
	UPROPERTY(BlueprintReadOnly, Category = "Mining", meta = (AllowPrivateAccess = true))
	FSFProgressTracker Tracker;

	float LastAdvanceTime = 0.f;
	/** 진행률 재계산 주기(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Mining", meta = (AllowPrivateAccess = true))
	float TickInterval = 0.1f;
	
	UPROPERTY(VisibleInstanceOnly, Category = "Mining|Info")
	EResourceType MineResourceType = EResourceType::Mineral;
	UPROPERTY(VisibleInstanceOnly, Category = "Mining|Info")
	int32 ExtractAmount = 5; // 작업이 완료되면 채굴할 양
	// UPROPERTY(VisibleInstanceOnly, Category = "Mining|Info")
	// float ExtractBaseWorkAmount = 10.f;   // 티어가 정하는 초당 작업량
	// UPROPERTY(VisibleInstanceOnly, Category = "Mining|Info")
	// float WorkSpeed = 1.f;
	// UPROPERTY(VisibleInstanceOnly, Category = "Mining|Info")
	// float ExtractInterval;
	// 작업총량, 수행당 작업증가량, 속도
	FTimerHandle MiningTimerHandle;
	
	// 채굴 가능한 셀을 매장량이 적은 순서대로 보관합니다.
	TArray<FSFMiningCellTarget> MiningTargets;
};
