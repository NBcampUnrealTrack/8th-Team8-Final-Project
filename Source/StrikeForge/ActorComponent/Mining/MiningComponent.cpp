#include "MiningComponent.h"
#include "Buildings/Resource/ResourceBase.h"
#include "Logging/SFLog.h"

UMiningComponent::UMiningComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMiningComponent::InitMineProperties(float InBaseWorkAmount, int32 InExtractAmount)
{
	Tracker.BaseWorkAmount = FMath::Max(0.f, InBaseWorkAmount);
	ExtractAmount         = FMath::Max(1, InExtractAmount);
}

void UMiningComponent::StartMining()
{
	const AResourceBase* Resource = LinkedResource.Get();
	if (!IsValid(Resource) || Resource->IsDepleted()) return;
	
	FTimerManager& TimerManager =
		GetWorld()->GetTimerManager();
	// 두번활성화 금지
	if (TimerManager.IsTimerActive(MiningTimerHandle))
	{
		return;
	}
	
	// 타이머 시작시간 저장
	LastAdvanceTime = GetWorld()->GetTimeSeconds();
	OnExtractProgressRatioChanged.Broadcast(MineResourceType, Tracker.GetRatio());

	TimerManager.SetTimer(
		MiningTimerHandle,
		this,
		&ThisClass::TickMining,
		TickInterval,
		true);
}

void UMiningComponent::TickMining()
{
	AResourceBase* Resource = LinkedResource.Get();
	if (!IsValid(Resource))
	{
		StopMining();// 자원이 사라지면 타이머도 멈춘다
		OnResourceDepleted.Broadcast(); 
		return;
	}
	
	const float Now = GetWorld()->GetTimeSeconds();
	const float DeltaTime = FMath::Max(0.f, Now - LastAdvanceTime);
	LastAdvanceTime = Now;
	
	if (Tracker.Advance(DeltaTime))
	{
		Tracker.CurrentWork -= Tracker.RequiredWork;   // ★ current 초기화

		const int32 Extracted = Resource->Extract(ExtractAmount);

		if (Extracted > 0)
		{
			OnResourceExtracted.Broadcast(MineResourceType, Extracted);
		}
		// 고갈은 자원이 broadcast
	}
	
	if (GetWorld()->GetTimerManager().IsTimerActive(MiningTimerHandle))
	{
		OnExtractProgressRatioChanged.Broadcast(MineResourceType, Tracker.GetRatio());
	}
	//SF_LOG(Warning, "%s:: Ratio: %f", *UEnum::GetValueAsString(MineResourceType), Tracker.GetRatio());
}

void UMiningComponent::StopMining()
{
	GetWorld()->GetTimerManager().ClearTimer(MiningTimerHandle);
}

bool UMiningComponent::TryLinkResource(AResourceBase* InResource)
{
	if (!IsValid(InResource)) return false;
	if (InResource->GetResourceType() != MineResourceType) return false;
	if (InResource->IsDepleted()) return false;          // ★ 고갈된 광맥은 거부
	// 같은 자원이 이미 연결돼 있으면 구독을 다시 만들지 않는다.
	if (LinkedResource.Get() == InResource) return true;

	UnlinkResource();                                     // 기존 구독 해제

	LinkedResource = InResource;
	Tracker.RequiredWork = InResource->GetResourceRequiredWork();
	InResource->OnDepleted.AddUniqueDynamic(this, &ThisClass::HandleResourceDepleted);
	return true;
}

bool UMiningComponent::HasUsableLinkedResource() const
{
	const AResourceBase* Resource = LinkedResource.Get();

	return IsValid(Resource) &&
		!Resource->IsDepleted();
}

bool UMiningComponent::IsLinkedResource(const AResourceBase* InResource) const
{
	return LinkedResource.Get() == InResource;
}

void UMiningComponent::UnlinkResource()
{
	if (AResourceBase* Old = LinkedResource.Get())
	{
		Old->OnDepleted.RemoveDynamic(this, &ThisClass::HandleResourceDepleted);
	}
	LinkedResource = nullptr;
}
void UMiningComponent::HandleResourceDepleted(AResourceBase* Resource)
{
	StopMining();
	OnResourceDepleted.Broadcast();      // 건물 → 수입률 회수
}

float UMiningComponent::GetIncomeRatePerSeconds() const
{
	const AResourceBase* Resource = LinkedResource.Get();
	if (!IsValid(Resource) || Resource->IsDepleted()) return 0.f;

	const float RequiredWork  = Resource->GetResourceRequiredWork();
	const float WorkPerSecond = Tracker.BaseWorkAmount * Tracker.WorkRateMultiplier;

	if (RequiredWork <= KINDA_SMALL_NUMBER) return 0.f;   // 0 나눗셈 방어

	// 1회 채굴 시간 = RequiredWork / WorkPerSecond
	// 수입률       = ExtractAmount / 1회 채굴 시간
	return ExtractAmount * WorkPerSecond / RequiredWork;
}

void UMiningComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopMining();
	UnlinkResource();
	Super::EndPlay(EndPlayReason);
}

// void UMiningComponent::PauseMining()
// {
// 	FTimerManager& TM = GetWorld()->GetTimerManager();
// 	if (!TM.IsTimerActive(MiningTimerHandle)) return;
//
// 	TickMining();                      // 멈추기 직전까지의 작업을 반영
// 	TM.PauseTimer(MiningTimerHandle);  // CurrentWork 는 그대로 남는다
// 	OnExtractProgressRatioChanged.Broadcast(MineResourceType, 0.f);
// }
//
// void UMiningComponent::ResumeMining()
// {
// 	FTimerManager& TM = GetWorld()->GetTimerManager();
// 	if (!TM.IsTimerPaused(MiningTimerHandle)) return;
//
// 	LastAdvanceTime = GetWorld()->GetTimeSeconds();   // 멈춰 있던 시간을 델타에서 제외
// 	TM.UnPauseTimer(MiningTimerHandle);
// 	OnExtractProgressRatioChanged.Broadcast(MineResourceType, GetIncomeRatePerSeconds());
// }
