#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProductComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FProductLineChanged);

USTRUCT(BlueprintType)
struct FRecordTimeInfo
{
	GENERATED_BODY()
	float StartTime;
	float CurrentTime;
	float Duration;
	float Ratio;
};

USTRUCT(BlueprintType)
struct FProductInfo
{
	GENERATED_BODY()
	FName RowName;
	float Duration;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UProductComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProductComponent();
	void InitProductCostMap();
	
	UFUNCTION(BlueprintCallable)
	void AddProductInLine(FName InProductName);
	void RemoveProductInLine(int32 InProductLineIndex);
	
	void StartProductLine();
	void CompletedProduct(FName& InCompletedName);
	void StopProductLine();
	
	const FRecordTimeInfo& GetCurrentProductTimeInfo();
	
	FProductLineChanged OnProductLineChanged;

protected:
	virtual void BeginPlay() override;

private:
	TArray<FProductInfo> ProductLine;
	
	FRecordTimeInfo CurrentProductTimeInfo;
	
	FTimerHandle CurrentTimerHandle;
	
	int32 MaxLineCount = 5;
};
