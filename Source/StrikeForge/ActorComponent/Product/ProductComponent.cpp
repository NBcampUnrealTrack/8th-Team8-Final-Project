#include "ProductComponent.h"

#include "Buildings/SFRootBase.h"
#include "Datas/Unit/UnitStatData.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Game/GameState/SFGameState.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Interfaces/Units/Targetable.h"
#include "Units/Unit/SFUnitSpawnLibrary.h"

UProductComponent::UProductComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}

void UProductComponent::AllClearProductLine()
{
	ProductLine.Reset();
	ResetProductProgress();
	OnProductLineChanged.Broadcast(ProductLine);
}

void UProductComponent::AddProductInLine(const FName& InProductName)
{
	UE_LOG(LogTemp, Warning, TEXT("UProductComponent::AddProductInLine: Called."));
	// if resource is enough
	if (ProductLine.Num() >= MaxLineCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("ProductLine is Full: %d / %d"), ProductLine.Num(), MaxLineCount);
		return;
	}
	
#pragma region CheckResources
	const FUnitStatData* UnitStatsRow = FindUnitRow(InProductName);
	if (!UnitStatsRow) return;
	ASFGameState* SFGS = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGS)) return;
	
	const int32 OwnerTeam = GetOwnerTeam();
	if (!SFGS->CanAffordResource(OwnerTeam, UnitStatsRow->UnitProductionCost))
	{
		SF_LOG(Warning, "[Product]자원이 부족해 [%s]을 생산하지못합니다. 현재유닛의 Cost: M: %d, E: %d",
			*InProductName.ToString(), UnitStatsRow->UnitProductionCost.Mineral, UnitStatsRow->UnitProductionCost.Ether);
		return;
	}
	
	if (!SFGS->CanAffordPopulation(OwnerTeam, UnitStatsRow->PopulationCost))
	{
		SF_LOG(Warning, "[Product] 수용인구가 부족해 [%s]을 생산하지못합니다. PopCost: %d, NowPop: %d/MaxPop: %d",
			*InProductName.ToString(),UnitStatsRow->PopulationCost, SFGS->GetCurrentPopulation(OwnerTeam) ,SFGS->GetMaximumPopulation(OwnerTeam));
		return;
	}
#pragma endregion 
	SFGS->TrySpendCost(OwnerTeam, UnitStatsRow->UnitProductionCost);
	ProductLine.Add({InProductName, UnitStatsRow->UnitProductionDuration});
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
	
	const FName RemovedRowName = ProductLine[InProductLineIndex].RowName;
	const bool bIsFrontProduct = (InProductLineIndex == 0);
	const int32 OwnerTeam = GetOwnerTeam();
	
	// 제거 + UI갱신 - 환불 성공 여부와 무관하게 일단 실행
	ProductLine.RemoveAt(InProductLineIndex); 
	OnProductLineChanged.Broadcast(ProductLine);
	
	// 환불시도
	if (const FUnitStatData* UnitRow = FindUnitRow(RemovedRowName))
	{
		if (ASFGameState* GS = GetWorld()->GetGameState<ASFGameState>())
		{
			// 환불
			GS->RefundCost(OwnerTeam, UnitRow->UnitProductionCost);
		}
	}
	
	if (!bIsFrontProduct)
	{
		return; // index > 0 : index0 timer can not be deleted.
	}
	// 맨앞열 유닛생산을 지우면 처음부터다시, 그외 유닛생산열을 지우면 첫번째 진행도는 유지
	// only deleted index == 0
	UnBindFunctionOnReleasePopulation();
	GetOwner()->GetWorld()->GetTimerManager().ClearTimer(CurrentTimerHandle);

	if (ProductLine.Num() > 0)
	{
		StartProductLine(); // restart next element
	}
	else
	{
		ResetProductProgress();
	}
}

void UProductComponent::PauseProductLine()
{
	if (bProductionPaused) return;

	// 대기열 존재 여부와 상관없이
	// 건물의 생산 기능 자체를 일시정지 상태로 만든다.
	bProductionPaused = true;

	if (ProductLine.IsEmpty()) return;
	
	if (bWaitingForPopulation) return;
	// 마지막 갱신부터 Pause 순간까지의 작업량 반영
	ProcessProductionToNow();

	// Advance 과정에서 현재 생산이 완료돼
	// 대기열이 비었을 수도 있다.
	if (!ProductLine.IsEmpty())
	{
		GetWorld()->GetTimerManager().PauseTimer(CurrentTimerHandle);
	}
}

