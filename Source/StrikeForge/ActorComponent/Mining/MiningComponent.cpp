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

bool UMiningComponent::TryStartMining()
{
	UWorld* World = GetWorld();

	if (!World || !PrepareFrontTarget())
	{
		return false;
	}

	FTimerManager& TimerManager = World->GetTimerManager();

	// 이미 채굴 중이라면 정상적인 채굴 상태입니다.
	if (TimerManager.IsTimerActive(MiningTimerHandle))
	{
		return true;
	}

	LastAdvanceTime = World->GetTimeSeconds();

	OnExtractProgressRatioChanged.Broadcast(
		MineResourceType, Tracker.GetRatio());

	TimerManager.SetTimer(
		MiningTimerHandle,
		this,
		&ThisClass::TickMining,
		TickInterval,
		true);

	return true;
}

void UMiningComponent::TickMining()
{
	if (!PrepareFrontTarget())
	{
		FinishMiningTargets();
		return;
	}

	const float Now =
		GetWorld()->GetTimeSeconds();

	const float DeltaTime =
		FMath::Max(0.f,	Now - LastAdvanceTime);

	LastAdvanceTime = Now;

	if (Tracker.Advance(DeltaTime))
	{
		Tracker.CurrentWork -= Tracker.RequiredWork;

		int32 Extracted = 0;

		/*
		 * 채굴량을 채울 때까지 앞쪽 셀부터 순회합니다.
		 * 고갈된 셀은 PrepareFrontTarget()이 배열에서 제거하고,
		 * 배열이 비면 순회를 멈추고 지금까지 누적된 양만 지급합니다.
		 */
		while (Extracted < ExtractAmount && PrepareFrontTarget())
		{
			const FSFMiningCellTarget& Target = MiningTargets[0];

			const int32 ExtractedFromCell =
				Target.ResourceActor->ExtractFromCell(
					Target.ResourceCellIndex, ExtractAmount - Extracted);

			if (ExtractedFromCell <= 0)
			{
				break;
			}

			Extracted += ExtractedFromCell;
		}

		if (Extracted > 0)
		{
			OnResourceExtracted.Broadcast(
				MineResourceType, Extracted);

			OnRemainingDepositChanged.Broadcast(
				MineResourceType, GetRemainingDeposit());
		}

		// 누적량을 지급한 뒤 배열이 비었다면 고갈을 알립니다.
		if (!PrepareFrontTarget())
		{
			FinishMiningTargets();
			return;
		}
	}

	if (GetWorld()->GetTimerManager().IsTimerActive(MiningTimerHandle))
	{
		OnExtractProgressRatioChanged.Broadcast(
			MineResourceType, Tracker.GetRatio());
	}
}

void UMiningComponent::SortMiningTargetsByDeposit()
{
	MiningTargets.Sort(
		[](const FSFMiningCellTarget& Left,
		   const FSFMiningCellTarget& Right)
		{
			const AResourceBase* LeftResource =
				Left.ResourceActor.Get();

			const AResourceBase* RightResource =
				Right.ResourceActor.Get();

			const bool bLeftValid =
				IsValid(LeftResource);

			const bool bRightValid =
				IsValid(RightResource);

			if (bLeftValid != bRightValid)
			{
				return bLeftValid;
			}

			if (!bLeftValid)
			{
				return false;
			}

			return
				LeftResource->GetCellDeposit(
					Left.ResourceCellIndex)
				<
				RightResource->GetCellDeposit(
					Right.ResourceCellIndex);
		});
}

bool UMiningComponent::PrepareFrontTarget()
{
	bool bRemovedTarget = false;

	while (!MiningTargets.IsEmpty())
	{
		const FSFMiningCellTarget& Target =
			MiningTargets[0];

		AResourceBase* Resource =
			Target.ResourceActor.Get();

		const bool bUsable =
			IsValid(Resource) &&
			Resource->GetResourceType() ==
				MineResourceType &&
			Resource->GetCellDeposit(
				Target.ResourceCellIndex) > 0;

		if (bUsable)
		{
			if (bRemovedTarget)
			{
				Tracker.CurrentWork = 0.f;
			}

			Tracker.RequiredWork =
				FMath::Max(
					KINDA_SMALL_NUMBER,
					Resource
						->GetResourceRequiredWork());

			return true;
		}

		/*
		 * 앞쪽 대상이 고갈됐거나 자원 액터가 사라졌다면
		 * 배열에서 제거합니다.
		 */
		MiningTargets.RemoveAt(0);
		bRemovedTarget = true;
	}

	if (bRemovedTarget)
	{
		Tracker.CurrentWork = 0.f;
	}

	return false;
}

void UMiningComponent::StopMining()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MiningTimerHandle);
	}
}

