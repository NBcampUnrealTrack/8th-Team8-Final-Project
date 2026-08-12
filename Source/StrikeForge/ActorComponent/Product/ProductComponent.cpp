#include "ProductComponent.h"

#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Datas/Unit/UnitProductData.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Units/Unit/SFUnitSpawnLibrary.h"

UProductComponent::UProductComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}

void UProductComponent::AllClearProductLine()
{
	StopProductLine();
	ProductLine.Reset();
	OnProductLineChanged.Broadcast(ProductLine);
}

void UProductComponent::AddProductInLine(FName InProductName)
{
	UE_LOG(LogTemp, Warning, TEXT("UProductComponent::AddProductInLine: Called."));
	// if resource is enough
	if (ProductLine.Num() >= MaxLineCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("ProductLine is Full: %d / %d"), ProductLine.Num(), MaxLineCount);
		return;
	}
	
#pragma region CheckResources
	// //TODO: resource check
	// ASFGameState* SFGameState = Cast<ASFGameState>(GetWorld()->GetGameState());
	// if (!IsValid(SFGameState)) return;
	// if (!SFGameState->TrySpendCost(unitcost?.mineral, unitcost?.ether))
	// 	return;
#pragma endregion 
	
	USFDataTableManager* DataManager = GetOwner()->GetGameInstance()->GetSubsystem<USFDataTableManager>();
	if (!IsValid(DataManager)) return;
	
	const FUnitProductData* Row = DataManager->GetDataRow<FUnitProductData>(EDataTableType::Product, InProductName);;
	if (!Row) return;

	ProductLine.Add({InProductName, Row->UnitProductionDuration});
	OnProductLineChanged.Broadcast(ProductLine);
	UE_LOG(LogTemp, Warning, TEXT("Insert ProductLine: %s / Currentindex: %d"), *InProductName.ToString(), ProductLine.Num() - 1);
	
	if (ProductLine.Num() == 1)
	{
		StartProductLine();
	}
}

void UProductComponent::RemoveProductInLine(int32 InProductLineIndex)
{
	if (!ProductLine.IsValidIndex(InProductLineIndex)) return;

	const bool bWasProducing = (InProductLineIndex == 0);

	ProductLine.RemoveAt(InProductLineIndex); 
	OnProductLineChanged.Broadcast(ProductLine);

	if (!bWasProducing)
	{
		return; // index > 0 : index0 timer can not be deleted.
	}

	// only deleted index == 0
	GetOwner()->GetWorld()->GetTimerManager().ClearTimer(CurrentTimerHandle);

	if (ProductLine.Num() > 0)
	{
		StartProductLine(); // restart next element
	}
	else
	{
		StopProductLine();
	}
}

void UProductComponent::StartProductLine()
{
	// TODO: Population Check Logic

	// 목표량 = 초 단위 Duration, 초당 1씩 누적 → ProgressComponent와 동일한 계산 방식
	Tracker.Reset(ProductLine[0].Duration, 1.f);
	LastAdvanceTime = GetWorld()->GetTimeSeconds();
	OnProductRatioChanged.Broadcast(Tracker.GetRatio());
	
	GetWorld()->GetTimerManager().SetTimer(
		CurrentTimerHandle,
		this,
		&UProductComponent::TickProduction,
		TickInterval,
		true);
}

void UProductComponent::CompletedProduct(const FName& InCompletedName)
{
	// FName Spawn Logic //
	// 생성 절차(NavMesh 위치 탐색·팀 계승·능력치 주입)는 스포너와 동일하므로
	// USFUnitSpawnLibrary 로 공통화했다.
	USFUnitSpawnLibrary::SpawnUnitFromRow(GetOwner(), InCompletedName);
	/////////////////////
	UE_LOG(LogTemp, Warning, TEXT("CompletedProduct: %s"), *InCompletedName.ToString());

	// Pop() 은 마지막 항목을 제거한다.
	// 대기열은 0번부터 처리되므로 앞에서 빼야 순서가 맞는다.
	ProductLine.RemoveAt(0);
	OnProductLineChanged.Broadcast(ProductLine);
	if (ProductLine.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Product Restart: %d / %d"), ProductLine.Num(), MaxLineCount);
		StartProductLine();
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("Product Ended: %d / %d"), ProductLine.Num(), MaxLineCount);
	StopProductLine();
}

void UProductComponent::StopProductLine()
{
	GetWorld()->GetTimerManager().ClearTimer(CurrentTimerHandle);
	Tracker.Reset(0.f, 0.f);
	OnProductRatioChanged.Broadcast(Tracker.GetRatio());
}

void UProductComponent::BeginPlay()
{
	Super::BeginPlay();

}

void UProductComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopProductLine();
	Super::EndPlay(EndPlayReason);
}

void UProductComponent::TickProduction()
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float ActualDelta = Now - LastAdvanceTime;
	LastAdvanceTime = Now;
	UE_LOG(LogTemp, Warning, TEXT("Now: %f, Delta: %f"), Now, ActualDelta);
	
	//if (Tracker.Advance(TickInterval))
	if (Tracker.Advance(ActualDelta))
	{
		GetWorld()->GetTimerManager().ClearTimer(CurrentTimerHandle);
		CompletedProduct(ProductLine[0].RowName);
	}
	
	OnProductRatioChanged.Broadcast(Tracker.GetRatio());
}
