#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActorComponent/Progress/SFProgressTracker.h"
#include "ActorComponent/Product/ProductInfo.h"
#include "ProductComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProductLineChanged, TArray<FProductInfo>, OutProductLineNames);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProductRatioChanged, float, OutProductRatio);

// USTRUCT(BlueprintType)
// struct FRecordTimeInfo
// {
// 	GENERATED_BODY()
// 	float StartTime;
// 	float CurrentTime;
// 	float Duration;
// 	float Ratio;
// };



UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UProductComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProductComponent();
	void AllClearProductLine();
	
	void AddProductInLine(FName InProductName);
	void RemoveProductInLine(int32 InProductLineIndex);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Product")
	bool IsProducing() const { return !ProductLine.IsEmpty(); }

	/** speed changer */
	UFUNCTION(BlueprintCallable, Category = "SF|Product")
	void SetSpeedMultiplier(float InMultiplier) { Tracker.SpeedMultiplier = InMultiplier; }
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Product")
	// current ratio
	FORCEINLINE float GetCurrentProductRatio() const { return Tracker.GetRatio(); }
	// current product line
	FORCEINLINE const TArray<FProductInfo>& GetProductLine() const { return ProductLine; }
	// current product value
	FORCEINLINE const FSFProgressTracker& GetProductValues() const { return Tracker; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void StartProductLine();
	void CompletedProduct(const FName& InCompletedName);
	void StopProductLine();
	void TickProduction();
private:
	TArray<FProductInfo> ProductLine;
	/** ProgressComponent와 동일한 계산기를 재사용한다 */
	UPROPERTY(BlueprintReadOnly, Category = "Product", meta = (AllowPrivateAccess = true))
	FSFProgressTracker Tracker;

	/** 진행률 재계산 주기(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Product", meta = (AllowPrivateAccess = true))
	float TickInterval = 1.f;
	
	FTimerHandle CurrentTimerHandle;
	float LastAdvanceTime = 0.f;
	
	int32 MaxLineCount = 5;
	
public:
	// product line changed
	UPROPERTY(BlueprintAssignable)
	FProductLineChanged OnProductLineChanged;
	// product ratio changed
	UPROPERTY(BlueprintAssignable)
	FProductRatioChanged OnProductRatioChanged;
};
