#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActorComponent/Progress/SFProgressTracker.h"
#include "ActorComponent/Product/ProductInfo.h"
#include "Datas/Unit/UnitStatData.h"
#include "ProductComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProductLineChanged, TArray<FProductInfo>, OutProductLineNames);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProductRatioChanged, float, OutProductRatio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProductCompleted, FName, OutCompletedRowName);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UProductComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProductComponent();
	void AllClearProductLine();
	
	void AddProductInLine(const FName& InProductName);
	void RemoveProductInLine(int32 InProductLineIndex);
	void PauseProductLine();
	void ResumeProductLine();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Product")
	bool bIsHasProducts() const { return !ProductLine.IsEmpty(); }

	/**
	 * 출처별 생산 속도 배율을 등록한다.
	 * 서로 다른 출처의 배율은 곱해지므로 구역 보너스와 스킬 버프가 서로 덮어쓰지 않는다.
	 */
	void SetSpeedModifier(FName ModifierId, float InMultiplier);
	/** 지정한 출처의 배율을 제거하고 남은 배율들로 최종 생산속도를 다시 계산합니다. */
	void RemoveSpeedModifier(FName ModifierId);
	// 집결지 setter
	void SetAssemblePoint(const FVector& InAssemblePoint);
	void CancleAssemblePoint();
	
	// getter
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Product")
	// current ratio
	FORCEINLINE float GetCurrentProductRatio() const { return Tracker.GetRatio(); }
	// current product line
	FORCEINLINE const TArray<FProductInfo>& GetProductLine() const { return ProductLine; }
	// current product value
	FORCEINLINE const FSFProgressTracker& GetProductValues() const { return Tracker; }
	// 집결지 getter - 나중에 vector가져와서 집결지랑 building위치벡터랑 연결해서 집결지표시용
	FORCEINLINE const FVector& GetAssemblePoint() const { return AssemblePoint; } 

	// 집결지 설정 여부
	UFUNCTION(BlueprintPure, Category = "SF|Product|Rally")
	bool HasAssemblePoint() const
	{
		return bSetAssemblePoint;
	}

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void StartProductLine();
	void CompletedProduct(const FName& InCompletedName);
	void ResetProductProgress();
	
	/** 마지막 갱신 이후 경과 시간을 현재 배율로 생산 진행도에 반영합니다. */
	void ProcessProductionToNow();
	void TickProduction();
	
	void SpawnAndAdvance(FName InRowName);
	
	// getter
	int32 GetOwnerTeam() const;
	
	// 조회도우미 함수
	const FUnitStatData* FindUnitRow(const FName& InRowName) const;
	// 최대 인구 검증
	bool CanAffordPopulation(const FName& InRowName) const;
	
	// population function
	// bind
	void BindFunctionOnMaxPopulation();
	void UnBindFunctionOnReleasePopulation();
	// bind function
	UFUNCTION()
	void HandlePopulationChanged(int32 NewCurrentPopulation, int32 NewMaximumPopulation);
private:
	TArray<FProductInfo> ProductLine;
	/** ProgressComponent와 동일한 계산기를 재사용한다 */
	UPROPERTY(BlueprintReadOnly, Category = "Product", meta = (AllowPrivateAccess = true))
	FSFProgressTracker Tracker;

	/** 진행률 재계산 주기(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Product", meta = (AllowPrivateAccess = true))
	float TickInterval = 0.1f;
	
	FTimerHandle CurrentTimerHandle;
	float LastAdvanceTime = 0.f;
	
	int32 MaxLineCount = 5;
	
	bool bProductionPaused = false;
	bool bWaitingForPopulation = false;
	
	// 컨트롤러에서 값을 받으면 building이 여기로 저장
	FVector AssemblePoint;
	bool bSetAssemblePoint = false;
public:
	// product line changed
	UPROPERTY(BlueprintAssignable)
	FProductLineChanged OnProductLineChanged;
	// product ratio changed
	UPROPERTY(BlueprintAssignable)
	FProductRatioChanged OnProductRatioChanged;
	/**
	 * 유닛 한 기가 실제로 생성된 직후.
	 *
	 * OnProductLineChanged 와 구분된다. 저쪽은 큐에 넣을 때·취소할 때·완료될 때가
	 * 모두 같은 신호로 오지만, 이쪽은 유닛이 월드에 나온 경우에만 온다.
	 *
	 * 인구가 가득 차 대기 중이면 CompletedProduct 가 스폰까지 가지 않으므로
	 * 이 신호도 오지 않는다 — 인구가 풀려 실제로 생성될 때 한 번 온다.
	 */
	UPROPERTY(BlueprintAssignable)
	FProductCompleted OnProductCompleted;
};
