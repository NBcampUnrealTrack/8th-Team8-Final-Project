#include "ProductComponent.h"

#include "Datas/Unit/UnitProductData.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Units/Unit/SFUnitSpawnLibrary.h"

UProductComponent::UProductComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}

void UProductComponent::InitProductCostMap()
{

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
	
	USFDataTableManager* DataManager = GetOwner()->GetGameInstance()->GetSubsystem<USFDataTableManager>();
	if (!DataManager) return;
	
	const FUnitProductData* Row = DataManager->GetDataRow<FUnitProductData>(EDataTableType::Product, InProductName);;
	if (!Row) return;

	ProductLine.Add({InProductName, Row->UnitProductionDuration});
	OnProductLineChanged.Broadcast();
	UE_LOG(LogTemp, Warning, TEXT("Insert ProductLine: %s / Currentindex: %d"), *InProductName.ToString(), ProductLine.Num() - 1);
	
	if (ProductLine.Num() == 1)
	{
		StartProductLine();
	}
}

void UProductComponent::RemoveProductInLine(int32 InProductLineIndex)
{
	// index = 0 : num = 1 / inindex = 1x inindex = 0
	if (ProductLine.Num() - 1 < InProductLineIndex)
	{
		return;
	}
	
	GetOwner()->GetWorld()->GetTimerManager().ClearTimer(CurrentTimerHandle);
	
	ProductLine.RemoveAt(InProductLineIndex);
	OnProductLineChanged.Broadcast();
	if (ProductLine.Num() == 0)
	{
		StopProductLine();
	}
}

void UProductComponent::StartProductLine()
{
	GetWorld()->GetTimerManager().SetTimer(
		CurrentTimerHandle,
		[this]()
		{
			// 참조로 넘기면 RemoveAt 이후 무효가 되므로 값을 복사한다
			FName CompletedName = ProductLine[0].RowName;
			CompletedProduct(CompletedName);
		},
		ProductLine[0].Duration,
		false);
}

void UProductComponent::CompletedProduct(FName& InCompletedName)
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
	OnProductLineChanged.Broadcast();
	if (ProductLine.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Product Restart: %d / %d"), ProductLine.Num(), MaxLineCount);
		StartProductLine();
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("Product Ended: %d / %d"), ProductLine.Num(), MaxLineCount);

}

void UProductComponent::StopProductLine()
{
}

const FRecordTimeInfo& UProductComponent::GetCurrentProductTimeInfo()
{
	CurrentProductTimeInfo.CurrentTime = GetWorld()->GetTimeSeconds();
	CurrentProductTimeInfo.Ratio =
		(CurrentProductTimeInfo.CurrentTime - CurrentProductTimeInfo.StartTime) / CurrentProductTimeInfo.Duration;
	return CurrentProductTimeInfo;
}

void UProductComponent::BeginPlay()
{
	Super::BeginPlay();
	
	InitProductCostMap();
}