void UProductComponent::ResumeProductLine()
{
	if (!bProductionPaused) return;
	bProductionPaused = false;

	if (ProductLine.IsEmpty()) return;
	
	if (bWaitingForPopulation)
	{
		const FName RowName = ProductLine[0].RowName;
		if (CanAffordPopulation(RowName))
		{
			UnBindFunctionOnReleasePopulation();
			SpawnAndAdvance(RowName);
		}
		return;
	}
	
	FTimerManager& TimerManager = GetWorld()->GetTimerManager();

	if (TimerManager.IsTimerPaused(CurrentTimerHandle))
	{
		LastAdvanceTime = GetWorld()->GetTimeSeconds();
		TimerManager.UnPauseTimer(CurrentTimerHandle);
	}
	else
	{
		StartProductLine();
	}
}

void UProductComponent::StartProductLine()
{
	GetWorld()->GetTimerManager().ClearTimer(CurrentTimerHandle);
	// 목표량 = 초 단위 Duration, 초당 1씩 누적 → ProgressComponent와 동일한 계산 방식
	Tracker.Reset(ProductLine[0].RequiredWork, 1.f); // 1씩오름
	LastAdvanceTime = GetWorld()->GetTimeSeconds();
	OnProductRatioChanged.Broadcast(Tracker.GetRatio());
	
	GetWorld()->GetTimerManager().SetTimer(
	   CurrentTimerHandle,
	   this,
	   &UProductComponent::TickProduction,
	   TickInterval,
	   true);

	
	if (bProductionPaused)
	{
		GetWorld()->GetTimerManager().PauseTimer(
			CurrentTimerHandle);
	}
}

void UProductComponent::CompletedProduct(const FName& InCompletedName)
{
	if (!CanAffordPopulation(InCompletedName))
	{
		BindFunctionOnMaxPopulation();
		SF_LOG(Warning, "[Product] 인구가 가득 차 대기 중입니다: %s", *InCompletedName.ToString());
		return;
	}

	SpawnAndAdvance(InCompletedName);
}

void UProductComponent::ResetProductProgress()
{
	UnBindFunctionOnReleasePopulation();
	
	GetWorld()->GetTimerManager().ClearTimer(CurrentTimerHandle);
	Tracker.Reset(0.f, 0.f);
	OnProductRatioChanged.Broadcast(Tracker.GetRatio());
}

void UProductComponent::ProcessProductionToNow()
{
	if (ProductLine.IsEmpty()) return;
	
	const float Now = GetWorld()->GetTimeSeconds();
	const float ActualDelta = FMath::Max(0.f, Now - LastAdvanceTime);
	LastAdvanceTime = Now;

	if (ActualDelta <= KINDA_SMALL_NUMBER) return;
	
	if (Tracker.Advance(ActualDelta))
	{
		GetWorld()->GetTimerManager().ClearTimer(CurrentTimerHandle);
		
		const FName CompletedProductName =	ProductLine[0].RowName;

		CompletedProduct(CompletedProductName);
		return;
	}

	OnProductRatioChanged.Broadcast(Tracker.GetRatio());
}

void UProductComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetProductProgress();
	Super::EndPlay(EndPlayReason);
}

void UProductComponent::TickProduction()
{
	ProcessProductionToNow();
}

void UProductComponent::HandlePopulationChanged(int32 NewCurrentPopulation, int32 NewMaximumPopulation)
{
	// 대기 중이 아니거나 큐가 비었으면 들을 이유가 없다.
	// 스폰이 유발한 재귀 통지도 여기서 걸러진다.
	if (!bWaitingForPopulation || ProductLine.IsEmpty())
	{
		UnBindFunctionOnReleasePopulation();
		return;
	}

	// 업그레이드로 정지된 상태면 재개하지 않는다.
	// 업그레이드가 끝나면 ResumeProductLine 이 이어받는다.
	if (bProductionPaused) return;

	const FName RowName = ProductLine[0].RowName;
	if (!CanAffordPopulation(RowName)) return;   // 아직도 부족하다

	UnBindFunctionOnReleasePopulation();
	SpawnAndAdvance(RowName);
}

