// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/Progress/SFProgressTracker.h"
#include "Components/ActorComponent.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "MiningComponent.generated.h"

class AResourceBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSFOnResourceExtracted, EResourceType, ResourceType, int32, Amount);
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
	
	void StartMining();

	// void PauseMining();
	// void ResumeMining();
	void StopMining();
	
	
	bool TryLinkResource(AResourceBase* InResource);
	bool HasUsableLinkedResource() const; // 등록을 마치고 자원검사( 자원이 사라졋는지, 매말랐는지 )
	// ui?
	float GetIncomeRatePerSeconds() const;
	// setter
	FORCEINLINE void SetMineResourceType(EResourceType InResourceType) { MineResourceType = InResourceType; }
	// getter
	FORCEINLINE EResourceType GetMineResourceType() const { return MineResourceType; }
	FORCEINLINE int32 GetExtractAmount() const { return ExtractAmount; }
	// 자원이 연결되있는지 확인
	bool IsLinkedResource(const AResourceBase* InResource) const;
	
	void UnlinkResource();
	UFUNCTION()
	void HandleResourceDepleted(AResourceBase* Resource);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void TickMining();

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
	int32 ExtractAmount = 5;; // 작업이 완료되면 채굴할 양
	// UPROPERTY(VisibleInstanceOnly, Category = "Mining|Info")
	// float ExtractBaseWorkAmount = 10.f;   // 티어가 정하는 초당 작업량
	// UPROPERTY(VisibleInstanceOnly, Category = "Mining|Info")
	// float WorkSpeed = 1.f;
	// UPROPERTY(VisibleInstanceOnly, Category = "Mining|Info")
	// float ExtractInterval;
	// 작업총량, 수행당 작업증가량, 속도
	FTimerHandle MiningTimerHandle;
	TWeakObjectPtr<AResourceBase> LinkedResource;
};