bool UMiningComponent::SetMiningTargets(
	const TArray<FSFMiningCellTarget>& InTargets)
{
	ClearMiningTargets();

	for (const FSFMiningCellTarget& Target : InTargets)
	{
		AResourceBase* Resource = Target.ResourceActor.Get();

		if (!IsValid(Resource))	continue;
		
		if (Resource->GetResourceType() != MineResourceType)
		{
			continue;
		}

		if (Resource->GetCellDeposit(Target.ResourceCellIndex) <= 0)
		{
			continue;
		}

		const bool bAlreadyAdded =
			MiningTargets.ContainsByPredicate(
				[&Target](
					const FSFMiningCellTarget& ExistingTarget)
				{
					return
						ExistingTarget.ResourceActor ==
							Target.ResourceActor &&
						ExistingTarget.ResourceCellIndex ==
							Target.ResourceCellIndex;
				});

		if (!bAlreadyAdded)
		{
			MiningTargets.Add(Target);
		}
	}
	
	// 유효성과 중복 검사를 통과한 대상만 정렬합니다.
	SortMiningTargetsByDeposit();

	return PrepareFrontTarget();
}

int32 UMiningComponent::GetRemainingDeposit() const
{
	int32 TotalRemainingDeposit = 0;

	for (const FSFMiningCellTarget& Target : MiningTargets)
	{
		const AResourceBase* Resource =	Target.ResourceActor.Get();

		if (!IsValid(Resource)) continue;

		TotalRemainingDeposit +=
			Resource->GetCellDeposit(Target.ResourceCellIndex);
	}

	return TotalRemainingDeposit;
}

float UMiningComponent::GetExtractProgressRatio() const
{
	return Tracker.GetRatio();
}

void UMiningComponent::ClearMiningTargets()
{
	StopMining();

	MiningTargets.Reset();
	Tracker.CurrentWork = 0.f;
}

void UMiningComponent::SetWorkRateModifier(FName ModifierId, float InMultiplier)
{
	// InMultiplier가 0아닌지 검사하고 ModifierId가 None이 아닌지 검사함
	if (!Tracker.WouldChangeWorkRateModifier(ModifierId, InMultiplier))
	{
		return;
	}

	// 배율 변경 전까지 흐른 시간은 이전 배율로 먼저 반영한다.
	if (UWorld* World = GetWorld();
		World && World->GetTimerManager().IsTimerActive(MiningTimerHandle))
	{
		TickMining();
	}

	Tracker.SetWorkRateModifier(ModifierId, InMultiplier);
	
	SF_LOG(Display, "[MiningComponent::SetWorkRateModifier] 광산의기본작업배율증가!: Causer: %s, Amount: %.1f",
		*ModifierId.ToString(), InMultiplier);
}

void UMiningComponent::RemoveWorkRateModifier(FName ModifierId)
{
	if (!Tracker.HasWorkRateModifier(ModifierId))
	{
		return;
	}

	// 배율 변경 전까지 흐른 시간은 이전 배율로 먼저 반영한다.
	if (UWorld* World = GetWorld();
		World && World->GetTimerManager().IsTimerActive(MiningTimerHandle))
	{
		TickMining();
	}

	Tracker.RemoveWorkRateModifier(ModifierId);
}

const FSFMiningCellTarget*
UMiningComponent::GetCurrentMiningTarget() const
{
	if (MiningTargets.IsEmpty())
	{
		return nullptr;
	}

	return &MiningTargets[0];
}

AResourceBase*
UMiningComponent::GetCurrentResource() const
{
	const FSFMiningCellTarget* Target =
		GetCurrentMiningTarget();

	if (!Target)
	{
		return nullptr;
	}

	AResourceBase* Resource =
		Target->ResourceActor.Get();

	if (!IsValid(Resource))
	{
		return nullptr;
	}

	if (Resource->GetResourceType() != MineResourceType)
	{
		return nullptr;
	}

	if (Resource->GetCellDeposit(
		Target->ResourceCellIndex) <= 0)
	{
		return nullptr;
	}

	return Resource;
}

void UMiningComponent::FinishMiningTargets()
{
	StopMining();

	MiningTargets.Reset();
	Tracker.CurrentWork = 0.f;

	OnExtractProgressRatioChanged.Broadcast(
		MineResourceType,0.f);

	OnResourceDepleted.Broadcast();
}

float UMiningComponent::GetIncomeRatePerSeconds() const
{
	const AResourceBase* Resource = GetCurrentResource();
	if (!IsValid(Resource)) return 0.f;

	const float RequiredWork =
		Resource->GetResourceRequiredWork();

	const float WorkPerSecond =
		Tracker.BaseWorkAmount * Tracker.WorkRateMultiplier;

	if (RequiredWork <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	return ExtractAmount *
		WorkPerSecond /	RequiredWork;
}

void UMiningComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearMiningTargets();
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