// 조회함수
const FUnitStatData* UProductComponent::FindUnitRow(const FName& InRowName) const
{
	const USFDataTableManager* DataManager = GetOwner()->GetGameInstance()->GetSubsystem<USFDataTableManager>();
	return IsValid(DataManager)
		? DataManager->GetDataRow<FUnitStatData>(EDataTableType::Unit, InRowName)
		: nullptr;
}

bool UProductComponent::CanAffordPopulation(const FName& InRowName) const
{
	const FUnitStatData* Row = FindUnitRow(InRowName);
	const ASFGameState* GS = GetWorld()->GetGameState<ASFGameState>();
	const int32 OwnerTeam = GetOwnerTeam();
	
	// 데이터나 GameState 가 없으면 인구로 막지 않는다.
	if (!Row || !IsValid(GS)) return true;

	return GS->CanAffordPopulation(OwnerTeam, Row->PopulationCost);
}

int32 UProductComponent::GetOwnerTeam() const
{
	const ASFRootBase* OwnerBuilding = Cast<ASFRootBase>(GetOwner());
	return OwnerBuilding ? OwnerBuilding->GetTeamIndex() : Targetable::NeutralTeam;
}

void UProductComponent::SpawnAndAdvance(FName InRowName)
{
	// 큐 갱신을 스폰보다 먼저 한다.
	// SpawnUnitFromRow -> 유닛 BeginPlay -> ApplyPopulationDelta 가
	// OnPopulationChanged 를 다시 브로드캐스트하는데, 그 시점에 이 슬롯이
	// 이미 비어 있어야 같은 항목이 두 번 처리되지 않는다.
	// InRowName 을 값으로 받는 이유도 이것이다 — RemoveAt 이 원본을 파괴한다.
	ProductLine.RemoveAt(0);
	OnProductLineChanged.Broadcast(ProductLine);

	// 생성 절차(NavMesh 위치 탐색·팀 계승·능력치 주입)는 스포너와 동일하므로
	// USFUnitSpawnLibrary 로 공통화했다.
	USFUnitSpawnLibrary::SpawnUnitFromRow(GetOwner(), InRowName);
	UE_LOG(LogTemp, Warning, TEXT("CompletedProduct: %s"), *InRowName.ToString());

	if (ProductLine.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Product Restart: %d / %d"), ProductLine.Num(), MaxLineCount);
		StartProductLine();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Product Ended: %d / %d"), ProductLine.Num(), MaxLineCount);
		ResetProductProgress();
	}

	// 다음 생산까지 정리한 뒤에 알린다.
	//
	// 스폰 직후에 알리면, 구독자가 그 자리에서 AddProductInLine 을 부를 경우
	// 그쪽에서 StartProductLine 이 한 번 돌고 아래 분기에서 또 한 번 돌게 된다.
	// 이 위치에서는 큐 상태가 이미 확정돼 있어 그런 일이 없다.
	//
	// 조기 return 을 else 로 바꾼 이유가 이것이다. 위 두 분기의 동작은 그대로다.
	OnProductCompleted.Broadcast(InRowName);
}

// 바인딩
void UProductComponent::BindFunctionOnMaxPopulation()
{
	bWaitingForPopulation = true;

	UWorld* World = GetWorld();
	if (!World) return;

	if (ASFGameState* GS = World->GetGameState<ASFGameState>())
	{
		if (USFTeamState* Team = GS->GetTeamState(GetOwnerTeam()))
		{
			Team->OnPopulationChanged.AddUniqueDynamic(this, &UProductComponent::HandlePopulationChanged);
		}
	}
}

void UProductComponent::UnBindFunctionOnReleasePopulation()
{
	bWaitingForPopulation = false;

	UWorld* World = GetWorld();
	if (!World) return;

	if (ASFGameState* GS = World->GetGameState<ASFGameState>())
	{
		if (USFTeamState* Team = GS->GetTeamState(GetOwnerTeam()))
		{
			Team->OnPopulationChanged.RemoveDynamic(this, &UProductComponent::HandlePopulationChanged);
		}
	}
}
