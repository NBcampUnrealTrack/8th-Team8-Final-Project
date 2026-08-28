#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActorComponent/Progress/SFProgressTracker.h"
#include "ActorComponent/Product/ProductInfo.h"
#include "Datas/Unit/UnitStatData.h"
#include "ProductComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProductLineChanged, TArray<FProductInfo>, OutProductLineNames);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProductRatioChanged, float, OutProductRatio);

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

	/** speed changer */
	UFUNCTION(BlueprintCallable, Category = "SF|Product")
	void SetSpeedMultiplier(float InMultiplier) { Tracker.WorkRateMultiplier = InMultiplier; }
	// 집결지 setter
	FORCEINLINE void SetAssemblePoint(const FVector& InAssemblePoint) { AssemblePoint = InAssemblePoint; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Product")
	// current ratio
	FORCEINLINE float GetCurrentProductRatio() const { return Tracker.GetRatio(); }
	// current product line
	FORCEINLINE const TArray<FProductInfo>& GetProductLine() const { return ProductLine; }
	// current product value
	FORCEINLINE const FSFProgressTracker& GetProductValues() const { return Tracker; }
	// 집결지 getter - 나중에 vector가져와서 집결지랑 building위치벡터랑 연결해서 집결지표시용
	FORCEINLINE const FVector& GetAssemblePoint() const { return AssemblePoint; } 

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void StartProductLine();
	void CompletedProduct(const FName& InCompletedName);
	void ResetProductProgress();
	
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
public:
	// product line changed
	UPROPERTY(BlueprintAssignable)
	FProductLineChanged OnProductLineChanged;
	// product ratio changed
	UPROPERTY(BlueprintAssignable)
	FProductRatioChanged OnProductRatioChanged;
};
